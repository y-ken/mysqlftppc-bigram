#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "ftbool.h"
#include "ftstring.h"
#if HAVE_ICU
#include <unicode/uclean.h>
#include <unicode/uversion.h>
#include <unicode/uchar.h>
#include <unicode/unorm.h>
#include "ftnorm.h"
#endif

// mysql headers
#include <my_global.h>
#include <m_ctype.h>
#include <my_sys.h>
#include <my_list.h>
#include <plugin.h>

#define HA_FT_MAXBYTELEN 254
#define FTPPC_MEMORY_ERROR -1
#define FTPPC_NORMALIZATION_ERROR -2
#define FTPPC_SYNTAX_ERROR -3

#if !defined(__attribute__) && (defined(__cplusplus) || !defined(__GNUC__)  || __GNUC__ == 2 && __GNUC_MINOR__ < 8)
#define __attribute__(A)
#endif

static char* bigram_unicode_normalize;
static char* bigram_unicode_version;
static char  bigram_info[128];

static void* icu_malloc(const void* context, size_t size){ return my_malloc(size,MYF(MY_WME)); }
static void* icu_realloc(const void* context, void* ptr, size_t size){ return my_realloc(ptr,size,MYF(MY_WME)); }
static void  icu_free(const void* context, void *ptr){ my_free(ptr); }


/** ftstate */
static LIST* list_top(LIST* root){
  LIST *cur = root;
  while(cur && cur->next){
    cur = cur->next;
  }
  return cur;
}

struct ftppc_mem_bulk {
  char*  mem_head;
  char*  mem_cur;
  size_t mem_size;
};

struct ftppc_state {
  /** immutable memory buffer */
  size_t bulksize;
  LIST*  mem_root;
  void*  engine;
  CHARSET_INFO* engine_charset;
};

static void* ftppc_alloc(struct ftppc_state *state, size_t length){
  LIST *cur = list_top(state->mem_root);
  while(1){
    if(!cur){
      // hit the root. create a new bulk.
      size_t sz = state->bulksize<<1;
      while(sz < length){
        sz = sz<<1;
      }
      state->bulksize = sz;
      
      struct ftppc_mem_bulk *tmp = (struct ftppc_mem_bulk*)my_malloc(sizeof(struct ftppc_mem_bulk), MYF(MY_WME));
      tmp->mem_head = my_malloc(sz, MYF(MY_WME));
      tmp->mem_cur  = tmp->mem_head;
      tmp->mem_size = sz;
      
      if(!tmp->mem_head){ return NULL; }
      
      state->mem_root = list_cons(tmp, cur);
      cur = state->mem_root;
    }
    
    struct ftppc_mem_bulk *bulk = (struct ftppc_mem_bulk*)cur->data;
    
    if(bulk->mem_cur + length < bulk->mem_head + bulk->mem_size){
      void* addr = bulk->mem_cur;
      bulk->mem_cur += length;
      return addr;
    }
    cur = cur->prev;
  }
}
/** /ftstate */


static int bigram_parser_plugin_init(void *arg __attribute__((unused)))
{
  bigram_info[0]='\0';
#if HAVE_ICU
  strcat(bigram_info, "with ICU ");
  char icu_tmp_str[16];
  char errstr[128];
  UVersionInfo versionInfo;
  u_getVersion(versionInfo); // get ICU version
  u_versionToString(versionInfo, icu_tmp_str);
  strcat(bigram_info, icu_tmp_str);
  u_getUnicodeVersion(versionInfo); // get ICU Unicode version
  u_versionToString(versionInfo, icu_tmp_str);
  strcat(bigram_info, "(Unicode ");
  strcat(bigram_info, icu_tmp_str);
  strcat(bigram_info, ")");
  
  UErrorCode ustatus=0;
  u_setMemoryFunctions(NULL, icu_malloc, icu_realloc, icu_free, &ustatus);
  if(U_FAILURE(ustatus)){
    sprintf(errstr, "u_setMemoryFunctions failed. ICU status code %d\n", ustatus);
    fputs(errstr, stderr);
    fflush(stderr);
  }
#else
  strcat(bigram_info, ", without ICU");
#endif
  return(0);
}
static int bigram_parser_plugin_deinit(void *arg __attribute__((unused)))
{
  return(0);
}


