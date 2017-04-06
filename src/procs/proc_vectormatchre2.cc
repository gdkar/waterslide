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

#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <unistd.h>
#include <cctype>
#include <cmath>   //sqrt
#include <numeric>   //sqrt
#include <algorithm>   //sqrt
#include <utility>   //sqrt
#include <functional>   //sqrt
#include <vector>
#include <string>
#include <cerrno>  //errno
#include <re2/re2.h>
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

typedef struct _flow_data_t {
     wslabel_t * label;
} flow_data_t;

typedef struct _vector_element_t 
{
   RE2 * pattern;
   wslabel_t *label;        /* the label associated with the vector element */
   size_t  count;      /* freq. of occurrence of the element */
} vector_element_t;


/*-----------------------------------------------------------------------------
 *                      P R O C _ I N S T A N C E
 *---------------------------------------------------------------------------*/
typedef struct _proc_instance_t {
   size_t  meta_process_cnt;
   size_t  hits;
   size_t outcnt;

   ws_outtype_t *  outtype_tuple;
   wslabel_t *     matched_label;   /* label affixed to the buffer that matches*/
   wslabel_set_t   lset;            /* set of labels to search over */
   int             intermed_labels; /* 1 if we should labels matched members*/

   int do_tag[LOCAL_MAX_TYPES];
   
   vector_element_t term_vector[LOCAL_VECTOR_SIZE];
   size_t term_vector_len;
} proc_instance_t;

const proc_labeloffset_t proc_labeloffset[] = {};
/*{
    {"MATCH",offsetof(proc_instance_t, matched_label)},
    {"",0},
};*/

/*-----------------------------------------------------------------------------
 *                  F U N C T I O N   P R O T O S
 *---------------------------------------------------------------------------*/
static int proc_process_meta(void *, wsdata_t*, ws_doutput_t*, int);
static int proc_process_allstr(void *, wsdata_t*, ws_doutput_t*, int);

static int vectormatch_loadfile(void *vinstance, 
                                void *type_table, 
                                char *thefile, const re2::RE2::Options & opts = {});

/*-----------------------------------------------------------------------------
 * proc_cmd_options
 *   parse out and process the command line options of the kid.
 *
 * [out] rval - 1 if ok, 0 if error.
 *---------------------------------------------------------------------------*/
