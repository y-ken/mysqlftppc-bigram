#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "ftbool.h"
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
#include <plugin.h>

#if !defined(__attribute__) && (defined(__cplusplus) || !defined(__GNUC__)  || __GNUC__ == 2 && __GNUC_MINOR__ < 8)
#define __attribute__(A)
#endif

static char* bigram_unicode_normalize;
static char* bigram_unicode_version;
static char  bigram_info[128];

static void* icu_malloc(const void* context, size_t size){ return my_malloc(size,MYF(MY_WME)); }
static void* icu_realloc(const void* context, void* ptr, size_t size){ return my_realloc(ptr,size,MYF(MY_WME)); }
static void  icu_free(const void* context, void *ptr){ my_free(ptr,MYF(0)); }

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
  return(0);
}
static int bigram_parser_deinit(MYSQL_FTPARSER_PARAM *param __attribute__((unused)))
{
  return(0);
}

static size_t str_convert(CHARSET_INFO *cs, char *from, size_t from_length,
                          CHARSET_INFO *uc, char *to,   size_t to_length){
  char *rpos, *rend, *wpos, *wend;
  my_wc_t wc;
  
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
    cnvres = uc->cset->wc_mb(uc, wc, (uchar*)wpos, (uchar*)wend);
    if(cnvres > 0){
      wpos += cnvres;
    }else{
      break;
    }
  }
  return (size_t)(wpos - to);
}

static int bigram_add_gram(MYSQL_FTPARSER_PARAM *param, MYSQL_FTPARSER_BOOLEAN_INFO *boolean_info, CHARSET_INFO *cs, my_wc_t* gram_wc, int ct){
  int convres = 0;
  int step = 0;
  uchar w_buffer[32]; // 2-gram buffer can't be longer than 32.
  int w_buffer_len=32;
  
  if(ct==-1 || ct==-2){
    // ab
    convres = cs->cset->wc_mb(cs, gram_wc[0], w_buffer, w_buffer+w_buffer_len);
    convres = cs->cset->wc_mb(cs, gram_wc[1], w_buffer+convres, w_buffer+w_buffer_len) + convres;
    if(convres>0) param->mysql_add_word(param, (char*)w_buffer, convres, boolean_info);
  }
  if(ct==-2){
    convres = cs->cset->wc_mb(cs, gram_wc[1], w_buffer, w_buffer+w_buffer_len);
    convres = cs->cset->wc_mb(cs, gram_wc[2], w_buffer+convres, w_buffer+w_buffer_len) + convres;
    if(convres>0) param->mysql_add_word(param, (char*)w_buffer, convres, boolean_info);
  }
  if(ct<-2){
    if(boolean_info->quot && param->mode == MYSQL_FTPARSER_FULL_BOOLEAN_INFO){
      boolean_info->type = FT_TOKEN_RIGHT_PAREN;
      param->mysql_add_word(param, (char*)w_buffer, 0, boolean_info); // push LEFT_PAREN token
      boolean_info->type = FT_TOKEN_WORD;
      boolean_info->quot = NULL;
      step = -1;
    }
  }
  if(ct==0 || ct==1) step = 0;
  if(ct==2){
    // push quote
    if(param->mode == MYSQL_FTPARSER_FULL_BOOLEAN_INFO){
      boolean_info->quot = (char*)1;
      boolean_info->type = FT_TOKEN_LEFT_PAREN;
      param->mysql_add_word(param, (char*)w_buffer, 0, boolean_info); // push LEFT_PAREN token
      boolean_info->type = FT_TOKEN_WORD;
    }
    // ab
    convres = cs->cset->wc_mb(cs, gram_wc[0], w_buffer, w_buffer+w_buffer_len);
    convres = cs->cset->wc_mb(cs, gram_wc[1], w_buffer+convres, w_buffer+w_buffer_len) + convres;
    if(convres>0) param->mysql_add_word(param, (char*)w_buffer, convres, boolean_info);
    // bc
    convres = cs->cset->wc_mb(cs, gram_wc[1], w_buffer, w_buffer+w_buffer_len);
    convres = cs->cset->wc_mb(cs, gram_wc[2], w_buffer+convres, w_buffer+w_buffer_len) + convres;
    if(convres>0) param->mysql_add_word(param, (char*)w_buffer, convres, boolean_info);
    step = 1;
  }
  if(ct>2){
    convres = cs->cset->wc_mb(cs, gram_wc[ct%2+1], w_buffer, w_buffer+w_buffer_len);
    convres = cs->cset->wc_mb(cs, gram_wc[ct%2+2], w_buffer+convres, w_buffer+w_buffer_len) + convres;
    if(convres>0) param->mysql_add_word(param, (char*)w_buffer, convres, boolean_info);
  }
  return step;
}

