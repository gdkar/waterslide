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

#define PROC_NAME "redisstream"
//#define DEBUG 1

#include <functional>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <unistd.h>
#include <cstring>

#include <memory>
#include <utility>
#include <algorithm>
#include <type_traits>
#include <iostream>
#include <cstdio>
#include <string>
#include <numeric>
#include <tuple>
#include <deque>
#include <vector>
#include <array>

#include <hiredis/hiredis.h>
#include "waterslide.h"
#include "waterslidedata.h"
#include "datatypes/wsdt_tuple.h"
#include "datatypes/wsdt_uint.h"
#include "datatypes/wsdt_double.h"
#include "stringhash5.h"
#include "procloader.h"


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

extern "C" const char proc_name[]     =  PROC_NAME;
extern "C" const char proc_version[] =  "1.0";
extern "C" const char *const proc_tags[]    =  {"Database", "Distributed", NULL};
extern "C" const char *const proc_alias[]             =  { "hiredisstream", NULL };
extern "C" const char proc_purpose[]            =  "interact with redis database";
extern "C" const char proc_description[]  = "Interact with redis database including "
     "GET, SET, DELETE, SUBSCRIBE, PUBLISH, INCREMENT, and DECREMENT. "
     "This kid can act as a source for subscription events. One can use this "
     "kid for interacting with other stream processing, for distributed "
     "processing or for storing data into more permanent storage.";

extern "C" const proc_option_t proc_opts[]      =  {
     /*  'option character', "long option string", "option argument",
     "option description", <allow multiple>, <required>*/
     {'P',"","stream",
      "stream key to write events into.",0,0},
     {'S',"","stream key",
      "label containing the stream key to write events into.",0,0},
     {'H',"","sub tuple",
      "label of the sub tuple to emit to the stream.",0,0},
     {'E',"","sub element",
      "label of the sub element to emit to the stream.",0,0},
     {'L',"","subsest prefix / name",
      "name or prefix to use for previous sub element / tuple",0,0},
     {'I',"","include element of tuple",
      "label to pick out of the proceeding sub tuple",0,0},
     {'R',"","flatten",
      "equivalent to -L '', only meaningful for sub tuples",0,0},
     {'h',"","hostname",
     "redis hostname (default to localhost)",0,0},
     {'p',"","port",
     "redis port (default to 6379)",0,0},
     {'m',"","maxlen",
      "maxlen parameter for XADD calls.",0,0},
     {'M',"","maxlen label",
      "label containing the value to use as maxlen.",0,0},
     //the following must be left as-is to signify the end of the array
     {' ',"","",
     "",0,0}
};

extern "C" const char proc_nonswitch_opts[]     =  "LABEL of key";
extern "C" const char *const proc_input_types[]       =  {"tuple", NULL};
// (Potential) Output types: tuple
extern "C" const char *const proc_output_types[]      =  {"tuple", NULL};
//char proc_requires[]           =  "";
// Ports: QUERYAPPEND
extern "C" const proc_port_t proc_input_ports[] =  {
     {"none","Emit the tuple to a redis stream"},
//     {"GET","Get value at key"},
//     {"SET","Set value at key"},
//     {"INCR","Increment value at key"},
//     {"DECR","Decrement value at key"},
//     {"PUBLISH","Publish value at specified channel"},
     {NULL, NULL}
};
//char *proc_tuple_container_labels[] =  {NULL};
//char *proc_tuple_conditional_container_labels[] =  {NULL};
extern "C" const char * const proc_tuple_member_labels[] =  {"VALUE", NULL};
extern "C" const char * const proc_synopsis[]          =  {"[PORT]:redisstream <LABEL> [-V VALUE] [-h hostname] [-p port]", NULL};
proc_example_t proc_examples[] =  {
     {"... | GET:redis WORD -L INFO | ...", "Queries redis server "
      "with the specified string in the WORD buffer; "
      "labels any result as INFO."},
     {"... | SET:redis WORD -V COUNT ", "Sets key and value in redis server "
      "with the specified key string in the WORD buffer and specified value "
      "string in COUNT buffer.; Has not output in this mode."},
     {"... | SET:redis WORD -V COUNT -x 5m ", "Sets key and value in redis server "
      "with the specified key string in the WORD buffer and specified value "
      "string in COUNT buffer; Value in redis is held for only 5 minutes."},
     {"... | INCR:redis WORD -L RESULT | ...", "Increments value at key in redis server "
      "using the specified key string in the WORD buffer; "
      "Resulting value after increment is labeled RESULT."},
     {"... | DECR:redis WORD -L RESULT | ...", "Decrements value at key in redis server "
      "using the specified key string in the WORD buffer;"
      "Resulting value after decrement is labeled RESULT."},
     {"... | PUBLISH:redis -P WordChannel VERB ADVERB | ...", "Publishes values in redis server "
      "using the channel named WordChannel using value strings found in VERB and ADVERB; "
      "input tuple is passed downstream as a passthrough operation."},
     {"redis -S SubChannel -h redishost -p 12345 -L REDISOUT | ...",
      "Subscribes to channel SubChannel in redis server found at host redishost and port 12345;"
      "Any output strings are labeled RESIDOUT; This acts as source kid that blocks waiting for input."},
    {NULL, NULL}
};

