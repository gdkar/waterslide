/*-----------------------------------------------------------------------------
 * file:        proc_timestamp.cc
 * date:        12-14-2017
 * description: <see proc_purpose[] below>
 *
 *-----------------------------------------------------------------------------
 * History
 *
 * 12-14-2017        Creation.
 *---------------------------------------------------------------------------*/

#define PROC_NAME "timestamp"
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
#include "datatypes/wsdt_tuple.h"
#include "datatypes/wsdt_ts.h"
#include "datatypes/wsdt_string.h"
#include "datatypes/wsdt_binary.h"
#include "waterslidedata.h"
#include "procloader.h"
#include "label_match.h"
#include "sysutil.h"  //sysutil_config_fopen

/*-----------------------------------------------------------------------------
 *                  D E F I N E S
 *---------------------------------------------------------------------------*/

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
extern "C" const char proc_version[]     = "0.0.1";
extern "C" const char *const proc_alias[]  = { "tstamp", "when", nullptr };

#if defined (MP_DOCS) || true
extern "C" const char *const proc_tags[]     = { "time", "timestamp", "stats", nullptr };
extern "C" const char proc_purpose[]     = "adds timestamps. that's 'bout it.";
extern "C" const char *const proc_synopsis[] = {
    "timestamp [ -N ] [ -I ] [ -L <label>]"
  , nullptr
    };
extern "C" const char proc_description[] =
    "timestamp does wall-clock timestamping."
    "\n";

extern "C" const proc_example_t proc_examples[] = {
    {"...| timestamp -L TSTIME|...\n",
    "add timestamp as a ts with label TSTIME"},
    {"...| timestamp -N -I -L INTTIME|...\n",
    "add timestamp as a int64 with nanosecond precision and label INTTIME"},
    {NULL,""}
};

extern "C" const proc_option_t proc_opts[] = {
    {'L',"label","LABEL",
     "label under which to add the timestamp.", 0, 0},
    {'I',"integer","",
     "add the timestamp as an integer.", 0, 0},
    {'N',"nanoseconds","",
     "use nanosecond precision.", 0, 0},
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
    {nullptr, nullptr}
};
#endif //MP_DOCS

/*-----------------------------------------------------------------------------
 *           D A T A S T R U C T   D E F S
 *---------------------------------------------------------------------------*/

namespace {
template<class It>
struct iter_range : std::tuple<It,It> {
    using traits_type = typename std::iterator_traits<It>;
    using iterator    = It;
    using super       = std::tuple<iterator,iterator>;
    using super::super;
    iterator begin() const { return std::get<0>(*this);}
    iterator end()   const { return std::get<1>(*this);}
};

template<class Tup, class It = typename std::tuple_element<0,Tup>::type>
iter_range<It> make_iter_range( Tup && tup)
{
    return { std::forward<Tup >(tup) };
}
}
/*-----------------------------------------------------------------------------
 *              P R O C _ I N S T A N C E
 *---------------------------------------------------------------------------*/
struct tstamp_proc {
    uint64_t meta_process_cnt{};
    ws_outtype_t *  outtype_tuple{};
    bool            as_integer{false};
    bool            as_nanos  {false};
//    wslabel_t * pattern_id_label{};   /* label affixed to the buffer that matches*/
    wslabel_t * tstamp_label{};   /* label affixed to the buffer that matches*/
   ~tstamp_proc();
    int cmd_options(
       int argc
     , char *argv[]
     , void *type_table
       );

    static int proc_process(void *, wsdata_t*, ws_doutput_t*, int);
    int process_common(wsdata_t*, ws_doutput_t*, int);
    proc_process_t input_set(
        wsdatatype_t * input_type
       , wslabel_t * port
       , ws_outlist_t* olist
       , int type_index
       , void * type_table
        );
};
tstamp_proc::~tstamp_proc()
{
    tool_print("meta process cnt %" PRIu64, meta_process_cnt);
}
const proc_labeloffset_t proc_labeloffset[] = {
//    {"MATCH",offsetof(proc_instance_t, umatched_label)},
//    {"VECTOR",offsetof(proc_instance_t,vector_name)},
//    {"PATTERN_ID",offsetof(tstamp_proc, pattern_id_label)}
};
/*-----------------------------------------------------------------------------
 * proc_cmd_options
 *  parse out and process the command line options of the kid.
 *
 * [out] rval - 1 if ok, 0 if error.
 *---------------------------------------------------------------------------*/
int tstamp_proc::cmd_options(
    int argc
  , char * argv[]
  , void * type_table
    )
{
    auto op = 0;
//    pattern_id_label = wsregister_label(type_table,"PATTERN_ID");
    tstamp_label = wsregister_label(type_table, "TSTAMP");

    while ((op = getopt(argc, argv, "L:IN")) != EOF) {
        switch (op) {
            case 'L':{  /* labels an entire tuple and  matching members. */
                tool_print("Registering label '%s' for matching buffers.", optarg);
                tstamp_label = wsregister_label(type_table, optarg);
                break;
            }
            case 'I':{
                as_integer = true;
                break;
            }
            case 'N':{
                as_nanos = true;
                break;
            }
            default: {
                tool_print("Unknown command option supplied");
                return 0;
            }
        }
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
    auto proc = new tstamp_proc{};
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
    auto proc = (tstamp_proc*)vinstance;
    return proc->input_set(input_type, port, olist, type_index, type_table);
}

proc_process_t tstamp_proc::input_set(
    wsdatatype_t * input_type
  , wslabel_t * port
  , ws_outlist_t* olist
  , int type_index
  , void * type_table
    )
{
    if (wsdatatype_match(type_table, input_type, "TUPLE_TYPE")){
        if (!outtype_tuple)
            outtype_tuple = ws_add_outtype(olist, dtype_tuple, nullptr);
        // Are we searching only on data associated with specific labels,
        // or anywhere in the tuple?
        return &tstamp_proc::proc_process;  // look in all strings
    }
    return nullptr;  // not matching expected type
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
/*static*/ int tstamp_proc::proc_process(void * vinstance,    /* the instance */
             wsdata_t* input_data,  /* incoming tuple */
                    ws_doutput_t * dout,    /* output channel */
             int type_index)
{
    auto proc = (tstamp_proc*)vinstance;
    return proc->process_common(input_data,dout,type_index);
}
int tstamp_proc::process_common(wsdata_t *input_data, ws_doutput_t* dout, int type_index)
{
    meta_process_cnt++;
    auto tsp = timespec{};
    if(as_nanos) {
        clock_gettime(CLOCK_REALTIME,&tsp);
    } else {
        auto tv = timeval{};
        gettimeofday(&tv,nullptr);
        tsp.tv_sec = tv.tv_sec;
        tsp.tv_nsec = tv.tv_usec * 1000;
    }
    if(as_integer) {
        uint64_t val = tsp.tv_sec * 1000000000L + tsp.tv_nsec;
        tuple_member_create_uint64(input_data, (as_nanos ? val : (val/1000)), tstamp_label);
    } else {
        tuple_member_create_ts(input_data,{ tsp.tv_sec, tsp.tv_nsec/1000},tstamp_label);
    }
    ws_set_outdata(input_data, outtype_tuple, dout);
    return 1;
}

/*-----------------------------------------------------------------------------
 * proc_destroy
 *  return 1 if successful
 *  return 0 if no..
 *---------------------------------------------------------------------------*/
int proc_destroy(void * vinstance)
{
    auto proc = (tstamp_proc*)vinstance;
    tool_print("meta_proc cnt %" PRIu64, proc->meta_process_cnt);
    delete proc;
    return 1;
}

