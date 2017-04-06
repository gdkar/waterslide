/*-----------------------------------------------------------------------------
 * file:        proc_vectormatchre2.c
 *          most portions taken from proc_vectormatch.c
 * date:        3-2-2011
 * description: <see proc_purpose[] below>
 *
 *-----------------------------------------------------------------------------
 * History
 *
 * 3-02-2010        Creation.
 * 5-15-2013        Updated documentation
 *---------------------------------------------------------------------------*/

#define PROC_NAME "vectormatchnpu"
//#define DEBUG 1

#include <cstdlib>
#include <atomic>
#include <thread>
#include <mutex>
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <cctype>
#include <cmath>     //sqrt
#include <numeric>   //sqrt
#include <algorithm>   //sqrt
#include <memory>
#include <deque>
#include <vector>
#include <array>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>   //sqrt
#include <functional>   //sqrt
#include <cerrno>  //errno
#include <funny-car/funny-car-c.h>
#include "waterslide.h"
#include "datatypes/wsdt_fixedstring.h"
#include "datatypes/wsdt_tuple.h"
#include "datatypes/wsdt_string.h"
#include "datatypes/wsdt_binary.h"
#include "datatypes/wsdt_vector_double.h"
#include "waterslidedata.h"
#include "procloader.h"
#include "label_match.h"
#include "sysutil.h"  //sysutil_config_fopen

/*-----------------------------------------------------------------------------
 *                  D E F I N E S
 *---------------------------------------------------------------------------*/
#define LOCAL_MAX_TYPES 25
#define LOCAL_VECTOR_SIZE 2000

using namespace funny_car;
/*-----------------------------------------------------------------------------
 *           D L S Y M   I N T E R F A C E
 *---------------------------------------------------------------------------*/



extern "C" {
int proc_init(
    wskid_t * kid
  , int argc
  , char ** argv
  , void ** vinstance
  , ws_sourcev_t * sv
  , void * type_table
    );

proc_process_t proc_input_set(
    void * vinstance
  , wsdatatype_t * input_type
  , wslabel_t * port
  , ws_outlist_t* olist
  , int type_index
  , void * type_table
    );
int proc_destroy(
    void * vinstance
    );
}
/*-----------------------------------------------------------------------------
 *                  G L O B A L S
 *---------------------------------------------------------------------------*/

extern "C" const char proc_name[]        = PROC_NAME;
extern "C" const char proc_version[]     = "0.1.1-rc.1";
extern "C" const char *const proc_alias[]  = { "vectornpu", "vnpu", "npu2", NULL };

#if defined (MP_DOCS) || true
extern "C" const char *const proc_tags[]     = { "match", "vector", "npu", "LRL", NULL };
extern "C" const char proc_purpose[]     = "matches a list of regular expressions and returns...... something";
extern "C" const char *const proc_synopsis[] = {
    "vectormatchnpu [-V <label>] -F <file> [-L <label>] [-W] <label of string member to match>"
  , nullptr
    };
extern "C" const char proc_description[] =
    "vectormatchnpu extends vectormatch to perform regular "
    "expression matching.\n"
    "vectormatchnpu uses LRL's NPU coprocessor. The format of the "
    "regular expressions are defined in re2/re2.h.\n"
    "\n"
    "\"select\" (SQL_SELECT)\n"
    "\"union\"      (SQL_UNION)\n"
    "\n";

extern "C" const proc_example_t proc_examples[] = {
    {"...| vectormatchnpu -F \"re.txt\" -M MY_STRING |...\n",
    "apply the labels of the matched regular expressions in "
    "re.txt to the MY_STRING field."},
    {"...| vectormatchnpu -F \"re.txt\" -L NPU_MATCH MY_STRING |...\n",
    "apply the label NPU_MATCH to the MY_STRING field if there was a match."},
    {"...| TAG:vectormatchnpu -F \"re.txt\" -L NPU_MATCH MY_STRING |...\n",
    "pass all tuples, but tag the ones that had a match"},
    {"...| vectormatchnpu -F \"re.txt\" |...\n",
    "match against all strings in the tuple"},
    {NULL,""}
};

extern "C" const proc_option_t proc_opts[] = {
    /*  'option character', "long option string", "option argument",
    "option description", <allow multiple>, <required>*/
    {'F',"","file",
    "file with items to search",0,0},
    {'M',"","",
    "affix keyword label to matching tuple member",0,0},
    {'L',"","",
    "common label to affix to matched tuple member",0,0},
    {'D',"","device",
    "device file to use.",0,0},
    {'E',"","expression",
    "provide a pcre expression to match against..",1,0},
    {'B',"","binary",
    "provide a filename of a npu binary to match with.",1,0},
    {'m',"","matches",
    "configure the number of matches to return per status.",0,0},
    {'P',"","matches",
    "pass all packets",0,0},

    /*
     {'T',"","",
     "tag flows that match (npacket only)",0,0},
     {'R',"","string",
     "string to match",0,0},
     {'Q',"","",
     "match first pkt only",0,0},
    */
    //the following must be left as-is to signify the end of the array
    {' ',"","",
    "",0,0}
};
extern "C" const char proc_nonswitch_opts[]               = "LABEL of string member to match";
extern "C" const char *const proc_input_types[]           = {"tuple","flush", nullptr};  //removed "npacket"
extern "C" const char *const proc_output_types[]          = {"tuple", nullptr}; //removed "npacket"
extern "C" const char *const proc_tuple_member_labels[]    = {nullptr};
extern "C" const char proc_requires[]              = "";
extern "C" const char *const proc_tuple_container_labels[] = {nullptr};
extern "C" const char *const proc_tuple_conditional_container_labels[] = {nullptr};