//function prototypes for local functions
/*static int proc_get(void *, wsdata_t*, ws_doutput_t*, int);
static int proc_set(void *, wsdata_t*, ws_doutput_t*, int);
static int proc_incr(void *, wsdata_t*, ws_doutput_t*, int);
static int proc_decr(void *, wsdata_t*, ws_doutput_t*, int);
static int proc_publish(void *, wsdata_t*, ws_doutput_t*, int);
static int proc_rsubscribe(void *, wsdata_t*, ws_doutput_t*, int);
*/
struct rc_del { void operator()(redisContext *rc){ if(rc) redisFree(rc);}};
using redis_ptr = std::unique_ptr<redisContext, rc_del>;
struct rr_del { void operator()(redisReply *rr){ if(rr) freeReplyObject(rr);}};
using reply_ptr = std::unique_ptr<redisReply,rr_del>;

struct sub_desc {
    wslabel_nested_set_t nest{};
    wslabel_set_t        lset{};
    std::string          prefix{};
    bool                 as_tuple{};
};
struct proc_redisstream {
     uint64_t meta_process_cnt{};
     uint64_t outcnt{};

     std::string hostname{"localhost"};
     uint16_t port{6379u};
 
     wslabel_t * stream_label{};
     std::string stream_key  {"waterslide"};

     uint64_t    maxlen      {};
     std::string maxlen_str  {};
     uint64_t    maxpipe     {};

     uint64_t    pipe_count  {};

     std::deque<sub_desc> items{};

     redis_ptr rc{};
     time_t expire_sec{};

     ~proc_redisstream();
     int cmd_options(int argc, char ** argv, void * type_table);

     void appendCommandArgv(int argc, const char * *argv, const size_t *argvlen);
     int  drainReplies(int count);
     std::pair<reply_ptr,int> getReply();
int proc_xadd(wsdata_t * tuple,ws_doutput_t * dout, int type_index);
int proc_flush(wsdata_t * tuple,ws_doutput_t * dout, int type_index);

};

int proc_redisstream::cmd_options(int argc, char ** argv, void * type_table) {
     int op;
     while ((op = getopt(argc, argv, "P:S:E:H:M:B:h:p:L:R:I:")) != EOF) {
          switch (op) {
          case 'P':
               stream_key = std::string{optarg};
               tool_print("publishing to stream named %s", stream_key.c_str());
               break;
          case 'S':
               stream_label = wsregister_label(type_table,optarg);
               tool_print("publishing to stream found under label %s", stream_label->name);
               break;
          case 'E': {
               items.emplace_back();
               auto &item = items.back();
               wslabel_nested_search_build(type_table, &item.nest, optarg);
               item.prefix = std::string{optarg};
               item.as_tuple = false;
               tool_print("emitting the element found at %s", optarg);
               break;
          }
          case 'I': {
                if(!items.empty()) {
                    auto & item = items.back();
                    if(!item.as_tuple)
                        break;
                    wslabel_set_add(type_table, &item.lset, optarg);
                    tool_print("including label %s in group %s", optarg,item.prefix.c_str());
                }
                break;
          }
          case 'H': {
               items.emplace_back();
               auto &item = items.back();
               wslabel_nested_search_build(type_table, &item.nest, optarg);
               item.prefix = std::string{optarg} + ".";
               item.as_tuple = true;
               tool_print("emitting the subtuple found at %s", optarg);
               break;
          }
          case 'L': {
                if(!items.empty()) {
                    auto &item = items.back();
                    item.prefix = std::string{optarg};
                    if(item.prefix.size() && item.as_tuple) {
                        if(item.prefix[item.prefix.size() - 1] != '.')
                            item.prefix += ".";
                    }
                    tool_print("emitting the subtuple found at %s", optarg);
                }
                break;
          }
          case 'R' : {
                if(!items.empty()) {
                    auto &item = items.back();
                    if(!item.as_tuple)
                        break;
                    item.prefix = std::string{};
                }
                break;
          }
          case 'M':
               maxlen = atoi(optarg);
               maxlen_str = std::to_string(maxlen);
               break;
          case 'B':
               maxpipe = atoi(optarg);
               break;
          case 'h':
               hostname = std::string(optarg);
               break;
          case 'p':
               port = (uint16_t)atoi(optarg);
               break;
          default:
                error_print("unrecognized options. %c\n",op);
               return 0;
          }
     }
     while (optind < argc) {
        if(argv[optind] and *argv[optind]) {
            auto _prefix = std::string{argv[optind]};
            items.emplace_back();
            auto &item = items.back();
            wslabel_nested_search_build(type_table, &item.nest, _prefix.c_str());
            item.prefix = _prefix;
            item.as_tuple = false;
            tool_print("emitting the element found at %s", _prefix.c_str());
        }
        optind++;
     }
     return 1;
}