static int bigram_parser_init(MYSQL_FTPARSER_PARAM *param __attribute__((unused)))
{
  struct ftppc_state *state = (struct ftppc_state*)my_malloc(sizeof(struct ftppc_state), MYF(MY_WME));
  state->bulksize = 8;
  state->mem_root = NULL;
  param->ftparser_state = state;
  
  return(0);
}
static int bigram_parser_deinit(MYSQL_FTPARSER_PARAM *param __attribute__((unused)))
{
  struct ftppc_state *state = (struct ftppc_state*)param->ftparser_state;
  list_free(state->mem_root, 1);
  
  return(0);
}

static size_t str_convert(CHARSET_INFO *cs, char *from, size_t from_length,
                          CHARSET_INFO *uc, char *to,   size_t to_length,
                          size_t *numchars){
  char *rpos, *rend, *wpos, *wend;
  my_wc_t wc;
  char* tmp = NULL;
  
  if(numchars){ *numchars = 0; }
  rpos = from;
  rend = from + from_length;
  wpos = to;
  wend = to + to_length;
  while(rpos < rend){
    int cnvres = 0;
    cnvres = cs->cset->mb_wc(cs, &wc, (uchar*)rpos, (uchar*)rend);
    if(cnvres > 0){
      rpos += cnvres;
    }else if(cnvres == MY_CS_ILSEQ){
      rpos++;
      wc = '?';
    }else{
      break;
    }
    if(!to){
      if(!tmp){ tmp=my_malloc(uc->mbmaxlen, MYF(MY_WME)); }
      cnvres = uc->cset->wc_mb(uc, wc, (uchar*)tmp, (uchar*)(tmp+uc->mbmaxlen));
    }else{
      cnvres = uc->cset->wc_mb(uc, wc, (uchar*)wpos, (uchar*)wend);
    }
    if(cnvres > 0){
      wpos += (size_t)cnvres;
    }else{
      break;
    }
    if(numchars){ *numchars++; }
  }
  if(tmp){ my_free(tmp); }
  return (size_t)(wpos-to);
}


static int bigram_add_word(MYSQL_FTPARSER_PARAM *param, FTSTRING *pbuffer, CHARSET_INFO *cs, MYSQL_FTPARSER_BOOLEAN_INFO *instinfo){
  if(ftstring_length(pbuffer)<=0){ return 0; }
  
  size_t tlen = ftstring_length(pbuffer);
  
  my_wc_t wc;
  char *src  = ftstring_head(pbuffer);
  char *srce = src + tlen;
  char *dst  = NULL;
  char *dste = NULL;
  
  int quote = 0;
  int save_transcode = 0;
  if(strcmp(cs->csname, param->cs->csname)!=0){
    size_t nums;
    tlen = str_convert(cs, ftstring_head(pbuffer), ftstring_length(pbuffer), param->cs, NULL, 0, &nums);
    save_transcode = 1;
    if(nums > 2){ quote = 1; }
  }else{
    if(cs->cset->numchars(cs, src, srce) > 2){ quote = 1; }
  }
  if(save_transcode || ftstring_internal(pbuffer) || pbuffer->rewritable){
    dst = (char*)ftppc_alloc((struct ftppc_state*)param->ftparser_state, tlen);
    if(!dst){ return FTPPC_MEMORY_ERROR; }
    dste = dst + tlen;
  }
  
  if(quote && instinfo){
    instinfo->quot = (char*)1;
    MYSQL_FTPARSER_BOOLEAN_INFO tmp = *instinfo;
    tmp.type = FT_TOKEN_LEFT_PAREN;
    param->mysql_add_word(param, dst, 0, &tmp); // push LEFT_PAREN token
  }
  
  char *prev = NULL;
  int wct = 0;
  while(src < srce){
    int cnvres = 0;
    char* t = src;
    
    cnvres = cs->cset->mb_wc(cs, &wc, (uchar*)src, (uchar*)srce);
    if(cnvres > 0){
      src += cnvres;
    }else if(cnvres == MY_CS_ILSEQ){
      src++;
    }else{
      break;
    }
    
    if(dst){
      cnvres = param->cs->cset->wc_mb(param->cs, wc, (uchar*)dst, (uchar*)dste);
      if(cnvres > 0){
        t = dst;
        dst += cnvres;
        if(prev){
          param->mysql_add_word(param, prev, dst-prev, instinfo);
        }
        prev = t;
      }else{
        break;
      }
    }else{
      if(prev){
        param->mysql_add_word(param, prev, src - prev, instinfo);
      }
      prev = t;
    }
    wct++;
  }
  
  if(quote && instinfo){
    instinfo->quot = (char*)1;
    MYSQL_FTPARSER_BOOLEAN_INFO tmp = *instinfo;
    tmp.type = FT_TOKEN_RIGHT_PAREN;
    param->mysql_add_word(param, dst, 0, &tmp); // push LEFT_PAREN token
  }
  return 0;
}