extern "C" const proc_port_t proc_input_ports[] = {
    {"none","pass if match"},
    {"TAG","pass all, label tuple if match"},
    {nullptr, nullptr}
};
#endif //MP_DOCS

/*-----------------------------------------------------------------------------
 *           D A T A S T R U C T   D E F S
 *---------------------------------------------------------------------------*/

struct flow_data_t {
    wslabel_t * label;
};

struct callback_data {
    wsdata_t         *input_data{nullptr};
    wsdata_t         *member_data{nullptr};
    int               nmatches{0};
    static constexpr const int capacity = 16ul;
    int               matches[capacity];
    bool              is_last{false};
    bool              hw_overflow{false};
    bool              qd_overflow{false};
    std::atomic<bool> is_done{false};
};

struct callback_batch {
    ptrdiff_t       rptr{0};
    ptrdiff_t       wptr{0};
    static constexpr const size_t capacity = 63ul;
    callback_data   data[capacity];

    callback_data&front() { return data[rptr];}
    callback_data&back()  { return data[wptr-1];}
    void emplace_back()   { wptr++; }
    void pop_back()   { --wptr; }
    void pop_front()      { if(front().is_done.load()) rptr++; }
    size_t size() const   { return wptr - rptr;}
    bool   empty() const  { return wptr == rptr;}
    bool   full() const   { return wptr == capacity;}
};

struct vector_element
{
    std::string pattern;
    std::string binfile;
    wslabel_t *label{};      /* the label associated with the vector element */
    size_t matchlen{};
    int    pid{-1};
    double count{};    /* freq. of occurrence of the element */
};
template<class It>
struct iter_range : std::tuple<It,It> {
    using traits_type = typename std::iterator_traits<It>;
    using iterator    = It;
    using super       = std::tuple<iterator,iterator>;
    using super::super;
/*    std::tuple<It,It> m_d{};
    usi
    iter_range() = default;
    iter_range(iter_range&&) noexcept = default;
    iter_range(const iter_range&) = default;
    iter_range&operator=(iter_range &&) noexcept = default;
    iter_range&operator=(const iter_range &) = default;
    iter_range(std::tuple<It,It> && args):m_d{args}{}
    template<class... Args>
    iter_range(Args && ...args)
    : m_d(std::forward<Args>(args)...)
    {}*/

    iterator begin() const { return std::get<0>(*this);}
    iterator end()   const { return std::get<1>(*this);}
};

template<class Tup, class It = typename std::tuple_element<0,Tup>::type>
iter_range<It> make_iter_range( Tup && tup)
{
    return { std::forward<Tup >(tup) };
}
/*-----------------------------------------------------------------------------
 *              P R O C _ I N S T A N C E
 *---------------------------------------------------------------------------*/
struct vectormatch_proc {
    uint64_t meta_process_cnt{};
    uint64_t hits{};
    uint64_t outcnt{};
    uint64_t hw_overflow{};
    uint64_t qd_overflow{};
    int      max_status{};
    ws_outtype_t *  outtype_tuple{};
    wslabel_t * pattern_id_label{};   /* label affixed to the buffer that matches*/
    wslabel_t * matched_label{};   /* label affixed to the buffer that matches*/
    wslabel_t * vector_name{};      /* label to be affixed to the matched vector */
    wslabel_set_t    lset{};        /* set of labels to search over */

    int do_tag[LOCAL_MAX_TYPES];

    std::vector<vector_element> term_vector{};
    std::multimap<int, vector_element&> term_map{};

    std::deque<
        std::unique_ptr<
            callback_batch
            >
        > cb_queue;
    bool                              got_match{false};
    npu_driver                       *driver{};
    npu_client                       *client{};
    int  verbosity{};
    int  status_size{1};
    bool label_members{}; /* 1 if we should labels matched members*/
    bool pass_all{false}; /* 1 if we should labels matched members*/
    bool thread_running{};
    std::string device_name = "/dev/lrl_npu0";
   ~vectormatch_proc();
    void reset_counts();
    int loadfile(void *type_table, const char *filename);

    int cmd_options(
       int argc
     , char *argv[]
     , void *type_table
       );
    int add_element(void * type_table,
                const char *restr, const char *binstr, size_t matchlen,
                const char *labelstr);

    static int proc_process_meta(void *, wsdata_t*, ws_doutput_t*, int);
    static int proc_process_allstr(void *, wsdata_t*, ws_doutput_t*, int);
    static int proc_process_flush(void *, wsdata_t*, ws_doutput_t*, int);
    int process_meta(wsdata_t*, ws_doutput_t*, int);
    int process_allstr(wsdata_t*, ws_doutput_t*, int);
    int process_flush(wsdata_t*, ws_doutput_t*, int);

