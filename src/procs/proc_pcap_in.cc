/*
No copyright is claimed in the United States under Title 17, U.S. Code.
All Other Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#define PROC_NAME "pcap_in"
//#define DEBUG 1
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <algorithm>
#include <utility>
#include <string>
#include <deque>
#include <sys/stat.h>
#include <unistd.h>
#include <poll.h>
#include <zlib.h>
#include <glob.h>
#if defined(__FreeBSD__)
#include <sys/endian.h>
#else
#include <endian.h>
#endif
#include <string>
#include <deque>
#include "waterslide.h"
#include "waterslidedata.h"
#include "procloader.h"
#include "sysutil.h"
#include "datatypes/wsdt_tuple.h"
#include "datatypes/wsdt_fixedstring.h"
#include "datatypes/wsdt_uint.h"
#include "datatypes/wsdt_uint64.h"
#include "datatypes/wsdt_double.h"
#include "protobuf/wsproto_lib.h"
#include "protobuf/ws_protobuf.h"

#include <pcap/pcap.h>

// TODO: as part of file metadata, include file offset and record length

extern "C" const char proc_version[]     = "0.1.1-rc.1";
extern "C" const char *const proc_tags[]     = { "pcap","input", NULL };
extern "C" const char *const proc_alias[]     = { "pcap","pcap_in","pcap_filein", NULL };
extern "C" const char proc_name[]       = PROC_NAME;
extern "C" const char proc_purpose[]    = "reads in data or files in stdin, creates metadata";
extern "C" const char *const proc_synopsis[]   = { "pcap_in [-m] [-s]", NULL };
extern "C" const char proc_description[] ="Reads in metadata from files or stdin in both the "
                         "pcap file input. ";
extern "C" const proc_example_t proc_examples[]    = {
     {"pcap_in | ...","process the list of files whose names were passed to stdin"},
     {"pcap_in -m | ...","include the file name of the tuple's source file in the tuple"},
     {"pcap_in -r '/tmp/*.pcap.gz' | ...", "Process all files in /tmp which match *.pcap.gz"},
     {NULL,""}
};
extern "C" const char proc_requires[]    = "";
extern "C" const char *const proc_input_types[]        = {NULL};
extern "C" const char *const proc_output_types[]   = {"tuple", NULL};
extern "C" const proc_port_t proc_input_ports[]  = {{NULL, NULL}};
extern "C" const char *const proc_tuple_container_labels[]     = {NULL};
extern "C" const char *const proc_tuple_conditional_container_labels[] = {NULL};
extern "C" const char *const proc_tuple_member_labels[] = {
        "PCAP_FILENAME",
        "PCAP_LINKTYPE",
        "TSTAMP",
        "CONTENT",
        };
extern "C" const proc_option_t proc_opts[] = {
     /*  'option character', "long option string", "option argument",
	 "option description", <allow multiple>, <required>*/
     //the following must be left as-is to signify the end of the array
     {'L',"","",
     "Label to use for output ( defualt 'CONTENT' )",0,0},
     {'m',"","",
     "pass on file metadata",0,0},
     {'s',"","",
     "suppress output about file opening",0,0},
     {'i',"","",
     "read data from stdin",0,0},
     {'r',"","Filepath",
     "expand passed glob as a list of files to process",1,0},
     {' ',"","",
     "",0,0}
};
extern "C" const char proc_nonswitch_opts[] = "";

#define LOCAL_FILENAME_MAX 2048

namespace std {
template<>
struct default_delete<pcap_t> {
    using pointer = pcap_t*;
    void operator() ( pointer ptr) noexcept { if(ptr) pcap_close(ptr);}
};
}
using pcap_ptr = std::unique_ptr<pcap_t>;
//function prototypes for local functions
struct proc_instance {
     uint64_t meta_process_cnt  {};
     uint64_t badfile_cnt       {};
     uint64_t outcnt            {};
     uint64_t total_bytes       {};

     ws_outtype_t * outtype_tuple{};
     std::deque<std::string> devices{};
     std::deque<std::string> filenames{};

     int snaplen{65536};
     int promisc{};
     int to_ms  {50};

