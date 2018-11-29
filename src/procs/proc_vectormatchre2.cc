/*-----------------------------------------------------------------------------
 * file:    proc_vectormatchre2.c
 *               most portions taken from proc_vectormatch.c
 * date:    3-2-2011
 * description: <see proc_purpose[] below>
 *
 *-----------------------------------------------------------------------------
 * History
 *
 * 3-02-2010      Creation.
 * 5-15-2013      Updated documentation
 *---------------------------------------------------------------------------*/

#define PROC_NAME "vectormatchre2"
//#define DEBUG 1

#include <re2/re2.h>
#include <re2/stringpiece.h>
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
#include <bitset>
#include <vector>
#include <array>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>   //sqrt
#include <functional>   //sqrt
#include <cerrno>  //errno
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
 *                           D E F I N E S
 *---------------------------------------------------------------------------*/
#define LOCAL_MAX_TYPES   25
#define LOCAL_VECTOR_SIZE 2000


/*-----------------------------------------------------------------------------
 *                           G L O B A L S
 *---------------------------------------------------------------------------*/
extern "C" const char proc_name[];
extern "C" const proc_labeloffset_t proc_labeloffset[];
extern "C" const char *const proc_alias[];
extern "C" const char *const proc_tags[];
extern "C" const char  proc_purpose[];
extern "C" const char  proc_description[];
extern "C" const char  *const proc_synopsis[];
extern "C" const proc_example_t proc_examples[];
extern "C" const proc_option_t proc_opts[];
extern "C" const char proc_nonswitch_opts[];
extern "C" const char *const proc_input_types[]    ;  //removed "npacket"
extern "C" const char *const proc_output_types[]    ; //removed "npacket"
extern "C" const char *const proc_tuple_member_labels[] ;
extern "C" const char proc_requires[] ;
extern "C" const char *const proc_tuple_container_labels[] ;
extern "C" const char *const proc_tuple_conditional_container_labels[] ;
extern "C" const proc_port_t proc_input_ports[];


 
extern "C" int proc_init(
    wskid_t * kid
  , int argc
  , char ** argv
  , void ** vinstance
  , ws_sourcev_t * sv
  , void * type_table
    );

extern "C" proc_process_t proc_input_set(
    void * vinstance
  , wsdatatype_t * input_type
  , wslabel_t * port
  , ws_outlist_t* olist
  , int type_index
  , void * type_table
    );
extern "C" int proc_destroy(
    void * vinstance
    );
 
const char proc_name[] = PROC_NAME;
const char proc_version[]    = "1.0.1-rc.1";
const char * const proc_alias[]     = { "vectorre2", NULL };
const char *const proc_tags[]     = { "match", "vector", NULL };
const char proc_purpose[]    = "matches a list of regular expressions and returns results";
const char *const proc_synopsis[] = {
     "vectormatchre2 -F <file> [-L <label>] [ -M <label> ] <label of string member to match>",
     NULL };
const char proc_description[] =
   "vectormatchre2 extends vectormatch to perform regular "
   "expression matching.\n"
   "\n"
   "vectormatchre2 uses Google's re2 library.  The format of the "
   "regular expressions are defined in re2/re2.h.\n"
   "\n"
   "\n"
   "Input file: same format as match\n"
   "\"select\"      (SQL_SELECT)\n"
   "\"union\"       (SQL_UNION)\n"
   "\n";

const proc_example_t proc_examples[] = {
   {"...| vectormatchre2 -F \"re.txt\" -M MY_STRING |...\n",
   "apply the labels of the matched regular expressions in "
   "re.txt to the MY_STRING field."},
   {"...| vectormatchre2 -F \"re.txt\" -L RE2_MATCH MY_STRING |...\n",
   "apply the label RE2_MATCH to the MY_STRING field if there was a match."},
   {"...| TAG:vectormatchre2 -F \"re.txt\" -L RE2_MATCH MY_STRING |...\n",
   "pass all tuples, but tag the ones that had a match"},
   {"...| vectormatchre2 -F \"re.txt\" |...\n",
   "match against all strings in the tuple"},
   {NULL,""}
};   