    proc_process_t input_set(
        wsdatatype_t * input_type
       , wslabel_t * port
       , ws_outlist_t* olist
       , int type_index
       , void * type_table
        );
};
vectormatch_proc::~vectormatch_proc()
{
    npu_client_flush(client);
    npu_client_free(&client);
    npu_thread_stop(driver);
    npu_driver_close(&driver);
    while(!cb_queue.empty()) {
        if(cb_queue.front()->empty())
            cb_queue.pop_front();
        else {
            auto &qd = cb_queue.front()->front();
            if(!qd.is_done.load())
                continue;
            if(qd.nmatches && !got_match) {
                hits++;
                got_match = true;
            }
            if(qd.hw_overflow)
                hw_overflow++;
            if(qd.qd_overflow)
                qd_overflow++;

            max_status = std::max<int>(max_status,qd.nmatches);
            for(auto i = 0; i < qd.nmatches; ++i) {
                auto mval = qd.matches[i];
                auto eq_range = make_iter_range(term_map.equal_range(mval));
                for(auto & term : eq_range) {
                    term.second.count++;
                    //i.e., is there a tuple member we are going to add to?
                    if (qd.member_data && label_members) {
                        /*get the label associated with the number returned by Aho-Corasick;
                        * default to label_match if one is not found. */
                        auto mlabel = term.second.label;
                        if (mlabel && !wsdata_check_label(qd.member_data, mlabel)) {
                            /* this allows labels to be indexed */
                            tuple_add_member_label(
                                    qd.input_data, /* the tuple itself */
                                    qd.member_data,    /* the tuple member to be added to */
                                    mlabel /* the label to be added */);
                        }
                        tuple_member_create_int(
                            qd.input_data,
                            term.second.pid,
                            pattern_id_label);
                    }
                }
            }
            if(qd.is_last) {
                reset_counts();
                if (got_match && matched_label) { /* this is the -L option label */
                    wsdata_add_label(
                        qd.input_data,
                        matched_label);
                    tuple_add_member_label(qd.input_data, /* the tuple itself */
                            qd.input_data,    /* the tuple member to be added to */
                            matched_label/* the label to be added */);
                }
                wsdata_delete(qd.input_data);
                got_match = false;

            }
            cb_queue.front()->pop_front();
        }
    }
    tool_print("meta_proc cnt %" PRIu64, meta_process_cnt);
    tool_print("matched tuples cnt %" PRIu64, hits);
    tool_print("output cnt %" PRIu64, outcnt);
    tool_print("hardware overflow cnt %" PRIu64, hw_overflow);
    tool_print("queue overflow cnt %" PRIu64, qd_overflow);
    tool_print("max status cnt %d" , max_status);

}

static int vectormatch_npu_log_cb(void *, int level, const char *fmt, va_list args)
{
    auto ilevel = int(level);
    char msg[4096] = { 0, };
    vsnprintf(msg,sizeof(msg),fmt, args);
    if(ilevel >= int(NPU_ERROR)) {
        error_print("%s",msg);
    }else if(ilevel >= int(NPU_VERBOSE)) {
        tool_print("%s",msg);
    }else{
        dprint("%s",msg);
    }
    return 0;
}
const proc_labeloffset_t proc_labeloffset[] = {
//    {"MATCH",offsetof(proc_instance_t, umatched_label)},
//    {"VECTOR",offsetof(proc_instance_t,vector_name)},
    {"PATTERN_ID",offsetof(vectormatch_proc, pattern_id_label)}
};

/*-----------------------------------------------------------------------------
 * proc_cmd_options
 *  parse out and process the command line options of the kid.
 *
 * [out] rval - 1 if ok, 0 if error.
 *---------------------------------------------------------------------------*/