static int bigram_parser_parse(MYSQL_FTPARSER_PARAM *param)
{
  char* feed = param->doc;
  size_t feed_length = (size_t)param->length;
  int feed_req_free = 0;
  CHARSET_INFO *cs = param->cs;
  
#if HAVE_ICU
  // normalize
  if(bigram_unicode_normalize && strcmp(bigram_unicode_normalize, "OFF")!=0){
    if(strcmp("utf8", cs->csname)!=0){
      // convert into UTF-8
      CHARSET_INFO *uc = get_charset(33, MYF(0)); // my_charset_utf8_general_ci for utf8 conversion
      // calculate mblen and malloc.
      int cv_length = uc->mbmaxlen * cs->cset->numchars(cs, feed, feed+feed_length);
      char* cv = my_malloc(cv_length, MYF(MY_WME));
      feed_length = str_convert(cs, feed, feed_length, uc, cv, cv_length, NULL);
      feed = cv;
      feed_req_free = 1;
      cs = uc;
    }
    char* nm;
    char* t;
    size_t nm_length=0;
    size_t nm_used=0;
    nm_length = feed_length+32;
    nm = my_malloc(nm_length, MYF(MY_WME));
    int status = 0;
    int mode = 1;
    int options = 0;
    if(strcmp(bigram_unicode_normalize, "C")==0) mode = UNORM_NFC;
    if(strcmp(bigram_unicode_normalize, "D")==0) mode = UNORM_NFD;
    if(strcmp(bigram_unicode_normalize, "KC")==0) mode = UNORM_NFKC;
    if(strcmp(bigram_unicode_normalize, "KD")==0) mode = UNORM_NFKD;
    if(strcmp(bigram_unicode_normalize, "FCD")==0) mode = UNORM_FCD;
    if(bigram_unicode_version && strcmp(bigram_unicode_version, "3.2")==0) options |= UNORM_UNICODE_3_2;
    if(feed_length > 0){
      nm_used = uni_normalize(feed, feed_length, nm, nm_length, mode, options);
      if(nm_used == 0){
        fputs("unicode normalization failed.\n",stderr);
        fflush(stderr);
        
        if(feed_req_free){ my_free(feed); }
        return FTPPC_NORMALIZATION_ERROR;
      }else if(nm_used > nm_length){
        nm_length = nm_used + 8;
        char *tmp = my_realloc(nm, nm_length, MYF(MY_WME));
        if(tmp){
          nm = tmp;
        }else{
          if(feed_req_free){ my_free(feed); }
          my_free(nm);
          return FTPPC_MEMORY_ERROR;
        }
        nm_used = uni_normalize(feed, feed_length, nm, nm_length, mode, options);
        if(nm_used == 0){
          fputs("unicode normalization failed.\n",stderr);
          fflush(stderr);
          
          if(feed_req_free){ my_free(feed); }
          my_free(nm);
          return FTPPC_NORMALIZATION_ERROR;
        }
      }
      if(feed_req_free){ my_free(feed); }
      feed = nm;
      feed_length = nm_used;
      feed_req_free = 1;
    }
  }
#endif
  
  FTSTRING buffer = { NULL, 0, NULL, 0, 0 };
  FTSTRING *pbuffer = &buffer;
  ftstring_bind(pbuffer, feed, feed_req_free);
  
  if(param->mode == MYSQL_FTPARSER_FULL_BOOLEAN_INFO){
    MYSQL_FTPARSER_BOOLEAN_INFO instinfo ={ FT_TOKEN_WORD, 0, 0, 0, 0, ' ', 0 };
    MYSQL_FTPARSER_BOOLEAN_INFO *info_may = (MYSQL_FTPARSER_BOOLEAN_INFO*)my_malloc(sizeof(MYSQL_FTPARSER_BOOLEAN_INFO), MYF(MY_WME));
    *info_may = instinfo;
    LIST *infos = NULL;
    list_push(infos, info_may);
    
    int context=CTX_CONTROL;
    SEQFLOW sf,sf_prev = SF_BROKEN;
    
    size_t prev_token_length = 0;
    char*  prev_token = NULL;
    int    prev_need_realloc = 0;
    
    char* pos = feed;
    char* docend = feed + feed_length;
    while(pos < docend){
      int readsize;
      my_wc_t dst;
      sf = ctxscan(cs, pos, docend, &dst, &readsize, context);
      
      if(sf==SF_ESCAPE){
        context |= CTX_ESCAPE;
        context |= CTX_CONTROL;
      }else{
        context &= ~CTX_ESCAPE;
        if(sf == SF_CHAR){
          context &= ~CTX_CONTROL;
        }else{
          context |= CTX_CONTROL;
        }
      }
      if(sf == SF_TRUNC){
        sf = SF_CHAR;
      }
      if(sf == SF_PLUS){   instinfo.yesno = 1; }
      if(sf == SF_MINUS){  instinfo.yesno = -1; }
      if(sf == SF_STRONG){ instinfo.weight_adjust++; }
      if(sf == SF_WEAK){   instinfo.weight_adjust--; }
      if(sf == SF_WASIGN){ instinfo.wasign = !instinfo.wasign; }
      if(sf == SF_LEFT_PAREN){
        MYSQL_FTPARSER_BOOLEAN_INFO *tmp = (MYSQL_FTPARSER_BOOLEAN_INFO*)my_malloc(sizeof(MYSQL_FTPARSER_BOOLEAN_INFO), MYF(MY_WME));
        *tmp = instinfo;
        list_push(infos, tmp);
      
        instinfo.type = FT_TOKEN_LEFT_PAREN;
        param->mysql_add_word(param, pos, 0, &instinfo); // push LEFT_PAREN token
        instinfo = *tmp;
      }
      if(sf == SF_QUOTE_START){ // in bigram, multiple chained tokens are always phrase. we don't need quote symbol.
        context |= CTX_QUOTE;
      }
      if(sf == SF_RIGHT_PAREN){
        instinfo = *((MYSQL_FTPARSER_BOOLEAN_INFO*)infos->data);
        instinfo.type = FT_TOKEN_RIGHT_PAREN;
        param->mysql_add_word(param, pos, 0, &instinfo); // push RIGHT_PAREN token
      
        MYSQL_FTPARSER_BOOLEAN_INFO *tmp = (MYSQL_FTPARSER_BOOLEAN_INFO*)infos->data;
        if(tmp){ my_free(tmp); }
        list_pop(infos);
        if(!infos){
          return FTPPC_SYNTAX_ERROR;
        } // must not reach the base info_may level.
        instinfo = *((MYSQL_FTPARSER_BOOLEAN_INFO*)infos->data);
      }
      if(sf == SF_QUOTE_END){ // in bigram, multiple chained tokens are always phrase. we don't need quote symbol.
        context &= ~CTX_QUOTE;
      }
      if(sf == SF_CHAR){
        ftstring_append(pbuffer, pos, readsize);
      }else{
        if(ftstring_length(pbuffer)>0){
          bigram_add_word(param, pbuffer, cs, &instinfo);
          ftstring_reset(pbuffer);
        }
      }
      
      if(readsize > 0){
        pos += readsize;
      }else if(readsize == MY_CS_ILSEQ){
        pos++;
      }else{
        break;
      }
      sf_prev = sf;
    }
    if(ftstring_length(pbuffer) > 0){
      bigram_add_word(param, pbuffer, cs, &instinfo);
    }
    list_free(infos, 1);
  }else{
    ftstring_append(pbuffer, feed, feed_length);
    bigram_add_word(param, pbuffer, cs, NULL);
  }
  ftstring_destroy(pbuffer);
  if(feed_req_free){ my_free(feed); }
  return 0;
}