// the following is a function to take in command arguments and initalize
// this processor's instance..
//  also register as a source here..
// return 1 if ok
// return 0 if fail
int proc_init(wskid_t * kid, int argc, char ** argv, void ** vinstance, ws_sourcev_t * sv,
              void * type_table) {
     
     //allocate proc instance of this processor
     auto proc = std::make_unique<proc_redisstream>();
     *vinstance = proc.get();

     //read in command options
     if (!proc->cmd_options(argc, argv, type_table))
          return 0;

    {
        struct timeval timeout = { 1, 500000 }; // 1.5 seconds
        auto c = redis_ptr(redisConnectWithTimeout(proc->hostname.c_str(), proc->port, timeout));
        if (!c|| c->err) {
            if (c) {
                printf("Connection error: %s\n", c->errstr);
            } else {
                printf("Connection error: can't allocate redis context\n");
            }
            return 0;
        }
        proc->rc = std::move(c);
        tool_print("got context");
    }
     *vinstance = proc.release();
     dprint("finished init");
     return 1; 
}

// this function needs to decide on processing function based on datatype
// given.. also set output types as needed (unless a sink)
//return 1 if ok
// return 0 if problem
proc_process_t proc_input_set(void * vinstance, wsdatatype_t * meta_type,
                              wslabel_t * port,
                              ws_outlist_t* olist, int type_index,
                              void * type_table) {
     tool_print("input_set");
    if (wsdatatype_match(type_table, meta_type, "TUPLE_TYPE")){

     tool_print("found xadd input");
     return [](void * vinstance, wsdata_t * tuple,
                      ws_doutput_t * dout, int type_index) {
        return static_cast<proc_redisstream*>(vinstance)->proc_xadd(
            tuple, dout, type_index);
     };
     }
    else if(wsdatatype_match(type_table,meta_type,"FLUSH_TYPE")) {
        dprint("found flush input");
        return [](void * vinstance, wsdata_t * tuple,
                        ws_doutput_t * dout, int type_index) {
            return static_cast<proc_redisstream*>(vinstance)->proc_flush(tuple, dout, type_index);
        };
    } else {
        dprint("not tuple or flush");
        return nullptr; // a function pointer
    }
}
static constexpr const char s_empty_{};
template<int CAP = 8192>
struct str_builder {
    using pointer = char*;
    using const_pointer = const char*;
    std::array<char, CAP> data_{};
    pointer               begin_{&data_[0]};
    pointer               end_  {begin_};
    pointer               limit_{begin_ + CAP};
    static constexpr const_pointer empty_begin() { return &s_empty_;}
    static constexpr const_pointer empty_end()   { return &s_empty_;}