int vectormatch_proc::cmd_options(
    int argc
  , char * argv[]
  , void * type_table
    )
{
    int op;
    int F_opt = 0;
    pattern_id_label = wsregister_label(type_table,"PATTERN_ID");

    while ((op = getopt(argc, argv, "Pm:B:E:D:v::F:L:M")) != EOF) {
        switch (op) {
          case 'v':
                if(optarg && *optarg == 'v') {
                    while(*optarg++ == 'v') {
                        ++verbosity;
                    }
                }else if(optarg) {
                    verbosity = atoi(optarg);
                }else{
                    ++verbosity;
                }
               break;

            case 'M':{  /* label tuple data members that match */
                label_members = true;
                break;
            }
            case 'm':{  /* label tuple data members that match */
                std::istringstream{optarg} >> status_size;
                break;
            }
            case 'E':{  /* label tuple data members that match */
                if (!add_element(type_table, optarg, nullptr, strlen(optarg)+1,nullptr)) {
                    tool_print("problem adding expression %s\n", optarg);
                }
                break;
            }
            case 'P':{  /* label tuple data members that match */
                pass_all = true;
                break;
            }

            case 'B':{  /* label tuple data members that match */
                if (!add_element(type_table, nullptr,optarg, 0,nullptr)) {
                    tool_print("problem adding expression %s\n", optarg);
                }
                break;
            }
            case 'D':{  /* label tuple data members that match */
                device_name = optarg;;
                break;
            }
            case 'L':{  /* labels an entire tuple and  matching members. */
                tool_print("Registering label '%s' for matching buffers.", optarg);
                matched_label = wsregister_label(type_table, optarg);
                break;
            }
            case 'F':{ /* load up the keyword file */
                if (loadfile(type_table, optarg)) {
                    tool_print("reading input file %s\n", optarg);
                } else {
                    tool_print("problem reading file %s\n", optarg);
                    return 0;
                }
                F_opt++;
                break;
            }
            /* These are present in proc_match.  We may want to include them here
            * at some point */

            /*
            case 'T':
            proc->tag_flows = 1;
            break;

            case 'Q':
            proc->fpkt_only = 1;
            proc->label_fpkt = wsregister_label(type_table, "FIRSTPKT");
            break;
            */

            default: {
                tool_print("Unknown command option supplied");
                exit(-1);
            }
        }
    }

    if (term_vector.empty()) {
        tool_print("ERROR: -F <filename> option is required.");
        exit(-1);
    }
    while (optind < argc) {
        wslabel_set_add(type_table, &lset, argv[optind]);
        dprint("searching for string with label %s",argv[optind]);
        optind++;
    }
    driver = npu_driver_alloc();
    if(!driver)
        return 0;
    npu_log_set_level(driver,(NPULogLevel)((int)NPU_INFO - verbosity));
    npu_log_set_handler(driver,(void*)this, &vectormatch_npu_log_cb);
    if(npu_driver_open(&driver,device_name.c_str()) < 0) {
          error_print("failed to open NPU hardware device");
          return 0;
     }
    npu_log_set_level(driver,(NPULogLevel)((int)NPU_WARN));
    
    for(auto & term : term_vector) {
        if(!term.binfile.empty()) {
            auto pattern_id = npu_pattern_insert_binary_file(
                driver
              , term.binfile.c_str()
                );
            if(pattern_id < 0) {
                error_print("failed to open to insert pattern '%s', file '%s'", term.pattern.c_str(), term.binfile.c_str());
                continue;
            }
            term_map.emplace(pattern_id, std::ref(term));
            tool_print("Loaded file '%s' with string '%s' label '%s' -> %d",
                term.binfile.c_str(), term.pattern.c_str(), term.label->name, pattern_id);
        }else if(term.matchlen) {
            auto pattern_id = npu_pattern_insert_pcre(
                driver
              , term.pattern.c_str()
              , term.matchlen
              , ""
                );
            if(pattern_id < 0) {
                error_print("failed to open to insert pattern '%s'", term.pattern.c_str());
                continue;
            }
            term_map.emplace(pattern_id, std::ref(term));
            tool_print("Loaded string '%s' label '%s' -> %d",
                term.pattern.c_str(), term.label->name, pattern_id);
        }else{
            auto pattern_id = npu_pattern_insert_binary_file(
                driver
              , term.pattern.c_str()
                );
            if(pattern_id < 0) {
                error_print("failed to open to insert pattern %s", term.pattern.c_str());
                continue;
            }
            term_map.emplace(pattern_id, std::ref(term));
            tool_print("Loaded string '%s' label '%s' -> %d",
                term.pattern.c_str(), term.label->name, pattern_id);
        }
    }
     npu_pattern_load(driver);
     npu_log_set_level(driver,(NPULogLevel)((int)NPU_INFO - verbosity));
     status_print("number of loaded patterns: %d\n", (int)npu_pattern_count(driver));
     status_print("device fill level: %d / %d\n", (int)npu_pattern_fill(driver),npu_pattern_capacity(driver));
     client = NULL;
     if(npu_client_attach(&client,driver) < 0) {
          error_print("could not crete a client for holding reference to npuDriver");
          return 0;
     }
     npu_driver_set_matches(driver, status_size);
     if(npu_thread_start(driver) < 0) {
          error_print("could not start DPU thread");
          return 0;
     }
    return 1;
}
/*-----------------------------------------------------------------------------
 * proc_init
 *  Initialize the instance.
 *
 * [out] rval - 1 if ok, 0 if fail.
 *---------------------------------------------------------------------------*/
int proc_init(wskid_t * kid,
        int argc,
        char ** argv,
        void ** vinstance,
        ws_sourcev_t * sv,
          void * type_table)
{
    //allocate proc instance of this processor
    auto proc = new vectormatch_proc{};
    *vinstance = proc;

    if(!proc->cmd_options(argc, argv, type_table)) {
       delete proc;
       *vinstance = nullptr;
       return 0;
    }
    return 1;
}


/*-----------------------------------------------------------------------------
 * proc_input_set
 * this function needs to decide on processing function based on datatype
 * given.. also set output types as needed (unless a sink)
 *
 * This pfunction returns a *processing function* specfic to the input type,
 * port, and label passed in.
 *
 * [in] vinstance  - the current instance we are dealing with (points to the
 *            local proc_instance_t structure).
 * [in] input_type - specifies the type of input.   For this proc, we can
 *            support the following:
 *                TUPLE_TYPE
 * [in] port        - the input port:  TAG, INVERSE, NOT
 * [i/o] olist      -
 * [in] type_index -
 * [in] type_table - dictionary of labels + dictionary of datatypes.
 *
 * return 1 if ok
 * return 0 if problem
 *---------------------------------------------------------------------------*/
proc_process_t proc_input_set(
    void * vinstance
  , wsdatatype_t * input_type
  , wslabel_t * port
  , ws_outlist_t* olist
  , int type_index
  , void * type_table
    )
{
    auto proc = (vectormatch_proc*)vinstance;
    return proc->input_set(input_type, port, olist, type_index, type_table);
}

