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

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <math.h>   //sqrt
#include <errno.h>  //errno
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

#ifdef __cplusplus
CPP_OPEN
#endif


/*-----------------------------------------------------------------------------
 *                           D E F I N E S
 *---------------------------------------------------------------------------*/
#define LOCAL_MAX_TYPES   25
#define LOCAL_VECTOR_SIZE 2000


/*-----------------------------------------------------------------------------
 *                           G L O B A L S
 *---------------------------------------------------------------------------*/
char proc_name[]       = PROC_NAME;
char proc_version[]    = "1.0";
const char *proc_alias[]     = { "vectorre2", NULL };
#ifdef MP_DOCS
const char *proc_tags[]     = { "match", "vector", NULL };
char proc_purpose[]    = "matches a list of regular expressions and returns a vector";
const char *proc_synopsis[] = {
     "vectormatchre2 [-V <label>] -F <file> [-L <label>] [-W] <label of string member to match>",
     NULL };
char proc_description[] =
   "vectormatchre2 extends vectormatch to perform regular "
   "expression matching.  vectormatchre2 will produce a vector of doubles "
   "representing which patterns were matched.\n"
   "\n"
   "vectormatchre2 uses Google's re2 library.  The format of the "
   "regular expressions are defined in re2/re2.h.\n"
   "\n"
   "Each pattern to be matched can contain an optional weight, and "
   "vectornorm can be used to compute various norms (weighted "
   "and unweighted) of the resulting vector.\n"
   "\n"
   "Input file: same format as match, with an optional 3rd column "
   "containing the weight; e.g.\n"
   "\"select\"      (SQL_SELECT)      0.8\n"
   "\"union\"       (SQL_UNION)       1.0\n"
   "\n";

proc_example_t proc_examples[] = {
   {"...| vectormatchre2 -F \"re.txt\" -V MY_VECTOR MY_STRING |...\n",
   "match the MY_STRING tuple member against the regular expressions in re.txt.  "
   "Create a vector of the results and create a new vector of doubles under the "
   " label of MY_VECTOR.  Only pass the tuples that match"},
   {"...| vectormatchre2 -F \"re.txt\" -V MY_VECTOR -M MY_STRING |...\n",
   "apply the labels of the matched regular expressions in "
   "re.txt to the MY_STRING field."},
   {"...| vectormatchre2 -F \"re.txt\" -V MY_VECTOR -L RE2_MATCH MY_STRING |...\n",
   "apply the label RE2_MATCH to the MY_STRING field if there was a match."},
   {"...| TAG:vectormatchre2 -F \"re.txt\" -V MY_VECTOR -L RE2_MATCH MY_STRING |...\n",
   "pass all tuples, but tag the ones that had a match"},
   {"...| vectormatchre2 -F \"re.txt\" -V MY_VECTOR |...\n",
   "match against all strings in the tuple"},
   {NULL,""}
};   

proc_option_t proc_opts[] = {
     /*  'option character', "long option string", "option argument",
	 "option description", <allow multiple>, <required>*/
     {'V',"","label",
     "attach vector and tag with <label>",0,0},
     {'F',"","file",
     "(required) file with items to search",0,0},
     {'M',"","",
      "affix keyword label to matching tuple member",0,0},
     {'L',"","",
      "common label to affix to matched tuple member",0,0},
     {'W',"","",
      "use weighted counts",0,0},
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
char proc_nonswitch_opts[]    = "LABEL of string member to match";
const char *proc_input_types[]    = {"tuple", NULL};  //removed "npacket"
const char *proc_output_types[]    = {"tuple", NULL}; //removed "npacket"
char *proc_tuple_member_labels[] = {NULL};
char proc_requires[] = "";
char *proc_tuple_container_labels[] = {NULL};
char *proc_tuple_conditional_container_labels[] = {NULL};


extern int errno;

proc_port_t proc_input_ports[] = {
     {"none","pass if match"},
     {"TAG","pass all, label tuple if match"},
     {NULL, NULL}
};
#endif //MP_DOCS

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
   unsigned int count;      /* freq. of occurrence of the element */
   double weight;           /* the weight (contained in the input file */
} vector_element_t;