static int proc_cmd_options(int argc, char ** argv, 
                            proc_instance_t * proc, void * type_table) 
{
   int op;
   int F_opt = 0;
   auto cfg_list = std::vector<std::string>{};
   auto opt_list = std::string{};
   while ((op = getopt(argc, argv, "O:F:L:M")) != EOF)  {
      switch (op)  {
         case 'O': {
                opt_list += std::string{optarg};
                break;
         }
         case 'M':  /* label tuple data members that match */
            proc->intermed_labels = 1;
            break;
            
         case 'L':  /* labels an entire tuple and  matching members. */
            tool_print("Registering label '%s' for matching buffers.", optarg);
            proc->matched_label = wsregister_label(type_table, optarg);
            break;
            
         case 'F':  /* load up the keyword file */ {
            cfg_list.emplace_back(optarg);
            break;
         }
         default:
            tool_print("Unknown command option supplied");
            exit(-1);
      }
   }
    re2::RE2::Options opts{};
    opts.set_dot_nl(true);
    opts.set_one_line(false);
    opts.set_perl_classes(true);
    opts.set_word_boundary(true);
    opts.set_case_sensitive(true);
    opts.set_posix_syntax(false);
    opts.set_encoding(re2::RE2::Options::EncodingLatin1);
    if(!opt_list.empty()) {
        auto negate = false;
        opts.set_never_nl(false);
        for(auto && c : opt_list) {
            switch(c) {
                case '-': negate = true;continue;
                case 'i': opts.set_case_sensitive(!negate);continue;
                case 'A': opts.set_encoding(negate?re2::RE2::Options::EncodingUTF8:re2::RE2::Options::EncodingLatin1);continue;
                case 'U': opts.set_encoding(!negate?re2::RE2::Options::EncodingUTF8:re2::RE2::Options::EncodingLatin1);continue;
                case 'm': opts.set_one_line(negate);continue;
                case 's': opts.set_dot_nl(!negate); continue;
                default:
                    ;
            };
        }
    }
    for( auto  fp : cfg_list) {
        if (vectormatch_loadfile(proc, type_table, &fp[0], opts))  {
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
      wslabel_set_add(type_table, &proc->lset, argv[optind]);
      dprint("searching for string with label %s",argv[optind]);
      optind++;
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
     proc_instance_t * proc = (proc_instance_t*)calloc(1,sizeof(proc_instance_t));
     *vinstance = proc;
     proc->term_vector_len = 0;

     //read in command options
     if (!proc_cmd_options(argc, argv, proc, type_table)) {
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
 *                   local proc_instance_t structure).
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
   proc_instance_t * proc = (proc_instance_t *)vinstance;
   if (type_index >= LOCAL_MAX_TYPES)  {
      return NULL;
   }
   if (wslabel_match(type_table, port, "TAG"))  {
      proc->do_tag[type_index] = 1;
   }
   // RDS - eliminated the NFLOW_REC and the NPACKET types.
   // TODO:  need to determine whether NPACKET is a type we want to support.
   if (wsdatatype_match(type_table, input_type, "TUPLE_TYPE")) {
      if (!proc->outtype_tuple)  {
         proc->outtype_tuple = ws_add_outtype(olist, dtype_tuple, NULL);  
      }
  
      // Are we searching only on data associated with specific labels,
      // or anywhere in the tuple?
      if (!proc->lset.len) {
         return proc_process_allstr;  // look in all strings
      }else {
         return proc_process_meta; // look only in the labels.
      }
   }
   
   return NULL;  // not matching expected type
}

/*-----------------------------------------------------------------------------
 * find_match
 *   main function performing re-matching.
 *   Appends labels to the appropriate tuple member.
 *
 * [out] rval - 1 if matches found, 0 otherwise.
 *---------------------------------------------------------------------------*/
static inline int find_match(proc_instance_t * proc,   /* our instance */
                             wsdata_t * wsd,           /* the tuple member */
                             char * content,           /* buffer to match */
                             int len,                  /* buffer length */
                             wsdata_t * tdata)         /* the tuple */
{
   if (len <= 0) {
      return 0;
   }

   re2::StringPiece str((const char *)content, len);

   unsigned int mval;
   int matches = 0;
   for(mval = 0; mval < proc->term_vector_len; mval++)  {      
      if(RE2::PartialMatch(str, *(proc->term_vector[mval].pattern)))   {
         proc->term_vector[mval].count++;
         //i.e., is there a tuple member we are going to add to?
         if (wsd && proc->intermed_labels) {
            /*get the label associated with the number returned by Aho-Corasick;
             * default to label_match if one is not found. */
            wslabel_t * mlabel = proc->term_vector[mval].label;
            if (mlabel && !wsdata_check_label(wsd, mlabel)) {
               /* this allows labels to be indexed */
               tuple_add_member_label(tdata, /* the tuple itself */
                                   wsd,   /* the tuple member to be added to */
                                   mlabel /* the label to be added */);
            }
            if (proc->matched_label) /* this is the -L option label */
            {
                tuple_add_member_label(tdata, wsd, proc->matched_label);
    //            tuple_add_member_label(tdata, tdata, proc->matched_label);
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
static inline int member_match(proc_instance_t *proc, /* our instance */
                               wsdata_t *member,      /* member to be searched*/
                               wsdata_t * mpd_label,  /* member to be labeled */
                               wsdata_t * tdata)      /* the tuple */
{
   int found = 0;
   char * buf;
   int len;
   if (dtype_string_buffer(member, &buf, &len)) {
      found = find_match(proc, mpd_label, buf, len, tdata);
   }
   if (found)  {
      proc->hits++;
   }
   
   return found;
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
static int proc_process_meta(void * vinstance,       /* the instance */
                             wsdata_t* input_data,   /* incoming tuple */
                             ws_doutput_t * dout,    /* output channel */
                             int type_index) 
{
   proc_instance_t * proc = (proc_instance_t*)vinstance;
   proc->meta_process_cnt++;
   
   wsdata_t ** mset;
   int mset_len;
   int i, j;
   int found = 0;
   
   /* lset - the list of labels we will be searching over.
    * Iterate over these labels and call match on each of them */
   for (i = 0; i < proc->lset.len; i++) {
      if (tuple_find_label(input_data, proc->lset.labels[i], &mset_len, &mset)) {
         /* mset - the set of members that match the specified label*/
         for (j = 0; j < mset_len; j++ ) {
            found += member_match(proc, mset[j], mset[j], input_data);
         }
      }
   }

   /* add the label and push to output */
   /* TODO - Perhaps I should push this code up to the inner for loop
    * above.  Then there would be a distinct vector for each matched
    * field */
   if (found || proc->do_tag[type_index]) {
      if (found && proc->matched_label && !wsdata_check_label(input_data, proc->matched_label)) {
        wsdata_add_label(input_data, proc->matched_label);
      }
      
      /* now that the vector has been read, reset the counts */
      ws_set_outdata(input_data, proc->outtype_tuple, dout);
      proc->outcnt++;
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
static int proc_process_allstr(void * vinstance,     /* the instance */
                               wsdata_t* input_data, /* incoming tuple */
                               ws_doutput_t * dout,  /* output channel */
                               int type_index) 
{
   proc_instance_t * proc = (proc_instance_t*)vinstance;
   wsdt_tuple_t * tuple = (wsdt_tuple_t*)input_data->data;
   
   proc->meta_process_cnt++;
   
   int i;
   int tlen = tuple->len; //use this length because we are going to grow tuple
   wsdata_t * member;
   int found = 0;
   
   for (i = 0; i < tlen; i++) {
      found = 0;
      member = tuple->member[i];
      found += member_match(proc, member, member, input_data);

      if (found || proc->do_tag[type_index]){
            if (found && proc->matched_label && !wsdata_check_label(input_data, proc->matched_label)) {
                wsdata_add_label(input_data, proc->matched_label);
         }
         /* now that the vector has been read, reset the counts */
         ws_set_outdata(input_data, proc->outtype_tuple, dout);
         proc->outcnt++;
      }
   }

   return 1;
}

#if 0
/*-----------------------------------------------------------------------------
 * proc_process_npkt
 *
 *   The processing function assigned by proc_input_set() for processing
 *   packets.  This function is used if searching should proceed over 
 *   packets.  Not currently used by vectormatchre2
 *
 *   It appears that this function is finding only a first match,
 *   rather than all matches. 
 *
 *   TODO:  Determine whether or not we do repeated invocations.
 *          For now, don't allow this function to be invoked.
 *
 * return 1 if output is available
 * return 0 if not output
 *---------------------------------------------------------------------------*/
static int proc_process_npkt(void * vinstance,       /* the instance */
                             wsdata_t* input_data,   /* input npacket */
                             ws_doutput_t * dout,    /* output channel */
                             int type_index) 
{
   proc_instance_t * proc = (proc_instance_t*)vinstance;
   npacket_t * npkt = (npacket_t*)input_data->data;
   
   // why is no matching being performed here ?
   proc->meta_process_cnt++;
   if (proc->tag_flows) 
   {
      if (lookup_flow_tag(proc, npkt, input_data)) 
      {
         ws_set_outdata(input_data, proc->outtype_npkt, dout);
         proc->outcnt++;
         return 1;
      }
   }

   if (npkt->clen) 
   {
      if (proc->fpkt_only && !wsdata_check_label(input_data, proc->label_fpkt))
      {
         return 1;
      }

      u_char * buf = (u_char*)npkt->content;
      uint32_t buflen = npkt->clen;
      ahoc_state_t ac_ptr = proc->ac_struct->root;
      int mval;
      if ((mval = ac_singlesearch(proc->ac_struct, &ac_ptr,
                                      buf, buflen, &buf, &buflen)) >= 0) 
      {
         wslabel_t * mlabel = proc->term_vector[mval].label;
         
         if (proc->tag_flows) 
         {
            attach_flow_tag(proc, npkt, mlabel);
         }

         if (!wsdata_check_label(input_data, mlabel)) 
         {
            wsdata_add_label(input_data, mlabel);
         }

         ws_set_outdata(input_data, proc->outtype_npkt, dout);
         proc->outcnt++;
      }
   }
   return 1;
}
#endif


/*-----------------------------------------------------------------------------
 * proc_destroy
 *  return 1 if successful
 *  return 0 if no..
 *---------------------------------------------------------------------------*/
int proc_destroy(void * vinstance) {
   proc_instance_t * proc = (proc_instance_t*)vinstance;
   tool_print("meta_proc cnt %" PRIu64, proc->meta_process_cnt);
   tool_print("matched tuples cnt %" PRIu64, proc->hits);
   tool_print("output cnt %" PRIu64, proc->outcnt);

   unsigned int mval;
   for(mval = 0; mval < proc->term_vector_len; mval++) {
      delete(proc->term_vector[mval].pattern);
   }


   //free dynamic allocations
   free(proc);

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
static int vectormatch_add_element(void *vinstance, void * type_table, 
                                   char *restr, unsigned int matchlen,
                                   char *labelstr, const re2::RE2::Options & opts)
{
   wslabel_t *newlab;
   unsigned int match_id;
   proc_instance_t *proc = (proc_instance_t*)vinstance;
   /* push everything into a vector element */
   if (proc->term_vector_len+1 < LOCAL_VECTOR_SIZE) {
      newlab = wsregister_label(type_table, labelstr);
      match_id = proc->term_vector_len;
      proc->term_vector[match_id].pattern = new re2::RE2(restr, opts);
      proc->term_vector[match_id].label = newlab;
      proc->term_vector_len++;
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
static int vectormatch_loadfile(void* vinstance, void* type_table,
                                char * thefile, const re2::RE2::Options & opts) 
{
   FILE * fp;
   char line [2001];
   int linelen;
   char * linep;
   char * matchstr;
   int matchlen;
   char * endofstring;
   char * match_label;

   proc_instance_t *proc = (proc_instance_t*)vinstance;
   
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
            if (!vectormatch_add_element(proc, type_table, matchstr, matchlen, 
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