    size_t space() const      { return limit_ - end_; }
    size_t capacity() const   { return CAP; }
    const_pointer data() const{ return &data_[0];}
    pointer data()            { return &data_[0];}
    size_t fill() const       { return end_ - data();}
    size_t size() const       { return end_ - begin_;}
    std::pair<const_pointer, size_t> get() const
    {
        if(begin_ == end_) {
            return { empty_begin(), 0ul};
        } else {
            return { static_cast<const_pointer>(begin_), end_ - begin_};
        }
    }
    template<class It>
    bool append(It from_, It to_)
    {
        if(from_ == to_ || !char(*from_))
            return true;
        auto tend = end_;
        while(tend != limit_) {
            if(from_ == to_|| !char(*from_)) {
                end_ = tend;
                return true;
            } else {
                *tend++ = char(*from_++);
            }
        }
        if(from_ == to_ || !char(*from_))
            return true;
        return false;
    }
    bool append(const char *from, size_t len)
    {
        return append(from, from+len);
    }
    bool append(const char *from_)
    {
        if(!char(*from_))
            return true;
        auto tend = end_;
        while(tend != limit_) {
            if(auto c = char(*from_)) {
                *tend++ = c;
                from_++;
            } else {
                end_ = tend;
                return true;
            }
        }
        return false;
    }
    std::pair<const_pointer, size_t> finish()
    {
        auto res = get();
        begin_ = end_;
        return res;
    }
    void discard()
    {
        end_ = begin_;
    }
};
template<int N = 128>
struct avec {
    using ccpointer = const char*;
    int                         argc{};
    std::array<ccpointer,N>     argv{};
    std::array<size_t,N>        argvlen{};

    int size() const { return argc;}
    static constexpr int capacity(){return N;}
    bool full() const { return size() == capacity();}
    std::pair<ccpointer&, size_t&> back()
    { return std::make_pair(std::ref(argv[argc-1]),std::ref(argvlen[argc-1])); }
    void push_back(const std::pair<const char*,size_t> & val)
    {
        if(!full()) {
            argc++;
            back() = val;
        }
    }
    void emplace_back(const char*ptr,size_t len)
    {
        push_back(std::make_pair(ptr,len));
    }
    void push_back(const char * str)
    {
        emplace_back(str, str ? strlen(str) : 0);
    }
    void push_back(const std::string & str)
    {
        emplace_back(str.c_str(),str.size());
    }
    template<size_t M>
    void push_back(const char (&str)[M])
    {
        emplace_back(static_cast<const char*>(str[0]),M);
    }
};