const proc_option_t proc_opts[] = {
     /*  'option character', "long option string", "option argument",
         "option description", <allow multiple>, <required>*/
     {'F',"","file",
     "(required) file with items to search",0,0},
     {'O',"","options",
     "pcre style regex flags to set by default",0,0},
     {'M',"","",
      "affix keyword label to matching tuple member",0,0},
     {'L',"","",
      "common label to affix to matched tuple member",0,0},
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
const char proc_nonswitch_opts[]    = "LABEL of string member to match";
const char *const proc_input_types[]    = {"tuple", NULL};  //removed "npacket"
const char *const proc_output_types[]    = {"tuple", NULL}; //removed "npacket"
const char *const proc_tuple_member_labels[] = {NULL};
const char proc_requires[] = "";
const char *const proc_tuple_container_labels[] = {NULL};
const char *const proc_tuple_conditional_container_labels[] = {NULL};


const proc_port_t proc_input_ports[] = {
     {"none","pass if match"},
     {"TAG","pass all, label tuple if match"},
     {NULL, NULL}
};

/*-----------------------------------------------------------------------------
 *                  D A T A   S T R U C T   D E F S 
 *---------------------------------------------------------------------------*/

struct vector_element {
   std::unique_ptr<RE2> pattern{};
   wslabel_t *label{};        /* the label associated with the vector element */
   size_t  count{};      /* freq. of occurrence of the element */
} ;

using namespace re2;
/*-----------------------------------------------------------------------------
 *                      P R O C _ I N S T A N C E
 *---------------------------------------------------------------------------*/
struct vectormatch_proc {
    using size_type = uint64_t;
    using difference_type = intptr_t;

    struct status_info {
        size_type meta_process_cnt{};
        size_type hit_cnt{};
        size_type out_cnt{};
        size_type byte_cnt{};
        int64_t   start_ts   {};

        static status_info make_info()
        {
            auto res = status_info{};
            auto ts = timespec{};
            clock_gettime(CLOCK_REALTIME, &ts);
            res.start_ts = ts.tv_sec * 1000000000l + ts.tv_nsec;
            return res;
        }
        status_info &operator+=(const status_info &o)
        {
            meta_process_cnt += o.meta_process_cnt;
            hit_cnt          += o.hit_cnt;
            out_cnt          += o.out_cnt;
            byte_cnt         += o.byte_cnt;
            return *this;
        }
    };
    struct status_labels {
        wslabel_t *event_cnt;
        wslabel_t *hit_cnt;
        wslabel_t *out_cnt;
        wslabel_t *byte_cnt;
        wslabel_t *start_ts;
        wslabel_t *end_ts;
        wslabel_t *interval;
        wslabel_t *event_rate;
        wslabel_t *bandwidth;
        wslabel_t *max_matches;
    };
    bool m_started{false};

    void check_started()
    {
        if(m_started)
            return;
        status_total = status_info::make_info();
        status_incremental = status_total;
        m_started = true;
    }
    status_labels stats_labels = {nullptr};
    status_info status_total{};
    status_info status_incremental{};

    wslabel_t *status_label{};
    wslabel_t *status_incr_label{};
    wslabel_t *status_total_label{};

   ws_outtype_t *  outtype_tuple{};
   wslabel_t *     matched_label{};   /* label affixed to the buffer that matches*/
   wslabel_set_t   lset{};            /* set of labels to search over */
   bool label_members{}; /* 1 if we should labels matched members*/

   bool pass_all{};
   std::bitset<LOCAL_MAX_TYPES> do_tag{};
   std::vector<vector_element> term_vector{}; 
    int find_match(          wsdata_t * wsd,           /* the tuple member */
                             char * content,           /* buffer to match */
                             int len,                  /* buffer length */
                             wsdata_t * tdata);         /* the tuple */;

    int member_match(          wsdata_t *member,      /* member to be searched*/
                               wsdata_t * mpd_label,  /* member to be labeled */
                               wsdata_t * tdata);      /* the tuple */

   int process_meta    (wsdata_t*, ws_doutput_t*, int);
   int process_allstr  (wsdata_t*, ws_doutput_t*, int);
   int process_status  (wsdata_t*, ws_doutput_t*, int);
   int process_monitor (wsdata_t*, ws_doutput_t*, int);
    int add_element(void * type_table, char *restr, unsigned int matchlen,
                                   char *labelstr, const RE2::Options & opts);

   ~vectormatch_proc();
    int loadfile(void *type_table, const char *filename, const RE2::Options & opts);
    int cmd_options(
       int argc
     , char *argv[]
     , void *type_table
       );
    proc_process_t input_set(
        wsdatatype_t * input_type
       , wslabel_t * port
       , ws_outlist_t* olist
       , int type_index
       , void * type_table
        );


} ;

const proc_labeloffset_t proc_labeloffset[] = {};
/*{
//    {"MATCH",offsetof(vectormatch_proc, matched_label)},
    {"",0},
};*/

/*-----------------------------------------------------------------------------
 *                  F U N C T I O N   P R O T O S
 *---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
 * proc_cmd_options
 *   parse out and process the command line options of the kid.
 *
 * [out] rval - 1 if ok, 0 if error.
 *---------------------------------------------------------------------------*/
int vectormatch_proc::cmd_options(int argc, char ** argv, void * type_table) 
{
   int op;
   int F_opt = 0;
   auto cfg_list = std::vector<std::string>{};
   auto opt_list = std::string{};
   while ((op = getopt(argc, argv, "PR:r:s:O:F:L:M")) != EOF)  {
      switch (op)  {
         case 'P':{
            pass_all = true;
            break;
         }
         case 'O': {
                opt_list += std::string{optarg};
                break;
         }
         case 'M':  /* label tuple data members that match */
            label_members = true;
            break;
            
         case 'L':  /* labels an entire tuple and  matching members. */
            tool_print("Registering label '%s' for matching buffers.", optarg);
            matched_label = wsregister_label(type_table, optarg);
            break;
            
         case 'F':  /* load up the keyword file */ {
            cfg_list.emplace_back(optarg);
            break;
         }
            case 'R':{  /* labels an entire tuple and  matching members. */
                tool_print("Registering label '%s' for status tuples.", optarg);
                status_total_label = wsregister_label(type_table, optarg);
                break;
            }
            case 'r':{  /* labels an entire tuple and  matching members. */
                tool_print("Registering label '%s' for aggregate status tuples.", optarg);
                status_incr_label = wsregister_label(type_table, optarg);
                break;
            }
            case 's':{  /* labels an entire tuple and  matching members. */
                tool_print("Registering container label '%s' for status tuples.", optarg);
                status_label = wsregister_label(type_table, optarg);
                break;
            }
         default:
            tool_print("Unknown command option supplied");
            exit(-1);
      }
   }
    stats_labels = {
        wsregister_label(type_table, "EVENT_CNT")
      , wsregister_label(type_table, "HIT_CNT")
      , wsregister_label(type_table, "OUT_CNT")
      , wsregister_label(type_table, "BYTE_CNT")
      , wsregister_label(type_table, "START_TS")
      , wsregister_label(type_table, "END_TS")
      , wsregister_label(type_table, "INTERVAL")
      , wsregister_label(type_table, "EVENT_RATE")
      , wsregister_label(type_table, "BANDWIDTH")
    };
    status_total = status_incremental = status_info::make_info();
    RE2::Options opts{};
    opts.set_dot_nl(true);
    opts.set_one_line(false);
    opts.set_perl_classes(true);
    opts.set_word_boundary(true);
    opts.set_case_sensitive(true);
    opts.set_posix_syntax(false);
    opts.set_encoding(RE2::Options::EncodingLatin1);
    if(!opt_list.empty()) {
        auto negate = false;
        opts.set_never_nl(false);
        for(auto && c : opt_list) {
            switch(c) {
                case '-': negate = true;continue;
                case 'i': opts.set_case_sensitive(!negate);continue;
                case 'A': opts.set_encoding(negate?RE2::Options::EncodingUTF8:RE2::Options::EncodingLatin1);continue;
                case 'U': opts.set_encoding(!negate?RE2::Options::EncodingUTF8:RE2::Options::EncodingLatin1);continue;
                case 'm': opts.set_one_line(negate);continue;
                case 's': opts.set_dot_nl(!negate); continue;
                default:
                    ;
            };
        }
    }
    for( auto  fp : cfg_list) {
        if (loadfile(type_table, fp.data(), opts))  {
            tool_print("reading input file %s\n", fp.data());
            F_opt++;
        } else  {
            tool_print("problem reading file %s\n", fp.data());
            return 0;
        }
    }
   if (!F_opt) {
      tool_print("ERROR: -F <filename> option is required.");
      exit(-1);
   }
   while (optind < argc) {
      wslabel_set_add(type_table, &lset, argv[optind]);
      dprint("searching for string with label %s",argv[optind]);
      optind++;
   }
   return 1;
}
vectormatch_proc::~vectormatch_proc()
{
    status_total += status_incremental;
    tool_print("meta_proc cnt %" PRIu64, status_total.meta_process_cnt);
    tool_print("matched tuples cnt %" PRIu64, status_total.hit_cnt);
    tool_print("output cnt %" PRIu64, status_total.out_cnt);
    tool_print("bytes processed %" PRIu64, status_total.byte_cnt);
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
     auto proc = std::make_unique<vectormatch_proc>();
     if(!proc->cmd_options(argc, argv, type_table))
        return 0;

     *vinstance = static_cast<void*>(proc.release());
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
 *                   local vectormatch_proc structure).
 * [in] input_type - specifies the type of input.  For this proc, we can
 *                   support the following:
 *                         TUPLE_TYPE
 * [in] port       - the input port:  TAG, INVERSE, NOT
 * [i/o] olist     -
 * [in] type_index - 
 * [in] type_table - dictionary of labels + dictionary of datatypes.
 *
 * return 1 if ok
 * return 0 if problem
 *---------------------------------------------------------------------------*/
proc_process_t proc_input_set(void * vinstance, 
                              wsdatatype_t * input_type,
                              wslabel_t * port,
                              ws_outlist_t* olist, 
                              int type_index,
                              void * type_table) 
{
   auto proc = (vectormatch_proc *)vinstance;
   if (type_index >= LOCAL_MAX_TYPES)  {
      return NULL;
   }
   if (wslabel_match(type_table, port, "TAG"))
      proc->do_tag[type_index] = true;

   // RDS - eliminated the NFLOW_REC and the NPACKET types.
   // TODO:  need to determine whether NPACKET is a type we want to support.
   if (wsdatatype_match(type_table, input_type, "TUPLE_TYPE")) {
      if (!proc->outtype_tuple)
         proc->outtype_tuple = ws_add_outtype(olist, dtype_tuple, NULL);  
  
      // Are we searching only on data associated with specific labels,
      // or anywhere in the tuple?
      if (!proc->lset.len) {
            return [](void * vinstance,    /* the instance */
             wsdata_t* input_data,  /* incoming tuple */
                    ws_doutput_t * dout,    /* output channel */
             int type_index)
            {
                auto proc = (vectormatch_proc*)vinstance;
                return proc->process_allstr(input_data,dout,type_index);
            };
      }else {
            return [](void * vinstance,    /* the instance */
             wsdata_t* input_data,  /* incoming tuple */
                    ws_doutput_t * dout,    /* output channel */
             int type_index)
            {
                auto proc = (vectormatch_proc*)vinstance;
                return proc->process_meta(input_data,dout,type_index);
            };
      }
   }  else if(wsdatatype_match(type_table,input_type, "FLUSH_TYPE")) {
        if(wslabel_match(type_table, port, "STATUS")) {
            return [](void * vinstance,    /* the instance */
                wsdata_t* input_data,  /* incoming tuple */
                    ws_doutput_t * dout,    /* output channel */
                int type_index)
            {
                auto proc = (vectormatch_proc*)vinstance;
                return proc->process_status(input_data,dout,type_index);
            };
        }   
    }else if(wsdatatype_match(type_table,input_type, "MONITOR_TYPE")) {
            return [](void * vinstance,    /* the instance */
            wsdata_t* input_data,  /* incoming tuple */
                ws_doutput_t * dout,    /* output channel */
                int type_index)
            {
                auto proc = (vectormatch_proc*)vinstance;
                return proc->process_monitor(input_data,dout,type_index);
            };       
    }   return NULL;  // not matching expected type
}

/*-----------------------------------------------------------------------------
 * find_match
 *   main function performing re-matching.
 *   Appends labels to the appropriate tuple member.
 *
 * [out] rval - 1 if matches found, 0 otherwise.
 *---------------------------------------------------------------------------*/
int vectormatch_proc::find_match(wsdata_t * wsd,           /* the tuple member */
                             char * content,           /* buffer to match */
                             int len,                  /* buffer length */
                             wsdata_t * tdata)         /* the tuple */
{
   if (len <= 0) {
      return 0;
   }

   StringPiece str((const char *)content, len);

   int matches = 0;
    for(auto && term : term_vector) {
      if(RE2::PartialMatch(str, *term.pattern))   {
         term.count++;
         //i.e., is there a tuple member we are going to add to?
         if (wsd && label_members) {
            /*get the label associated with the number returned by Aho-Corasick;
             * default to label_match if one is not found. */
            auto mlabel = term.label;
            if (mlabel && !wsdata_check_label(wsd, mlabel)) {
               /* this allows labels to be indexed */
               tuple_add_member_label(tdata, /* the tuple itself */
                                   wsd,   /* the tuple member to be added to */
                                   mlabel /* the label to be added */);
            }
         }
         matches++;
      }   
   }
   return matches ? 1 : 0;
}


/*-----------------------------------------------------------------------------
 * member_match
 *   Performs AC matching on a particular tuple member.  This function is
 *   effectively a wrapper to find_match that simply pulls out the string
 *   buffer in the member.
 *
 * [out] rval - 1 iff a match has been found.
 *---------------------------------------------------------------------------*/
int vectormatch_proc::member_match(
                               wsdata_t *member,      /* member to be searched*/
                               wsdata_t * mpd_label,  /* member to be labeled */
                               wsdata_t * tdata)      /* the tuple */
{
   int found = 0;
   char * buf;
   int len;
   if (dtype_string_buffer(member, &buf, &len)) {
      status_incremental.byte_cnt+=  len;
      found = find_match(mpd_label, buf, len, tdata);
   }
   if (found)
      status_incremental.hit_cnt++;

   return found;
}
int vectormatch_proc::process_status(wsdata_t *input_data, ws_doutput_t* dout, int type_index)
{

    auto tmp = status_incremental;
    status_total += status_incremental;
    status_incremental = status_info::make_info();

    if(!status_label && !status_incr_label && !status_total_label)
        return 0;

    auto tdata = ws_get_outdata(outtype_tuple);

    if(status_label)
        wsdata_add_label(tdata, status_label);

    auto fill_values = [&](wsdata_t *dst, const status_info & data, int64_t _end_ts) {
        auto _start_ts = data.start_ts;
        auto _interval = _end_ts - _start_ts;
        auto _interval_d = _interval * 1e-9;
        tuple_member_create_uint64(dst, data.meta_process_cnt,    stats_labels.event_cnt     );
        tuple_member_create_uint64(dst, data.hit_cnt,             stats_labels.hit_cnt       );
        tuple_member_create_uint64(dst, data.out_cnt,             stats_labels.out_cnt       );
        tuple_member_create_uint64(dst, data.byte_cnt,            stats_labels.byte_cnt      );
        tuple_member_create_uint64(dst, _start_ts,                stats_labels.start_ts      );
        tuple_member_create_uint64(dst, _end_ts,                  stats_labels.end_ts        );
        tuple_member_create_double(dst, _interval_d,              stats_labels.interval      );
        if(_interval_d) {
            tuple_member_create_double(dst, data.meta_process_cnt / _interval_d, stats_labels.event_rate);
            tuple_member_create_double(dst, data.byte_cnt         / _interval_d, stats_labels.bandwidth);
        } else {
            tuple_member_create_double(dst, 0., stats_labels.event_rate);
            tuple_member_create_double(dst, 0., stats_labels.bandwidth);
        }
    };
    if(!status_label) {
        if(status_incr_label && !status_total_label) {
            wsdata_add_label(tdata, status_incr_label);
            fill_values(tdata, tmp, status_incremental.start_ts);
        } else if(status_total_label && !status_incr_label) {
             wsdata_add_label(tdata, status_total_label);
            fill_values(tdata, status_total, status_incremental.start_ts);
           
        }
    } else {
        if(status_incr_label) {
            auto sdata = tuple_member_create_wsdata(tdata, dtype_tuple, status_incr_label);
            fill_values(sdata, tmp, status_incremental.start_ts);
        }
        if(status_total_label) {
            auto sdata = tuple_member_create_wsdata(tdata, dtype_tuple, status_total_label);
            fill_values(sdata, status_total, status_incremental.start_ts);
        }
    }
    ws_set_outdata(tdata, outtype_tuple, dout);
    return 0;
}
/*-----------------------------------------------------------------------------
 * proc_process_monitor_
 *
 *  The processing function assigned by proc_input_set() for processing
 *  tuples. This function is used if a set of a labels to be searched over
 *  have been supplied.
 *
 * return 1 if output is available
 * return 0 if not output
 *---------------------------------------------------------------------------*/
int vectormatch_proc::process_monitor(wsdata_t *input_data, ws_doutput_t* dout, int type_index)
{
    auto tmp = status_incremental;
    status_total += status_incremental;
    status_incremental = status_info::make_info();

    wsdata_t *mtdata = wsdt_monitor_get_tuple(input_data);

    if(!status_label && !status_incr_label && !status_total_label)
        return 0;

    auto tdata = (status_label ? tuple_member_create_wsdata(mtdata,dtype_tuple,status_label) : mtdata);

    if(tdata) {
        auto fill_values = [&](wsdata_t *dst, const status_info & data, int64_t _end_ts) {
            auto _start_ts = data.start_ts;
            auto _interval = _end_ts - _start_ts;
            auto _interval_d = _interval * 1e-9;
            tuple_member_create_uint64(dst, data.meta_process_cnt,    stats_labels.event_cnt     );
            tuple_member_create_uint64(dst, data.hit_cnt,             stats_labels.hit_cnt       );
            tuple_member_create_uint64(dst, data.out_cnt,             stats_labels.out_cnt       );
            tuple_member_create_uint64(dst, data.byte_cnt,            stats_labels.byte_cnt      );
            tuple_member_create_uint64(dst, _start_ts,                stats_labels.start_ts      );
            tuple_member_create_uint64(dst, _end_ts,                  stats_labels.end_ts        );
            tuple_member_create_double(dst, _interval_d,              stats_labels.interval      );
            if(_interval_d) {
                tuple_member_create_double(dst, data.meta_process_cnt / _interval_d, stats_labels.event_rate);
                tuple_member_create_double(dst, data.byte_cnt         / _interval_d, stats_labels.bandwidth);
            } else {
                tuple_member_create_double(dst, 0., stats_labels.event_rate);
                tuple_member_create_double(dst, 0., stats_labels.bandwidth);
            }
        };
        if(!status_label) {
            if(status_incr_label && !status_total_label) {
                wsdata_add_label(tdata, status_incr_label);
                fill_values(tdata, tmp, status_incremental.start_ts);
            } else if(status_total_label && !status_incr_label) {
                wsdata_add_label(tdata, status_total_label);
                fill_values(tdata, status_total, status_incremental.start_ts);
            
            }
        } else {
            if(status_incr_label) {
                auto sdata = tuple_member_create_wsdata(tdata, dtype_tuple, status_incr_label);
                fill_values(sdata, tmp, status_incremental.start_ts);
            }
            if(status_total_label) {
                auto sdata = tuple_member_create_wsdata(tdata, dtype_tuple, status_total_label);
                fill_values(sdata, status_total, status_incremental.start_ts);
            }
        }
    }
    wsdt_monitor_set_visit(input_data);
    return 0;
}

/*-----------------------------------------------------------------------------
 * proc_process_meta
 *
 *   The processing function assigned by proc_input_set() for processing
 *   tuples.  This function is used if a set of a labels to be searched over
 *   have been supplied.
 *
 * return 1 if output is available
 * return 0 if not output
 *---------------------------------------------------------------------------*/
int vectormatch_proc::process_meta(/* the instance */
                             wsdata_t* input_data,   /* incoming tuple */
                             ws_doutput_t * dout,    /* output channel */
                             int type_index) 
{
    check_started();
    status_incremental.meta_process_cnt++;
   
   wsdata_t ** mset  = nullptr;
   int mset_len = 0;
   int i = 0, j = 0;
   int found = 0;
   
   /* lset - the list of labels we will be searching over.
    * Iterate over these labels and call match on each of them */
   for (i = 0; i < lset.len; i++) {
      if (tuple_find_label(input_data, lset.labels[i], &mset_len, &mset)) {
         /* mset - the set of members that match the specified label*/
         for (j = 0; j < mset_len; j++ ) {
            found += member_match(mset[j], mset[j], input_data);
         }
      }
   }

   /* add the label and push to output */
   /* TODO - Perhaps I should push this code up to the inner for loop
    * above.  Then there would be a distinct vector for each matched
    * field */
   if (found || pass_all || do_tag[type_index]) {
      if (found && matched_label && !wsdata_check_label(input_data, matched_label))
        wsdata_add_label(input_data, matched_label);
      
      /* now that the vector has been read, reset the counts */
      ws_set_outdata(input_data, outtype_tuple, dout);
      status_incremental.out_cnt++;
   }
   
   return 1;
}


/*-----------------------------------------------------------------------------
 * proc_process_allstr
 *
 *   The processing function assigned by proc_input_set() for processing
 *   tuples.  This function is used if searching should proceed over all
 *   string-based data in the tuple.
 *
 * return 1 if output is available
 * return 0 if not output
 *---------------------------------------------------------------------------*/
int vectormatch_proc::process_allstr(wsdata_t* input_data, /* incoming tuple */
                               ws_doutput_t * dout,  /* output channel */
                               int type_index) 
{
    check_started();
    status_incremental.meta_process_cnt++;

   wsdt_tuple_t * tuple = (wsdt_tuple_t*)input_data->data;
   
   int i;
   int tlen = tuple->len; //use this length because we are going to grow tuple
   wsdata_t * member;
   int found = 0;
   
   for (i = 0; i < tlen; i++) {
      found = 0;
      member = tuple->member[i];
      found += member_match(member, member, input_data);

      if (found || pass_all || do_tag[type_index]){
            if (found && matched_label && !wsdata_check_label(input_data, matched_label))
                wsdata_add_label(input_data, matched_label);
         /* now that the vector has been read, reset the counts */
         ws_set_outdata(input_data, outtype_tuple, dout);
         status_incremental.out_cnt++;
      }
   }

   return 1;
}

/*-----------------------------------------------------------------------------
 * proc_destroy
 *  return 1 if successful
 *  return 0 if no..
 *---------------------------------------------------------------------------*/
int proc_destroy(void * vinstance) {
   auto  proc = static_cast<vectormatch_proc*>(vinstance);
   //free dynamic allocations
   delete proc;

   return 1;
}


/*-----------------------------------------------------------------------------
 * vectormatch_add_element
 *   adds an element to the vector table and pushes into AC struct
 * 
 * [in] vinstance  - the current instance
 * [in] type_table - needed to create new labels
 * [in] restr      - the input regex pattern/string
 * [in] matchlen   - the input string length
 * [in] labelstr   - string containing the label to be made
 *
 * [out] rval      - 1 if okay, 0 if error.
 *---------------------------------------------------------------------------*/
int vectormatch_proc::add_element(void * type_table, 
                                   char *restr, unsigned int matchlen,
                                   char *labelstr, const RE2::Options & opts)
{
   /* push everything into a vector element */
   if (term_vector.size()+1 < LOCAL_VECTOR_SIZE) {
      auto newlab = wsregister_label(type_table, labelstr);
        term_vector.emplace_back();
        auto &e = term_vector.back();
        e.pattern = std::make_unique<RE2>(restr,opts);
        e.label = newlab;
   } else {
      fprintf(stderr, "proc_vectormatchre2.c: regex pattern table full.\n");
      return 0;
   }
   
   return 1;
}


/*-----------------------------------------------------------------------------
 * process_hex_string
 *   Lifted straight out of label_match.c
 *
 *---------------------------------------------------------------------------*/
static int process_hex_string(char * matchstr, int matchlen) {
     int i;
     char matchscratchpad[1500];
     int soffset = 0;
     
     for (i = 0; i < matchlen; i++) {
          if (isxdigit(matchstr[i])) {
               if (isxdigit(matchstr[i + 1])) {
                    matchscratchpad[soffset] = (char)strtol(matchstr + i, NULL, 16);
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
 *   read input match strings from input file.  This function is taken
 *   from label_match.c/label_match_loadfile.
 *---------------------------------------------------------------------------*/
int vectormatch_proc::loadfile(void* type_table,const char * thefile, const RE2::Options & opts) 
{
   FILE * fp;
   char line [2001];
   int linelen;
   char * linep;
   char * matchstr;
   int matchlen;
   char * endofstring;
   char * match_label;
   
   if ((fp = sysutil_config_fopen(thefile,"r")) == NULL) {
      fprintf(stderr, 
              "Error: Could not open vectormatches file %s:\n  %s\n",
              thefile, strerror(errno));
      return 0;
   } 
   
   while (fgets(line, 2000, fp))  {
      //strip return
      linelen = strlen(line);
      if (line[linelen - 1] == '\n')  {
         line[linelen - 1] = '\0';
         linelen--;
      }
      if ((linelen <= 0) || (line[0] == '#'))  {
         continue;
      }

      linep = line;
      matchstr = NULL;
      match_label = NULL;
      
      // read line - exact seq
        if (linep[0] == '"') {
            endofstring = (char*)find_escaped(linep + 1,nullptr,'"');
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
            if (!add_element(type_table, matchstr, matchlen, 
                                        match_label, opts))
            {
                sysutil_config_fclose(fp);
                return 0;
            }
            tool_print("Adding entry for string '%s' label '%s'",
                    matchstr, match_label);
        }

   }
   sysutil_config_fclose(fp);
   return 1;
}