/*-----------------------------------------------------------------------------
 *                      P R O C _ I N S T A N C E
 *---------------------------------------------------------------------------*/
typedef struct _proc_instance_t {
   uint64_t meta_process_cnt;
   uint64_t hits;
   uint64_t outcnt;

   ws_outtype_t *  outtype_tuple;
   wslabel_t *     matched_label;   /* label affixed to the buffer that matches*/
   wslabel_t *     vector_name;     /* label to be affixed to the matched vector */
   wslabel_set_t   lset;            /* set of labels to search over */
   int             intermed_labels; /* 1 if we should labels matched members*/

   int do_tag[LOCAL_MAX_TYPES];
   
   vector_element_t term_vector[LOCAL_VECTOR_SIZE];
   unsigned int term_vector_len;

   unsigned int weighted_counts; /* should we weight the counts? */

} proc_instance_t;


/*-----------------------------------------------------------------------------
 *                  F U N C T I O N   P R O T O S
 *---------------------------------------------------------------------------*/
static int proc_process_meta(void *, wsdata_t*, ws_doutput_t*, int);
static int proc_process_allstr(void *, wsdata_t*, ws_doutput_t*, int);

static inline int  add_vector(void * vinstance, wsdata_t* input_data);
static inline void reset_counts(proc_instance_t *proc);