int bigram_unicode_version_check(MYSQL_THD thd, struct st_mysql_sys_var *var, void *save, struct st_mysql_value *value){
    char buf[4];
    int len=4;
    const char *str;
    
    str = value->val_str(value,buf,&len);
    if(!str) return -1;
    *(const char**)save=str;
    if(len==3){
      if(memcmp(str, "3.2", len)==0) return 0;
    }
    if(len==7){
      if(memcmp(str, "DEFAULT", len)==0) return 0;
    }
    return -1;
}

int bigram_unicode_normalize_check(MYSQL_THD thd, struct st_mysql_sys_var *var, void *save, struct st_mysql_value *value){
    char buf[4];
    int len=4;
    const char *str;
    
    str = value->val_str(value,buf,&len);
    if(!str) return -1;
    *(const char**)save=str;
    if(!get_charset(33, MYF(0))) return -1; // If you don't have utf8 codec in mysql, it fails
    if(len==1){
        if(str[0]=='C'){ return 0;}
        if(str[0]=='D'){ return 0;}
    }
    if(len==2){
        if(str[0]=='K' && str[1]=='C'){ return 0;}
        if(str[0]=='K' && str[1]=='D'){ return 0;}
    }
    if(len==3){
        if(str[0]=='F' && str[1]=='C' && str[2]=='D'){ return 0;}
        if(str[0]=='O' && str[1]=='F' && str[2]=='F'){ return 0;}
    }
    return -1;
}