proc_process_t vectormatch_proc::input_set(
    wsdatatype_t * input_type
  , wslabel_t * port
  , ws_outlist_t* olist
  , int type_index
  , void * type_table
    )
{
    if (type_index >= LOCAL_MAX_TYPES)
        return nullptr;

    if (wslabel_match(type_table, port, "TAG"))
        do_tag[type_index] = 1;

    // RDS - eliminated the NFLOW_REC and the NPACKET types.
    // TODO:  need to determine whether NPACKET is a type we want to support.
    if (wsdatatype_match(type_table, input_type, "TUPLE_TYPE")){
        if (!outtype_tuple)
            outtype_tuple = ws_add_outtype(olist, dtype_tuple, NULL);

        // Are we searching only on data associated with specific labels,
        // or anywhere in the tuple?
        if (!lset.len) {
            return &vectormatch_proc::proc_process_allstr;  // look in all strings
        } else {
            return &vectormatch_proc::proc_process_meta; // look only in the labels.
        }
    }else if(wsdatatype_match(type_table,input_type, "FLUSH_TYPE")) {
        return &vectormatch_proc::proc_process_flush;
    }
    return nullptr;  // not matching expected type
}
static void result_cb(void *opaque, const npu_result *res)
{
    if(res->nmatches) {
        auto priv = static_cast<callback_data*>(opaque);
        if(priv->nmatches + res->nmatches > priv->capacity) {
            priv->qd_overflow = true;
        }
        if(res->overflow) {
            priv->hw_overflow = true;
        }
        for(auto i = 0;(i < res->nmatches) && (priv->nmatches + i < priv->capacity); ++i) {
            priv->matches[priv->nmatches + i] = res->matches[i];
        }
        priv->nmatches += res->nmatches;
    }
}
static void cleanup_cb(void *opaque)
{
    auto priv = static_cast<callback_data*>(opaque);
    priv->is_done.store(true);
}

/*-----------------------------------------------------------------------------
 * reset_counts
 *
 *
 *---------------------------------------------------------------------------*/
void vectormatch_proc::reset_counts()
{
    for(auto & term : term_vector)
        term.count = 0;
}
/*-----------------------------------------------------------------------------
 * proc_process_meta
 *
 *  The processing function assigned by proc_input_set() for processing
 *  tuples. This function is used if a set of a labels to be searched over
 *  have been supplied.
 *
 * return 1 if output is available
 * return 0 if not output
 *---------------------------------------------------------------------------*/