static int vectormatch_loadfile(void *vinstance, 
				void *type_table, 
				char *thefile);

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

   while ((op = getopt(argc, argv, "WV:F:L:M")) != EOF) 
   {
      switch (op) 
      {
	 case 'M':  /* label tuple data members that match */
	    proc->intermed_labels = 1;
	    break;
	    
	 case 'L':  /* labels an entire tuple and  matching members. */
	    tool_print("Registering label '%s' for matching buffers.", optarg);
	    proc->matched_label = wsregister_label(type_table, optarg);
	    break;
	    
	 case 'F':  /* load up the keyword file */
	    if (vectormatch_loadfile(proc, type_table, optarg)) 
	    {
	       tool_print("reading input file %s\n", optarg);
	    }
	    else 
	    {
	      tool_print("problem reading file %s\n", optarg);
	      return 0;
	    }

	    F_opt++;
	    break;
	    
	 case 'V':
	    {
	       char temp[1200];
	       if (strlen(optarg) >= 1196)
	       {
		  fprintf(stderr, 
			  "Error: (vectormatchre2) -V argument too long: %s\n", 
			  optarg);
		  return 0;
	       }
	       strncpy(temp, optarg, 1196);
	       strcat(temp, "_LEN");

	       proc->vector_name = wsregister_label(type_table, optarg);

	       tool_print("setting vector label to %s", optarg);
   	    }
	    break;

	 case 'W':
	    proc->weighted_counts = 1;
	    break;

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
	    
	 default:
	    tool_print("Unknown command option supplied");
	    exit(-1);
      }
   }

   if (!F_opt)
   {
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
     proc_instance_t * proc =
          (proc_instance_t*)calloc(1,sizeof(proc_instance_t));
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
   
   if (type_index >= LOCAL_MAX_TYPES) 
   {
      return NULL;
   }
   
   if (wslabel_match(type_table, port, "TAG")) 
   {
      proc->do_tag[type_index] = 1;
   }
   
   // RDS - eliminated the NFLOW_REC and the NPACKET types.
   // TODO:  need to determine whether NPACKET is a type we want to support.
   if (wsdatatype_match(type_table, input_type, "TUPLE_TYPE"))
   {
      if (!proc->outtype_tuple) 
      {
	 proc->outtype_tuple = ws_add_outtype(olist, dtype_tuple, NULL);  
      }
  
      // Are we searching only on data associated with specific labels,
      // or anywhere in the tuple?
      if (!proc->lset.len) 
      {
	 return proc_process_allstr;  // look in all strings
      }
      else 
      {
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
   if (len <= 0) 
   {
      return 0;
   }

   re2::StringPiece str((const char *)content, len);

   unsigned int mval;
   int matches = 0;
   
   for(mval = 0; mval < proc->term_vector_len; mval++) 
   {      
      if(RE2::PartialMatch(str, *(proc->term_vector[mval].pattern)))  
      {
         proc->term_vector[mval].count++;

         //i.e., is there a tuple member we are going to add to?
         if (wsd && proc->intermed_labels) 
         {
	    /*get the label associated with the number returned by Aho-Corasick;
	     * default to label_match if one is not found. */
	    wslabel_t * mlabel = proc->term_vector[mval].label;

	    if (!wsdata_check_label(wsd, mlabel)) 
	    {
	       /* this allows labels to be indexed */
	       tuple_add_member_label(tdata, /* the tuple itself */
				   wsd,   /* the tuple member to be added to */
				   mlabel /* the label to be added */);
            }
         }

         if (proc->matched_label) /* this is the -L option label */
         {
            tuple_add_member_label(tdata, wsd, proc->matched_label);
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
   if (dtype_string_buffer(member, &buf, &len)) 
   {
      found = find_match(proc, mpd_label, buf, len, tdata);
   }
   
   if (found) 
   {
      proc->hits++;
   }
   
   return found;
}


/*-----------------------------------------------------------------------------
 * reset_counts
 *
 *
 *---------------------------------------------------------------------------*/
static inline void reset_counts(proc_instance_t *proc)
{
   unsigned int i=0;

   for (i=0; i < proc->term_vector_len; i++)
   {
      proc->term_vector[i].count = 0;
   }
}

/*-----------------------------------------------------------------------------
 * add_vector
 *   Adds the vector to the tuple.
 *---------------------------------------------------------------------------*/
static inline int add_vector(void * vinstance, wsdata_t* input_data)
{
   proc_instance_t * proc = (proc_instance_t*)vinstance;
   wsdt_vector_double_t *dt;
   unsigned int i;
   
   dt = (wsdt_vector_double_t*)tuple_member_create(input_data, 
			    dtype_vector_double, 
			    proc->vector_name);

   if (!dt) 
      return 0; /* most likely the tuple is full */
   
   for (i=0; i < proc->term_vector_len; i++)
   {
      double count = proc->term_vector[i].count;
      
      if (proc->weighted_counts)
      {
	 count *= proc->term_vector[i].weight;
      }
      wsdt_vector_double_insert(dt, count);
   }
   
   return 1;
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
   for (i = 0; i < proc->lset.len; i++) 
   {
      if (tuple_find_label(input_data, proc->lset.labels[i], &mset_len,
			   &mset)) 
      {
	 /* mset - the set of members that match the specified label*/
	 for (j = 0; j < mset_len; j++ ) 
	 {
            found += member_match(proc, mset[j], mset[j], input_data);
	 }
      }
   }

   /* add the label and push to output */
   /* TODO - Perhaps I should push this code up to the inner for loop
    * above.  Then there would be a distinct vector for each matched
    * field */
   if (found || proc->do_tag[type_index])
   {
      if (proc->matched_label && found)
      {
	 wsdata_add_label(input_data, proc->matched_label);
      }
      
      if (proc->vector_name)
	 add_vector(proc, input_data);

      /* now that the vector has been read, reset the counts */
      reset_counts(proc);
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
   
   for (i = 0; i < tlen; i++) 
   {
      found = 0;
      member = tuple->member[i];
      found += member_match(proc, member, member, input_data);

      if (found || proc->do_tag[type_index])
      {
	 if (proc->matched_label && found)
	 {
	    wsdata_add_label(input_data, proc->matched_label);
	 }
	 
	 if (proc->vector_name)
	    add_vector(proc, input_data);
	 
	 /* now that the vector has been read, reset the counts */
	 reset_counts(proc);
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
   for(mval = 0; mval < proc->term_vector_len; mval++)
   {
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
 * [in] weight     - the weight for the element.
 *
 * [out] rval      - 1 if okay, 0 if error.
 *---------------------------------------------------------------------------*/
static int vectormatch_add_element(void *vinstance, void * type_table, 
				   char *restr, unsigned int matchlen,
				   char *labelstr, 
				   double weight)
{
   wslabel_t *newlab;
   unsigned int match_id;

   proc_instance_t *proc = (proc_instance_t*)vinstance;
   
   /* push everything into a vector element */
   if (proc->term_vector_len+1 < LOCAL_VECTOR_SIZE)
   {
      newlab = wsregister_label(type_table, labelstr);
      match_id = proc->term_vector_len;
      proc->term_vector[match_id].pattern = new re2::RE2(restr);
      proc->term_vector[match_id].label = newlab;
      proc->term_vector[match_id].weight = weight;
      proc->term_vector_len++;
   }
   else
   {
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


/*-----------------------------------------------------------------------------
 * vectormatch_loadfile
 *   read input match strings from input file.  This function is taken
 *   from label_match.c/label_match_loadfile, modified to allow for weights.
 *---------------------------------------------------------------------------*/
static int vectormatch_loadfile(void* vinstance, void* type_table,
				char * thefile) 
{
   FILE * fp;
   char line [2001];
   int linelen;
   char * linep;
   char * matchstr;
   int matchlen;
   char * endofstring;
   char * match_label;
   double weight;

   proc_instance_t *proc = (proc_instance_t*)vinstance;
   
   if ((fp = sysutil_config_fopen(thefile,"r")) == NULL) 
   {
      fprintf(stderr, 
	      "Error: Could not open vectormatches file %s:\n  %s\n",
	      thefile, strerror(errno));
      return 0;
   } 
   
   while (fgets(line, 2000, fp)) 
   {
      //strip return
      linelen = strlen(line);
      if (line[linelen - 1] == '\n') 
      {
	 line[linelen - 1] = '\0';
	 linelen--;
      }

      if ((linelen <= 0) || (line[0] == '#')) 
      {
	 continue;
      }

      linep = line;
      matchstr = NULL;
      match_label = NULL;
      
      // read line - exact seq
      if (linep[0] == '"') 
      {
	 linep++;
	 endofstring = (char *)rindex(linep, '"');
	 if (endofstring == NULL) 
	 {
	    continue;
	 }

	 endofstring[0] = '\0';
	 matchstr = linep;
	 matchlen = strlen(matchstr);
	 //sysutil_decode_hex_escapes(matchstr, &matchlen);
	 linep = endofstring + 1;
      }
      else 
      if (linep[0] == '{') 
      {
	 linep++;
	 endofstring = (char *)rindex(linep, '}');
	 if (endofstring == NULL) 
	 {
	    continue;
	 }
	 endofstring[0] = '\0';
	 matchstr = linep;

	 matchlen = process_hex_string(matchstr, strlen(matchstr));
	 if (!matchlen) 
	 {
	    continue;
	 }
	 
	 linep = endofstring + 1;
      }
      else 
      {
	 continue;
      }

      // Get the corresonding label 
      if (matchstr) 
      { 
	 //find (PROTO)
	 match_label = (char *) index(linep,'(');
	 endofstring = (char *) rindex(linep,')');

	 if (match_label && endofstring && (match_label < endofstring)) 
	 {
	    match_label++;
	    endofstring[0] = '\0';
	 }
	 else  
	 {
	    fprintf(stderr,
		    "Error: no label corresponding to match string '%s'\n",
		    matchstr);
            sysutil_config_fclose(fp);
	    return 0;
	 }

	 linep = endofstring + 1;
      }
      
      // Finally, get the weight if it exists
      if (match_label)
      {
	 char * weight_str;
	 char *char_ptr;

	 weight_str = strtok_r(linep, " \t", &char_ptr);
	 weight = (weight_str != NULL) ? strtof(weight_str, NULL) : 1.0;
      }
      else
	 weight = 1.0;

      if (!vectormatch_add_element(proc, type_table, matchstr, matchlen, 
				   match_label, weight))
      {
         sysutil_config_fclose(fp);
	 return 0;
      }

      tool_print("Adding entry for string '%s' label '%s' weight '%g'",
		 matchstr, match_label, weight);

   }
   sysutil_config_fclose(fp);

   return 1;
}

#ifdef __cplusplus
CPP_CLOSE
#endif