static MYSQL_SYSVAR_STR(normalization, bigram_unicode_normalize,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_MEMALLOC,
  "Set unicode normalization (OFF, C, D, KC, KD, FCD)",
  bigram_unicode_normalize_check, NULL, "OFF");

static MYSQL_SYSVAR_STR(unicode_version, bigram_unicode_version,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_MEMALLOC,
  "Set unicode version (3.2, DEFAULT)",
  bigram_unicode_version_check, NULL, "DEFAULT");

static struct st_mysql_show_var bigram_status[]=
{
  {"Bigram_info", (char *)bigram_info, SHOW_CHAR},
  {0,0,0}
};

static struct st_mysql_sys_var* bigram_system_variables[]= {
#if HAVE_ICU
  MYSQL_SYSVAR(normalization),
  MYSQL_SYSVAR(unicode_version),
#endif
  NULL
};

static struct st_mysql_ftparser bigram_parser_descriptor=
{
  MYSQL_FTPARSER_INTERFACE_VERSION, /* interface version      */
  bigram_parser_parse,              /* parsing function       */
  bigram_parser_init,               /* parser init function   */
  bigram_parser_deinit              /* parser deinit function */
};

mysql_declare_plugin(ft_bigram)
{
  MYSQL_FTPARSER_PLUGIN,      /* type                            */
  &bigram_parser_descriptor,  /* descriptor                      */
  "bigram",                   /* name                            */
  "Hiroaki Kawai",            /* author                          */
  "Bi-gram Full-Text Parser", /* description                     */
  PLUGIN_LICENSE_BSD,
  bigram_parser_plugin_init,  /* init function (when loaded)     */
  bigram_parser_plugin_deinit,/* deinit function (when unloaded) */
  0x0106,                     /* version                         */
  bigram_status,              /* status variables                */
  bigram_system_variables,    /* system variables                */
  NULL
}
mysql_declare_plugin_end;