     FILE * in    {};
     pcap_ptr pcap;
     int used_glob{};
     void * type_table          {};
     wsdata_t * file_wsd        {};
     wslabel_t * label_file     {};
     wslabel_t * label_linktype {};
     wslabel_t * label_content  {};
     wslabel_t * label_tstamp   {};
     wslabel_t * label_caplen   {};
     wslabel_t * label_length   {};
     bool stdin_data{};
     bool done{};
     bool pass_file_meta{};
     bool suppress_output{};
     bool repeat_forever{};
     bool is_device{};
     uint64_t repeat_until{};
};

static int read_names_glob(proc_instance *proc, const char *pattern);
static int read_names_file(proc_instance *proc);
static int get_next_file(proc_instance * proc);
static int proc_pcap_in_source(void *, wsdata_t*, ws_doutput_t*, int);

static int proc_cmd_options(int argc, char ** argv, 
                             proc_instance * proc, void * type_table)
{
     auto op = 0;
     while ((op = getopt(argc, argv, "d:N:PT:r:ismL:RU:")) != EOF) {
        switch (op) {
            case 'd':
                proc->devices.emplace_back(optarg);
                break;
            case 'T':
                proc->to_ms = atoi(optarg);
                break;
            case 'N':
                proc->snaplen = atoi(optarg);
                break;
            case 'P':
                proc->promisc = 1;
                break;
            case 'i':
                proc->stdin_data = true;
                break;
            case 'L':
                if(optarg)
                    proc->label_content = wsregister_label(type_table, optarg);
                break;
            case 'm':
                proc->pass_file_meta = true;
                break;
            case 's':
                proc->suppress_output = true;
                break;
            case 'r':
                read_names_glob(proc, optarg);
                proc->used_glob++;
                break;
            case 'R':
                proc->repeat_forever = true;
                break;
            case 'U': {
                auto _until = atol(optarg);
                if(_until > 0) {
                    proc->repeat_until = _until;
                }
            }
            default:
                return 0;
        }
     }
     if(proc->stdin_data && proc->pass_file_meta) {
          error_print("cannot pass file metadata (-m) if reading content from stdin (-i)");
          return 0;
     }
     return 1;
}
static int read_names_file(proc_instance *proc)
{
     auto count = 0;
     char buf[LOCAL_FILENAME_MAX+1] = { 0, };

     while ( fgets(buf, LOCAL_FILENAME_MAX, proc->in) ) {
          auto len = ::strlen(buf); auto p = buf;
          while ( len && std::isspace(*p) ) {
               p++; len--;
          }
          while ( len && std::isspace(p[len-1]) ) {
               p[len-1] = '\0'; len--;
          }
          struct stat sbuf{};
          if ( !::stat(p, &sbuf) ) { /* Check if file exists */
               proc->filenames.push_back(p);
               count++;
          } else {
               error_print("File '%s' cannot be read", p);
               proc->badfile_cnt++;
          }
     }
     return count;
}
static int read_names_glob(proc_instance *proc, const char *pattern)
{
     auto count = 0;
     auto globbuf = glob_t{};
     auto opts = 0;
     dprint("looking for files matching '%s'",pattern);
//#ifdef GLOB_BRACE
     opts |= GLOB_BRACE;
//#endif
//#ifdef GLOB_TILDE
     opts |= GLOB_TILDE;
//#endif
     auto ret = ::glob(pattern, opts, nullptr, &globbuf);
     if ( !ret ) {
          for ( auto i = 0ul; i < globbuf.gl_pathc ; i++ ) {
               count++;
               dprint("adding globbed file '%s'",globbuf.gl_pathv[i]);
               proc->filenames.push_back(globbuf.gl_pathv[i]);
          }
     }
     globfree(&globbuf);
     return count;
}
static int get_next_device(proc_instance *proc)
{
    if(proc->done)
        return 0;
    while(!proc->devices.empty()) {
        auto _device = std::move(proc->devices.front());
        proc->devices.pop_front();
        char errbuf[PCAP_ERRBUF_SIZE] = {0,};
        proc->pcap.reset(pcap_open_live(_device.c_str(),proc->snaplen, proc->promisc, proc->to_ms, errbuf));
        if(proc->pcap) {
            if (!proc->suppress_output) {
                tool_print("opened device%s", errbuf);
            }
            proc->is_device=true;
            if(proc->repeat_forever || (proc->total_bytes < proc->repeat_until))
                proc->devices.push_back(_device);
            return 1;
        } else {
            tool_print("failed to open device %s, %s", _device.c_str(),errbuf);
        }
    }
    return 0;
}
static int get_next_file(proc_instance * proc)
{
     if (proc->done)
          return 0;

     //close old capture if needed
     proc->pcap.reset();
     if(get_next_device(proc))
        return 1;
     while ( !proc->filenames.empty() ) {
          auto filename = proc->filenames.front();
          proc->filenames.pop_front();
          char errbuf[PCAP_ERRBUF_SIZE] = { 0, };
          proc->pcap.reset(pcap_open_offline(filename.c_str(), errbuf));
          if(proc->pcap) {
               if (proc->pass_file_meta) {
                    auto fn   = ::basename(&filename[0]);
                    auto flen = int(::strlen(fn));

                    if (proc->file_wsd)
                         wsdata_delete(proc->file_wsd);

                    if((proc->file_wsd = wsdata_create_string(fn, flen))){
                         wsdata_add_reference(proc->file_wsd);
                         wsdata_add_label(proc->file_wsd, proc->label_file);
                    }
               }
               if (!proc->suppress_output) {
                    tool_print("opened file %s", errbuf);
               }
               if(proc->repeat_forever || (proc->total_bytes < proc->repeat_until))
                    proc->filenames.push_back(filename);
                proc->is_device=false;
               return 1;
          }
     }
     proc->done = true;
     return 0;
}
// the following is a function to take in command arguments and initalize
// this processor's instance..
//  also register as a source here..
// return 1 if ok
// return 0 if fail
int proc_init(wskid_t * kid, int argc, char ** argv, void ** vinstance, ws_sourcev_t * sv, void * type_table)
{
     //allocate proc instance of this processor
     auto proc = new proc_instance{};
     *vinstance       = proc;
     proc->in         = stdin;
     proc->type_table = type_table; // for inline label lookup
     proc->label_content  = wsregister_label(type_table, "CONTENT");

     //read in command options
     if (!proc_cmd_options(argc, argv, proc, type_table))
          return 0;

     proc->label_file     = wsregister_label(type_table, "PCAP_FILENAME");
     proc->label_linktype = wsregister_label(type_table, "PCAP_LINKTYPE");
     proc->label_tstamp   = wsregister_label(type_table, "TSTAMP");
     proc->label_caplen   = wsregister_label(type_table, "CAP_LEN");
     proc->label_length   = wsregister_label(type_table, "LENGTH");

     if(!proc->outtype_tuple)
        proc->outtype_tuple = ws_register_source_byname(type_table,"TUPLE_TYPE", proc_pcap_in_source, sv);

    if (proc->stdin_data) {
        char errbuf[PCAP_ERRBUF_SIZE] = { 0, };
        proc->pcap.reset(pcap_fopen_offline_with_tstamp_precision(
            stdin, PCAP_TSTAMP_PRECISION_NANO, errbuf
            ));
        if(!proc->pcap) {
            error_print(
                "failed to open stdin as a pcap dump. '%s'"
                , errbuf
                );
        }
    } else if (proc->devices.empty()) {
        /* Read from stdin if user didn't specify with -g */
        if ( !proc->used_glob && proc->filenames.empty() )
            read_names_file(proc);
        if ( proc->devices.empty() && proc->filenames.empty() ) {
            error_print("No files to process");
        }
    }
    // set the buffer
    return 1; 
}
// this function needs to decide on processing function based on datatype
// given.. also set output types as needed (unless a sink)
//return 1 if ok
// return 0 if problem
proc_process_t proc_input_set(void * vinstance, wsdatatype_t * input_type,
                              wslabel_t * port,
                              ws_outlist_t* olist, int type_index,
                              void * type_table)
{
     return nullptr;
}
static inline int read_next_record(proc_instance * proc, wsdata_t * tdata)
{
    while(true) {
            // since we are closing the file, mark the format as unknown.
        if(!proc->pcap) {
            if (proc->stdin_data)
                return 0;

            get_next_file(proc);
            if (!proc->pcap) {
                dprint("nofile");
                return 0;
            }
        }
        auto pkt_hdr  = pcap_pkthdr{};
        auto pkt_data = pcap_next(
            proc->pcap.get()
          , &pkt_hdr
            );
        if(!pkt_data) {
            if(!proc->is_device) {
                proc->pcap.reset();
            }
        } else {
            tuple_dupe_binary(
                tdata
              , proc->label_content
              , const_cast<char*>(
                    reinterpret_cast<const char*>(
                        pkt_data
                        )
                    )
              , pkt_hdr.caplen
                );
            tuple_member_create_ts(
                tdata
              , wsdt_ts_t{ pkt_hdr.ts.tv_sec, pkt_hdr.ts.tv_usec}
              , proc->label_tstamp
                );
            tuple_member_create_uint(
                tdata
              , pkt_hdr.caplen
              , proc->label_caplen
                );
            tuple_member_create_uint(
                tdata
              , pkt_hdr.len
              , proc->label_length
                );
            tuple_member_create_int(
                tdata
              , pcap_datalink(proc->pcap.get())
              , proc->label_linktype
                );
            proc->total_bytes += pkt_hdr.caplen;
            return 1;
        }
    }
}
// get the size of the typetable
static int32_t get_current_index_size(void * type_table) {
       mimo_datalists_t * mdl = (mimo_datalists_t *)type_table;
       return mdl->index_len;
}
//// proc processing function assigned to a specific data type in proc_io_init
//return 1 if output is available
// return 0 if not output
//
static int proc_pcap_in_source(
    void * vinstance
  , wsdata_t* source_data
  , ws_doutput_t * dout
  , int type_index
    )
{
     auto proc = (proc_instance*)vinstance;
     if (proc->done)
          return 0;
     // get the size of the index so we can know later if it has changed
     auto start_index_size = get_current_index_size(proc->type_table); 
     if (read_next_record(proc, source_data)) {
          auto tuple = (wsdt_tuple_t*)source_data->data;
          if (tuple->len) {
               proc->meta_process_cnt++;
               if (proc->pass_file_meta && proc->file_wsd) {
                    add_tuple_member(
                        source_data
                      , proc->file_wsd
                        );
               }
               // get the end index size
               auto end_index_size = get_current_index_size(proc->type_table); 
               if(start_index_size == end_index_size) {
                    // if the index size remained the same, write out the tuple
                    ws_set_outdata(
                        source_data
                      , proc->outtype_tuple
                      , dout
                        );
                    proc->outcnt++;
               } else {
                    // if the index size changed, dupe the tuple to reindex the
                    // labels in the tuple
                    dprint("cloning tuple to add new labels into label index");
                    auto source_data_copy = wsdata_alloc(dtype_tuple);
                    if(!source_data_copy) {
                         return 0;
                    }
                    if(!tuple_deep_copy(
                        source_data
                      , source_data_copy
                        )) {
                         wsdata_delete(source_data_copy);
                         return 0;
                    }
                    ws_set_outdata(
                        source_data_copy
                      , proc->outtype_tuple
                      , dout
                        );
                    proc->outcnt++;
                }
                return 1;
          }
     }
     return 0;
}
//return 1 if successful
//return 0 if no..
int proc_destroy(void * vinstance)
{
     auto proc = (proc_instance*)vinstance;
     tool_print("meta_proc cnt %" PRIu64, proc->meta_process_cnt);
     tool_print("output cnt %" PRIu64, proc->outcnt);
     tool_print("total bytes %" PRIu64, proc->total_bytes);
     if (proc->badfile_cnt) {
          tool_print("badfile cnt %" PRIu64, proc->badfile_cnt);
     }
     if (proc->in && proc->in != stdin) {
          ::fclose(proc->in);
     }
     if (proc->file_wsd) {
          wsdata_delete(proc->file_wsd);
     }
     //free dynamic allocations
     delete proc;

     return 1;
}