static int bigram_parser_parse(MYSQL_FTPARSER_PARAM *param)
{
  DBUG_ENTER("bigram_parser_parse");

  CHARSET_INFO *uc = NULL; // if the sequence changed to UTF-8, it is not null
  CHARSET_INFO *cs = param->cs;
  char* feed = param->doc;
  size_t feed_length = (size_t)param->length;
  int feed_req_free = 0;
  
  if(strcmp("utf8", cs->csname)!=0 && bigram_unicode_normalize && strcmp(bigram_unicode_normalize, "OFF")!=0){
    uc = get_charset(33,MYF(0)); // my_charset_utf8_general_ci for utf8 conversion
  }
  
  // convert into UTF-8
  if(uc){
    // calculate mblen and malloc.
    int cv_length = uc->mbmaxlen * cs->cset->numchars(cs, feed, feed+feed_length);
    char* cv = my_malloc(cv_length, MYF(MY_WME));
    feed_length = str_convert(cs, feed, feed_length, uc, cv, cv_length);
    feed = cv;
    feed_req_free = 1;
  }
  
#if HAVE_ICU
  // normalize
  if(bigram_unicode_normalize && strcmp(bigram_unicode_normalize, "OFF")!=0){
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
    t = uni_normalize(feed, feed_length, nm, nm_length, &nm_used, mode, options, &status);
    if(status != 0){
      nm_length=nm_used;
      nm = my_realloc(nm, nm_length, MYF(MY_WME));
      t = uni_normalize(feed, feed_length, nm, nm_length, &nm_used, mode, options, &status);
      if(status != 0){
        fputs("unicode normalization failed.\n",stderr);
        fflush(stderr);
      }else{
        nm = t;
      }
    }else{
      nm = t;
    }
    feed_length = nm_used;
    if(feed_req_free) my_free(feed,MYF(0));
    feed = nm;
    feed_req_free = 1;
  }
#endif
  
  // setup params
  param->flags |= MYSQL_FTFLAGS_NEED_COPY; // buffer is to be free-ed
  
  int context=CTX_CONTROL;
  int depth=0;
  MYSQL_FTPARSER_BOOLEAN_INFO bool_info_may ={ FT_TOKEN_WORD, 0, 0, 0, 0, ' ', 0 };
  MYSQL_FTPARSER_BOOLEAN_INFO instinfo;
  MYSQL_FTPARSER_BOOLEAN_INFO baseinfos[16];
  instinfo = baseinfos[0] = bool_info_may;
  
  // working buffers:
  my_wc_t gram_wc[4];
  int step;
  
  int ct=0;
  char* pos = feed;
  char* docend = feed + feed_length;
  while(pos < docend){
    int convres;
    my_wc_t wc;
    int broken=1;
    while(pos < docend){
      SEQFLOW sf;
      if(uc){
        sf = ctxscan(uc, pos, docend, &wc, &convres, context);
      }else{
        sf = ctxscan(cs, pos, docend, &wc, &convres, context);
      }
      if(convres > 0){
        pos += convres;
      }else if(convres == MY_CS_ILSEQ){
        pos++;
        wc = '?';
      }else{
        break;
      }
      
      if(param->mode == MYSQL_FTPARSER_FULL_BOOLEAN_INFO){
        if(sf==SF_ESCAPE){
          context |= CTX_ESCAPE;  // ESCAPE ON
          context |= CTX_CONTROL; // CONTROL ON
        }else{
          context &= ~CTX_ESCAPE; // ESCAPE OFF
          if(sf == SF_CHAR){
            context &= ~CTX_CONTROL; // CONTROL OFF
          }else{
            context |= CTX_CONTROL; // CONTROL ON
          }
          
          if(sf == SF_QUOTE_START) context |= CTX_QUOTE;
          if(sf == SF_QUOTE_END)   context &= ~CTX_QUOTE;
          if(sf == SF_LEFT_PAREN){
            depth++;
            if(depth>16) depth=16;
            baseinfos[depth] = instinfo;
            instinfo.type = FT_TOKEN_LEFT_PAREN;
            param->mysql_add_word(param, pos, 0, &instinfo); // push LEFT_PAREN token
            instinfo = baseinfos[depth];
          }
          if(sf == SF_RIGHT_PAREN){
            instinfo.type = FT_TOKEN_RIGHT_PAREN;
            param->mysql_add_word(param, pos, 0, &instinfo); // push RIGHT_PAREN token
            depth--;
            if(depth<0) depth=0;
            instinfo = baseinfos[depth];
          }
          if(sf == SF_PLUS){
            instinfo.yesno = 1;
          }
          if(sf == SF_MINUS){
            instinfo.yesno = -1;
          }
          if(sf == SF_PLUS) instinfo.weight_adjust = 1;
          if(sf == SF_MINUS) instinfo.weight_adjust = -1;
          if(sf == SF_WASIGN){
            instinfo.wasign = -1;
          }
        }
        if(sf == SF_CHAR){
          broken = 0;
          break; // emit char
        }else if(sf != SF_ESCAPE){
          step = bigram_add_gram(param, &instinfo, cs, gram_wc, -ct); // the gram sequece end.
          depth+=step;
          if(depth<0) depth=0;
          if(step!=0){
            instinfo = baseinfos[depth];
          }
          ct=0;
        }
        if(sf == SF_WHITE || sf == SF_QUOTE_END || sf == SF_LEFT_PAREN || sf == SF_RIGHT_PAREN){
          instinfo = baseinfos[depth];
        }
      }else{
        broken = 0;
        break; // emit char
      }
    }
    if(broken) break;
    
    if(ct%2==0){
      if(ct==0) gram_wc[0] = wc;
      gram_wc[2] = wc;
    }else{
      gram_wc[1] = wc;
      gram_wc[3] = wc;
    }
    step = bigram_add_gram(param, &instinfo, cs, gram_wc, ct);
    depth+=step;
    if(depth>16) depth=16;
    if(step > 0) baseinfos[depth] = instinfo;
    ct++;
  }
  if(param->mode==MYSQL_FTPARSER_FULL_BOOLEAN_INFO) bigram_add_gram(param, &instinfo, cs, gram_wc, -ct);
  
  if(feed_req_free) my_free(feed, MYF(0));
  DBUG_RETURN(0);
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
    if(!get_charset(33,MYF(0))) return -1; // If you don't have utf8 codec in mysql, it fails
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
  0x0015,                     /* version                         */
  bigram_status,              /* status variables                */
  bigram_system_variables,    /* system variables                */
  NULL
}
mysql_declare_plugin_end;