/*static*/ int vectormatch_proc::proc_process_meta(void * vinstance,    /* the instance */
             wsdata_t* input_data,  /* incoming tuple */
                    ws_doutput_t * dout,    /* output channel */
             int type_index)
{
    auto proc = (vectormatch_proc*)vinstance;
    return proc->process_meta(input_data,dout,type_index);
}
/*static*/ int vectormatch_proc::proc_process_allstr(void * vinstance,    /* the instance */
             wsdata_t* input_data,  /* incoming tuple */
                    ws_doutput_t * dout,    /* output channel */
             int type_index)
{
    auto proc = (vectormatch_proc*)vinstance;
    return proc->process_allstr(input_data,dout,type_index);
}
/*static*/ int vectormatch_proc::proc_process_flush(void * vinstance,    /* the instance */
             wsdata_t* input_data,  /* incoming tuple */
                    ws_doutput_t * dout,    /* output channel */
             int type_index)
{
    auto proc = (vectormatch_proc*)vinstance;
    return proc->process_flush(input_data,dout,type_index);
}
int vectormatch_proc::process_flush(wsdata_t *input_data, ws_doutput_t* dout, int type_index)
{
    npu_client_flush(client);
    if(dtype_is_exit_flush(input_data)) {
        npu_client_free(&client);
        npu_thread_stop(driver);
    }
    while(!cb_queue.empty()) {
        if(cb_queue.front()->empty())
            cb_queue.pop_front();
        else {
            auto &qd = cb_queue.front()->front();
            if(!qd.is_done.load())
                continue;
            if(qd.nmatches && !got_match) {
                hits++;
                got_match = true;
            }
            if(qd.hw_overflow)
                hw_overflow++;
            if(qd.qd_overflow)
                qd_overflow++;
            max_status = std::max<int>(max_status,qd.nmatches);
            for(auto i = 0; i < std::min(qd.capacity,qd.nmatches); ++i) {
                auto mval = qd.matches[i];
                auto eq_range = make_iter_range(term_map.equal_range(mval));
                for(auto & term : eq_range) {
                    term.second.count+=1;
                //i.e., is there a tuple member we are going to add to?
                if (qd.member_data && label_members) {
                    /*get the label associated with the number returned by Aho-Corasick;
                    * default to label_match if one is not found. */
                    auto mlabel = term.second.label;
                    if(mlabel) {
                        if (!wsdata_check_label(qd.member_data, mlabel)) {
                            /* this allows labels to be indexed */
                            tuple_add_member_label(qd.input_data, /* the tuple itself */
                                    qd.member_data,    /* the tuple member to be added to */
                                    mlabel /* the label to be added */);
                        }
                    }
                    if(matched_label && !wsdata_check_label(qd.member_data,matched_label))
                        tuple_add_member_label(
                            qd.input_data, /* the tuple itself */
                            qd.member_data,    /* the tuple member to be added to */
                            matched_label/* the label to be added */);

                }
            }
            }
            if(qd.is_last) {
                if (got_match && matched_label) { /* this is the -L option label */
                    wsdata_add_label(qd.input_data,matched_label);
                }
                if(dout && (got_match || pass_all || do_tag[type_index])) {
                    ws_set_outdata(qd.input_data, outtype_tuple, dout);
                  ++outcnt;
                }
                wsdata_delete(qd.input_data);
                got_match = false;
            }
            cb_queue.front()->pop_front();
        }
    }
    return 1;
}
int vectormatch_proc::process_meta(wsdata_t *input_data, ws_doutput_t* dout, int type_index)
{
    meta_process_cnt++;
    wsdata_t ** members;
    int members_len;
    int found = 0;
    auto submitted = false;
    /* lset - the list of labels we will be searching over.
    * Iterate over these labels and call match on each of them */
    auto bail = false;
    for (auto i = 0; i < lset.len && !bail; i++) {
        if (tuple_find_label(input_data, lset.labels[i], &members_len,&members)){
            auto nbin = 0;
            auto nsub = 0;

            for(auto j = 0; j < members_len; ++j) {
                if(members[j]->dtype == dtype_binary
                || members[j]->dtype == dtype_string)
                    ++nbin;
            }
            if(nbin) {
                wsdata_add_reference(input_data);
                submitted = true;
                for(auto j = 0; j < members_len && !bail; ++j) {
                    auto member = members[j];
                    if(cb_queue.empty() || cb_queue.back()->full()) {
                        cb_queue.emplace_back(new callback_batch());
                    }
                    cb_queue.back()->emplace_back();
                    auto &qd = cb_queue.back()->back();
                    qd.input_data = input_data;
                    qd.member_data= member;
                    qd.is_done.store(false);
                    qd.hw_overflow = 0;
                    qd.qd_overflow = 0;
                    qd.nmatches    = 0;
                    qd.is_last     = (++nsub == nbin);
                    auto err = npu_client_new_packet(client, &qd, result_cb, cleanup_cb);
                    if(err < 0) {
                        qd.is_last = true;
                        qd.is_done.store(true);
                        break;
                    }
                    auto dbeg = (const char*)0;
                    auto dend = (const char*)0;
                    if(member->dtype == dtype_binary) {
                        auto wsb = (wsdt_binary_t*)member->data;
                        dbeg = wsb->buf;
                        dend = dbeg + wsb->len;
                    }else if(member->dtype == dtype_string) {
                        auto wss = (wsdt_string_t*)member->data;
                        dbeg = wss->buf;
                        dend = dbeg + wss->len;
                    }
                    while(dbeg != dend) {
                        auto dnxt = npu_client_write_packet(client,dbeg,dend);
                        if(!dnxt) {
                            auto err = errno;
                            error_print("npu_client_write_packet failed, %d, %s", err,strerror(err));
                            qd.is_last = true;
                            qd.is_done.store(true);
                            bail = true;
                            break;
                        }
                        dbeg = dnxt;
                    }
                    npu_client_end_packet(client);
                }
            }
        }
    }
    if((do_tag[type_index] || pass_all) && !submitted) {
        ws_set_outdata(input_data, outtype_tuple, dout);
      ++outcnt;
    }
    while(!cb_queue.empty()) {
        if(cb_queue.front()->empty())
            cb_queue.pop_front();
        else {
            auto &qd = cb_queue.front()->front();
            if(!qd.is_done.load())
                break;
            found += qd.nmatches;
            if(qd.nmatches && !got_match) {
                hits++;
                got_match = true;
            }
            max_status = std::max<int>(max_status,qd.nmatches);
            if(qd.hw_overflow)
                hw_overflow++;
            if(qd.qd_overflow)
                qd_overflow++;
            for(auto i = 0; i < std::min(qd.capacity,qd.nmatches); ++i) {
                auto mval = qd.matches[i];
                auto eq_range = make_iter_range(term_map.equal_range(mval));
                for(auto & term : eq_range) {
                    term.second.count++;

                //i.e., is there a tuple member we are going to add to?
                if (qd.member_data && label_members) {
                    /*get the label associated with the number returned by Aho-Corasick;
                    * default to label_match if one is not found. */
                    auto mlabel = term.second.label;
                    if (mlabel && !wsdata_check_label(qd.member_data, mlabel)) {
                        /* this allows labels to be indexed */
                        tuple_add_member_label(qd.input_data, /* the tuple itself */
                                qd.member_data,    /* the tuple member to be added to */
                                mlabel /* the label to be added */);
                    }
                    if(matched_label && !wsdata_check_label(qd.member_data,matched_label))
                        tuple_add_member_label(
                            qd.input_data, /* the tuple itself */
                            qd.member_data,    /* the tuple member to be added to */
                            matched_label/* the label to be added */);

                }
            }
            }

            if(qd.is_last ){
                if (got_match && matched_label) { /* this is the -L option label */
                    wsdata_add_label(qd.input_data,matched_label);
                }

                if(got_match || pass_all || do_tag[type_index]) {
                    ws_set_outdata(qd.input_data, outtype_tuple, dout);
                  ++outcnt;
                }
                reset_counts();
                wsdata_delete(qd.input_data);
                got_match = false;
            }
            cb_queue.front()->pop_front();
        }
    }
    return found ? 1 : 0;
}

/*-----------------------------------------------------------------------------
 * proc_process_allstr
 *
 *  The processing function assigned by proc_input_set() for processing
 *  tuples. This function is used if searching should proceed over all
 *  string-based data in the tuple.
 *
 * return 1 if output is available
 * return 0 if not output
 *---------------------------------------------------------------------------*/