void proc_redisstream::appendCommandArgv(int argc, const char **argv, const size_t *argvlen)
{
    if(rc) {
        redisAppendCommandArgv(rc.get(),argc, argv, argvlen);
        pipe_count++;
        if(pipe_count > maxpipe)
            drainReplies(pipe_count - maxpipe + ( maxpipe >> 4));
    }
}
proc_redisstream::~proc_redisstream()
{
    if(rc) {
        while(pipe_count) {
            auto res = getReply();
            if(res.first) {
                pipe_count--;
            } else {
                auto err = res.second;
                if(err == REDIS_ERR_EOF || err == REDIS_ERR_OTHER || err == REDIS_ERR_OTHER) {
                    break;
                }
            }
        }
    }
    drainReplies(pipe_count);
}
std::pair<reply_ptr,int> proc_redisstream::getReply()
{
    redisReply *reply = nullptr;
    if(!rc) {
        return { reply_ptr{}, REDIS_ERR_EOF};
    }
    if(redisGetReply(rc.get(), (void**)&reply) == REDIS_OK) {
        return {reply_ptr{reply}, REDIS_OK};
    } else {
        return {reply_ptr{}, rc->err};
    }
}
int proc_redisstream::drainReplies(int count)
{
    auto res = 0;
    while(count--> 0) {
        auto rep = getReply();;
        auto & rp = rep.first;
        auto & err = rep.second;
        if(rp || err == REDIS_ERR_PROTOCOL) {
            pipe_count--;
            res++;
        } else {
            break;
        }
    }
    return res;
}
int proc_redisstream::proc_flush(wsdata_t *ituple,ws_doutput_t *dout, int type_idex)
{
    drainReplies(pipe_count);
    return 1;
}
int proc_redisstream::proc_xadd(wsdata_t *ituple,ws_doutput_t *dout, int type_idex)
{
    meta_process_cnt++;

    wsdata_t *skey{};
    auto stream_ = std::make_pair<char*,int>(&stream_key[0],int(stream_key.size()));
    if(stream_label) {
        if((skey = tuple_find_single_label(ituple, stream_label))) {
            if(skey->dtype == dtype_binary){
                auto bdata_ = (wsdt_binary_t*)skey->data;
                stream_ = std::make_pair(bdata_->buf,bdata_->len);
            } else {
                dtype_string_buffer(skey, &stream_.first, &stream_.second);
            }
        }
    }
    if(items.size()) {
        auto args = avec<128>{};
        auto sbuf = str_builder<8192>{};
        args.push_back("XADD");
        args.push_back(stream_);
        if(maxlen) {
            args.push_back("MAXLEN");
            args.push_back("~");
            args.push_back(maxlen_str);
        }
        args.push_back("*");
        auto valid = false;
        for(auto && item : items) {
            wsdata_t *m= NULL;
            if(!item.as_tuple) {
                tuple_nested_search(ituple,
                    &item.nest,
                    [](void *vproc, void *vret, wsdata_t *tdata, wsdata_t *member) -> int{
                        wsdata_t **wret = (wsdata_t **)vret;
                        if(!wret|| *wret)
                            return 0;
                        *wret = member;
                        return 1;
                    },this, &m);
                if(m) {
                    auto tmp = std::pair<char*,int>{};
                    if(m->dtype == dtype_binary) {
                        auto bdata_ = (wsdt_binary_t*)m->data;
                        tmp = std::make_pair(bdata_->buf, bdata_->len);
                    } else {
                        dtype_string_buffer(m, &tmp.first, &tmp.second);
                    }
                    if(!tmp.first || !tmp.second)
                        continue;
                    args.push_back(item.prefix);
                    args.push_back(tmp);
                    valid = true;
                }
            } else {
                tuple_nested_search(ituple,
                    &item.nest,
                    [](void *self, void *vret, wsdata_t *tdata, wsdata_t *member) -> int{
                        wsdata_t **wret = (wsdata_t **)vret;
                        if(!wret|| *wret)
                            return 0;
                        if(member->dtype == dtype_tuple) {
                            auto s = (wsdt_tuple_t*)member->data;
                            if(s->len)
                                *wret = member;
                        }
                        return 1;
                    },this, &m);
                if(m && m->dtype == dtype_tuple) {
                    auto add_member = [&](wsdata_t * msub, const char*_lab) {
                        if(!_lab && msub->label_len)
                            _lab = msub->labels[0]->name;
                        if(_lab) {
                            auto tmp = std::pair<char*,int>{};
                            if(msub->dtype == dtype_binary) {
                                auto bdata_ = (wsdt_binary_t*)msub->data;
                                tmp = std::make_pair(bdata_->buf, bdata_->len);
                            } else {
                                dtype_string_buffer(msub, &tmp.first, &tmp.second);
                            }
                            if(!tmp.first || !tmp.second)
                                return false; 
                            if(item.prefix.size()) {
                                if(!sbuf.append(item.prefix.data(),item.prefix.size())
                                || !sbuf.append(_lab)) {
                                    sbuf.discard();
                                    return false;
                                }
                                args.push_back(sbuf.finish());
                            } else {
                                args.push_back(_lab);
                            }
                            args.push_back(tmp);
                            valid = true;
                            return true;
                        }
                        return false;
                    };
                    if(item.lset.len) {
                        auto iter = tuple_labelset_iter_t{};
                        tuple_init_labelset_iter(&iter, m, &item.lset);
                        auto _fid = 0;
                        auto _flabel = static_cast<wslabel_t*>(nullptr);
                        auto _fmember= static_cast<wsdata_t*>(nullptr);
                        while(tuple_search_labelset(&iter, &_fmember, &_flabel, &_fid)) {
                            if(!add_member(_fmember, _flabel->name))
                                continue;
                            iter.mlen = 0;
                            iter.labelpos++;
                        }
                    } else {
                        auto s = (wsdt_tuple_t*)m->data;
                        auto subs = s->member;
                        auto nsub = s->len;
                        for(size_t i = 0; i < nsub; ++i)
                            add_member(subs[i], nullptr);
                    }
                }
            }
        }
        if(valid) {
            appendCommandArgv(args.size(),&args.argv[0],&args.argvlen[0]);
            return 1;
        }
    } else {
    auto astuple = (wsdt_tuple_t*)ituple->data;
    auto args = avec<128>{};
    args.push_back("XADD");
    args.push_back(stream_);
    if(maxlen) {
        args.push_back("MAXLEN");
        args.push_back("~");
        args.push_back(maxlen_str);
    }
    args.push_back("*");
    auto valid = false;

    /*    auto dump_tuple = [&](wsdt_tuple_t* astuple) {
            args.push_back("XADD");
            args.push_back(stream_);
            if(maxlen) {
                args.push_back("MAXLEN");
                args.push_back("~");
                args.push_back(maxlen_str);
            }
            args.push_back("*");

            auto valid = false;*/
        for(size_t i = 0; i < astuple->len; ++i) {
            auto m = astuple->member[i];
            if(m->label_len) {
                auto tmp = std::pair<char*,int>{};
                if(m->dtype == dtype_binary) {
                    auto bdata_ = (wsdt_binary_t*)m->data;
                    tmp = std::make_pair(bdata_->buf, bdata_->len);
                } else {
                    dtype_string_buffer(m, &tmp.first, &tmp.second);
                }
                if(!tmp.first || !tmp.second)
                    continue;
                args.push_back(m->labels[0]->name);
                args.push_back(tmp);
                valid = true;
            }
        }
        if(valid) {
            appendCommandArgv(args.size(),&args.argv[0],&args.argvlen[0]);
            return 1;
        }
    }
    return 1;
}
//// proc processing function assigned to a specific data type in proc_io_init
//return 1 if output is available
// return 0 if not output
/*static int proc_get(void * vinstance, wsdata_t * tuple,
                      ws_doutput_t * dout, int type_index) {

     dprint("proc_get");
     proc_instance_t * proc = (proc_instance_t*)vinstance;
     proc->meta_process_cnt++;

     dprint("search keys");
     tuple_nested_search(tuple, &proc->nest_keys,
                         nest_search_callback_get,
                         proc, NULL);
     
     ws_set_outdata(tuple, proc->outtype_tuple, dout);

     //always return 1 since we don't know if table will flush old data
     return 1;
}

static int proc_rsubscribe(void * vinstance, wsdata_t * tuple,
                      ws_doutput_t * dout, int type_index) {
     
     proc_instance_t * proc = (proc_instance_t*)vinstance;
     redisReply *reply = NULL;

     dprint("attempting to get subscribe reply");
     if (redisGetReply(proc->rc,(void**)&reply) != REDIS_OK) {
          dprint("got invalid reply");
          return 1;
     }
     
     dprint("got reply %d", reply->type);
     if ((reply->type == REDIS_REPLY_STRING) && reply->len && reply->str) {
          dprint("got reply string");
          tuple_dupe_binary(tuple, proc->label_outvalue, reply->str, reply->len);
          ws_set_outdata(tuple, proc->outtype_tuple, dout);
     }
     else if (reply->type == REDIS_REPLY_ARRAY) {
          if (reply->elements == 3) {
               redisReply *r = reply->element[2];
               if ((r->type == REDIS_REPLY_STRING) && r->len && r->str) {
                    tuple_dupe_binary(tuple, proc->label_outvalue, r->str, r->len);
                    ws_set_outdata(tuple, proc->outtype_tuple, dout);
               }
          }
     }
     freeReplyObject(reply);
     return 1;
}

//select first element in search
static int nest_search_callback_one(void * vproc, void * vkey,
                                    wsdata_t * tdata, wsdata_t * member) {
     wsdata_t ** pkey = (wsdata_t**)vkey; 
     if (*pkey != NULL) {
          return 0;
     }
     *pkey = member;
     return 1;
}

static int proc_set(void * vinstance, wsdata_t * tuple,
                      ws_doutput_t * dout, int type_index) {

     proc_instance_t * proc = (proc_instance_t*)vinstance;
     proc->meta_process_cnt++;

     wsdata_t * key = NULL;
     wsdata_t * value = NULL;

     tuple_nested_search(tuple, &proc->nest_keys,
                         nest_search_callback_one,
                         proc, &key);
     tuple_nested_search(tuple, &proc->nest_values,
                         nest_search_callback_one,
                         proc, &value);

     if (key && value) {
          dprint("found key and value");
          char * keybuf = NULL;
          int keylen = 0;
          char * valbuf = NULL;
          int vallen = 0;
          
          if (dtype_string_buffer(key, &keybuf, &keylen) && 
              dtype_string_buffer(value, &valbuf, &vallen)) {
               dprint("found key and value strings");

               redisReply *reply;

               if (proc->expire_sec) {
                    reply = redisCommand(proc->rc, "SET %b %b EX %d",
                                         keybuf, keylen, valbuf, vallen,
                                         (int)proc->expire_sec);
               }
               else {
                    dprint("setting %.*s %.*s", keylen, keybuf, vallen, valbuf);
                    reply = redisCommand(proc->rc, "SET %b %b",
                                         keybuf, keylen, valbuf, vallen);
               }
               if (reply) {
                    freeReplyObject(reply);
               }
          }
     }

     //always return 1 since we don't know if table will flush old data
     return 1;
}

static int nest_search_callback_incr(void * vproc, void * vevent,
                                    wsdata_t * tdata, wsdata_t * member) {
     proc_instance_t * proc = (proc_instance_t*)vproc;
     //search for member key
     char * buf = NULL;
     int len = 0;

     if (!dtype_string_buffer(member, &buf, &len)) {
          return 0;
     }
     redisReply *reply;

     reply = redisCommand(proc->rc, "INCR %b", buf, len);
     if (reply) {
          if (reply->type == REDIS_REPLY_INTEGER) {
               tuple_member_create_int(tdata, reply->integer, proc->label_outvalue);
          }
          freeReplyObject(reply);
     }
     return 1;
}

static int proc_incr(void * vinstance, wsdata_t * tuple,
                      ws_doutput_t * dout, int type_index) {

     proc_instance_t * proc = (proc_instance_t*)vinstance;
     proc->meta_process_cnt++;

     tuple_nested_search(tuple, &proc->nest_keys,
                         nest_search_callback_incr,
                         proc, NULL);
     
     ws_set_outdata(tuple, proc->outtype_tuple, dout);

     //always return 1 since we don't know if table will flush old data
     return 1;
}

static int nest_search_callback_decr(void * vproc, void * vevent,
                                    wsdata_t * tdata, wsdata_t * member) {
     proc_instance_t * proc = (proc_instance_t*)vproc;
     //search for member key
     char * buf = NULL;
     int len = 0;

     if (!dtype_string_buffer(member, &buf, &len)) {
          return 0;
     }
     redisReply *reply;

     reply = redisCommand(proc->rc, "DECR %b", buf, len);
     if (reply) {
          if (reply->type == REDIS_REPLY_INTEGER) {
               tuple_member_create_int(tdata, reply->integer, proc->label_outvalue);
          }
          freeReplyObject(reply);
     }
     return 1;
}

static int proc_decr(void * vinstance, wsdata_t * tuple,
                      ws_doutput_t * dout, int type_index) {

     proc_instance_t * proc = (proc_instance_t*)vinstance;
     proc->meta_process_cnt++;

     tuple_nested_search(tuple, &proc->nest_keys,
                         nest_search_callback_decr,
                         proc, NULL);
     
     ws_set_outdata(tuple, proc->outtype_tuple, dout);
     //always return 1 since we don't know if table will flush old data
     return 1;
}

static int nest_search_callback_publish(void * vproc, void * vevent,
                                    wsdata_t * tdata, wsdata_t * member) {
     dprint("got publish value");
     proc_instance_t * proc = (proc_instance_t*)vproc;
     //search for member key
     char * buf = NULL;
     int len = 0;

     if (!dtype_string_buffer(member, &buf, &len)) {
          return 0;
     }
     redisReply *reply;

     dprint("attempting to publish value");
     reply = redisCommand(proc->rc, "PUBLISH %s %b", proc->publish_channel, buf, len);
     if (reply) {
          dprint("success in publishing");
          freeReplyObject(reply);
     }
     return 1;
}


static int proc_publish(void * vinstance, wsdata_t * tuple,
                      ws_doutput_t * dout, int type_index) {

     proc_instance_t * proc = (proc_instance_t*)vinstance;
     proc->meta_process_cnt++;

     tuple_nested_search(tuple, &proc->nest_keys,
                         nest_search_callback_publish,
                         proc, NULL);
     tuple_nested_search(tuple, &proc->nest_values,
                         nest_search_callback_publish,
                         proc, NULL);
     
     ws_set_outdata(tuple, proc->outtype_tuple, dout);

     //always return 1 since we don't know if table will flush old data
     return 1;
}*/

//return 1 if successful
//return 0 if no..
int proc_destroy(void * vinstance) {
    delete static_cast<proc_redisstream*>(vinstance);
    return 1;
}