int vectormatch_proc::process_allstr(
               wsdata_t* input_data, /* incoming tuple */
               ws_doutput_t * dout, /* output channel */
               int type_index)
{


    meta_process_cnt++;
    auto found = 0;
    auto submitted = false;
    /* lset - the list of labels we will be searching over.
    * Iterate over these labels and call match on each of them */
    auto bail = false;
    auto tup = (wsdt_tuple_t*)input_data->data;
    auto members = tup->member;
    int members_len = tup->len;

    auto nbin = 0;
    auto nsub = 0;

    for(auto j = 0; j < members_len; ++j) {
        if(members[j]->dtype == dtype_binary
        || members[j]->dtype == dtype_string)
            ++nbin;
    }
    if(nbin) {
        wsdata_add_reference(input_data);
        for(auto j = 0; j < members_len && !bail; ++j) {
            auto member = members[j];
            if(cb_queue.empty() || cb_queue.back()->full()) {
                cb_queue.emplace_back(new callback_batch());
            }
            cb_queue.back()->emplace_back();
            auto &qd = cb_queue.back()->back();
            qd.input_data = input_data;
            qd.member_data= member;
            qd.is_done.store(false);
            qd.hw_overflow = 0;
            qd.qd_overflow = 0;
            qd.nmatches    = 0;
            qd.is_last     = (++nsub == nbin);
            submitted = true;
            auto err = npu_client_new_packet(client, &qd, result_cb, cleanup_cb);
            if(err < 0) {
                qd.is_last = true;
                qd.is_done.store(true);
                break;
            }
            auto dbeg = (const char*)0;
            auto dend = (const char*)0;
            if(member->dtype == dtype_binary) {
                auto wsb = (wsdt_binary_t*)member->data;
                dbeg = wsb->buf;
                dend = dbeg + wsb->len;
            }else if(member->dtype == dtype_string) {
                auto wss = (wsdt_string_t*)member->data;
                dbeg = wss->buf;
                dend = dbeg + wss->len;
            }
            while(dbeg != dend) {
                auto dnxt = npu_client_write_packet(client,dbeg,dend);
                if(!dnxt) {
                    auto err = errno;
                    error_print("npu_client_write_packet failed, %d, %s", err,strerror(err));
                    qd.is_last = true;
                    qd.is_done.store(true);
                    bail = true;
                    break;
                }
                dbeg = dnxt;
            }
            npu_client_end_packet(client);
        }
    }
    if((do_tag[type_index] || pass_all) && !submitted) {
        ws_set_outdata(input_data, outtype_tuple, dout);
      ++outcnt;
    }
    while(!cb_queue.empty()) {
        if(cb_queue.front()->empty())
            cb_queue.pop_front();
        else {
            auto &qd = cb_queue.front()->front();
            if(!qd.is_done.load())
                break;
            found += qd.nmatches;
            max_status = std::max<int>(max_status,qd.nmatches);
            if(qd.hw_overflow)
                hw_overflow++;
            if(qd.qd_overflow)
                qd_overflow++;
            if(qd.nmatches && !got_match) {
                hits++;
                got_match = true;
            }
            for(auto i = 0; i < std::min(qd.capacity,qd.nmatches); ++i) {
                auto mval = qd.matches[i];
                auto eq_range = make_iter_range(term_map.equal_range(mval));
                for(auto & term : eq_range) {
                    term.second.count++;
                //i.e., is there a tuple member we are going to add to?
                if (qd.member_data && label_members) {
                    /*get the label associated with the number returned by Aho-Corasick;
                    * default to label_match if one is not found. */
                    auto mlabel = term.second.label;
                    if (mlabel && !!wsdata_check_label(qd.member_data, mlabel)) {
                        /* this allows labels to be indexed */
                        tuple_add_member_label(qd.input_data, /* the tuple itself */
                                qd.member_data,    /* the tuple member to be added to */
                                mlabel /* the label to be added */);
                    }
                    if(matched_label && !wsdata_check_label(qd.member_data,matched_label))
                        tuple_add_member_label(
                            qd.input_data, /* the tuple itself */
                            qd.member_data,    /* the tuple member to be added to */
                            matched_label/* the label to be added */);

                }
            }
            }
            if(qd.is_last) {
                if (got_match && matched_label) { /* this is the -L option label */
                    wsdata_add_label(qd.input_data,matched_label);
                }
                if(got_match || pass_all || do_tag[type_index]) {
                    ws_set_outdata(qd.input_data, outtype_tuple, dout);
                  ++outcnt;
                }
                reset_counts();
                wsdata_delete(qd.input_data);
                got_match = false;
            }
            cb_queue.front()->pop_front();
        }
    }
    return found ? 1 : 0;
}

/*-----------------------------------------------------------------------------
 * proc_destroy
 *  return 1 if successful
 *  return 0 if no..
 *---------------------------------------------------------------------------*/
int proc_destroy(void * vinstance)
{
    auto proc = (vectormatch_proc*)vinstance;

    tool_print("meta_proc cnt %" PRIu64, proc->meta_process_cnt);
    tool_print("matched tuples cnt %" PRIu64, proc->hits);
    tool_print("output cnt %" PRIu64, proc->outcnt);

    delete proc;
    return 1;
}


/*-----------------------------------------------------------------------------
 * vectormatch_add_element
 *  adds an element to the vector table and pushes into AC struct
 *
 * [in] vinstance  - the current instance
 * [in] type_table - needed to create new labels
 * [in] restr       - the input regex pattern/string
 * [in] matchlen     - the input string length
 * [in] labelstr     - string containing the label to be made
 *
 * [out] rval       - 1 if okay, 0 if error.
 *---------------------------------------------------------------------------*/
int vectormatch_proc::add_element(void * type_table,
                const char *restr, const char *binstr, size_t matchlen,
                const char *labelstr)
{
    /* push everything into a vector element */
    auto newlab = labelstr ? wsregister_label(type_table, labelstr)
                : restr ? wsregister_label(type_table,restr)
                : nullptr;
//    auto match_id = term_vector.size();
    term_vector.emplace_back();
    term_vector.back().pattern = std::string { restr ? restr : ""};
    term_vector.back().binfile = std::string { binstr ? binstr : ""};
    term_vector.back().matchlen = matchlen;
    term_vector.back().pid = term_vector.size();
    term_vector.back().label = newlab;

    return 1;
}


/*-----------------------------------------------------------------------------
 * process_hex_string
 *  Lifted straight out of label_match.c
 *
 *---------------------------------------------------------------------------*/
static int process_hex_string(char * matchstr, int matchlen) {
    int i;
    char matchscratchpad[1500];
    int soffset = 0;

    for (i = 0; i < matchlen; i++) {
       if (isxdigit(matchstr[i])) {
          if (isxdigit(matchstr[i + 1])) {
             matchscratchpad[soffset] = (char)strtol(matchstr + i,
                                        NULL, 16);
             soffset++;
             i++;
          }
       }
    }

    if (soffset) {
       //overwrite hex-decoded string on top of original string..
       memcpy(matchstr,matchscratchpad, soffset);
    }

    return soffset;
}
namespace {
const char* find_escaped (const char *ptr,const char *pend, char val)
{
    if(!ptr)
        return nullptr;
    auto ok0 = [](const char*a, const char*b)->bool{return a != b;};
    auto ok1 = [](const char*a, const char* )->bool{return *a;   };
    auto ok = pend ? ok0 : ok1;
    for(; ok(ptr,pend); ++ptr) {
        auto c = *ptr;
        if(c == '\\') {
            if(!ok(++ptr,pend))
                break;
        }else if(c == val) {
            return ptr;
        }
    }
    return pend;
};
}
/*-----------------------------------------------------------------------------
 * vectormatch_loadfile
 *  read input match strings from input file.  This function is taken
 *  from label_match.c/label_match_loadfile, 
 *---------------------------------------------------------------------------*/
int vectormatch_proc::loadfile(void* type_table,const char * thefile)
{
    FILE * fp;
    char line [2001];
    int linelen;
    char * linep = nullptr;
    char * datap = nullptr;
    char * matchstr = nullptr;
    int    matchlen;
    char * binstr = nullptr;
    char * endofstring = nullptr;
    char * match_label = nullptr;

    if ((fp = sysutil_config_fopen(thefile,"r")) == NULL) {
        fprintf(stderr,
            "Error: Could not open vectormatches file %s:\n %s\n",
            thefile, strerror(errno));
        return 0;
    }

    while (fgets(line, 2000, fp)) {
        //strip return
        linelen = strlen(line);
        if (line[linelen - 1] == '\n') {
            line[linelen - 1] = '\0';
            linelen--;
        }

        if ((linelen <= 0) || (line[0] == '#'))
            continue;
        datap = line + linelen;
        linep = line;
        matchstr = NULL;
        match_label = NULL;
        // read line - exact seq
        if (linep[0] == '"') {
            endofstring = (char*)find_escaped(linep + 1,nullptr,'"');
//            linep++;
//            endofstring = (char *)rindex(linep, '"');
            if (endofstring == NULL)
            continue;

            endofstring[0] = '\0';
            matchstr = linep + 1;
            matchlen = strlen(matchstr);
            //sysutil_decode_hex_escapes(matchstr, &matchlen);
            linep = endofstring + 1;
        } else if (linep[0] == '{') {
            linep++;
            endofstring = (char *)index(linep, '}');
            if (endofstring == NULL)
                continue;
            endofstring[0] = '\0';
            matchstr = linep;
            matchlen = process_hex_string(matchstr, strlen(matchstr));
            if (!matchlen)
                continue;

            linep = endofstring + 1;
        } else {
            continue;
        }

        // Get the corresonding label
        if (matchstr) {
            //find (PROTO)
            match_label = (char *) index(linep,'(');
            endofstring = (char *) index(linep,')');
//            endofstring = match_label ? (char *) find_escaped(match_label,nullptr, ')') : nullptr;

            if (match_label && endofstring && (match_label < endofstring)) {
                match_label++;
                endofstring[0] = '\0';
            } else {
                fprintf(stderr,
                    "Error: no label corresponding to match string '%s'\n",
                    matchstr);
                sysutil_config_fclose(fp);
                return 0;
            }
            linep = endofstring + 1;
        }
        if (match_label) {
            binstr = std::find_if(linep,datap, [](char x){return x != ' ' && x != '\t';});
            if(binstr && binstr!= datap) {
                if(*binstr== '"') {
                    ++binstr;
                    auto bin_end = (char*)find_escaped(binstr ,nullptr,'"');
                    if(!bin_end) {
                        binstr = nullptr;
                    } else {
                        *bin_end = '\0';
                    }
                }else if(*binstr== '\'') {
                    ++binstr;
                    auto bin_end = (char*)find_escaped(binstr,nullptr,'\'');
                    if(!bin_end) {
                        binstr = nullptr;
                    } else {
                        *bin_end = '\0';
                    }
                }
            }
        }
        if(!binstr) {
            if (!add_element(type_table, matchstr, nullptr, matchlen,match_label)) {
                sysutil_config_fclose(fp);
                return 0;
            }
            dprint("Adding entry for string '%s' label '%s'",
                matchstr, match_label);
        } else {
            auto binloc = std::string{binstr};
            if (!add_element(type_table, matchstr, binloc.c_str(), matchlen,match_label)) {
                sysutil_config_fclose(fp);
                return 0;
            }
            dprint("Adding entry for file '%s' string '%s' label '%s'",
                binloc.c_str(), matchstr, match_label);
        }

    }
    sysutil_config_fclose(fp);
    return 1;
}
