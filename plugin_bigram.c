#include <stdlib.h>
#include <ctype.h>
#include <mysql/my_global.h>
#include <mysql/m_ctype.h>
#include <mysql/my_sys.h>

#include <mysql/plugin.h>

#include "ftbool.h"

#if !defined(__attribute__) && (defined(__cplusplus) || !defined(__GNUC__)  || __GNUC__ == 2 && __GNUC_MINOR__ < 8)
#define __attribute__(A)
#endif

static int bigram_parser_plugin_init(void *arg __attribute__((unused)))
{
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


static int bigram_parser_parse(MYSQL_FTPARSER_PARAM *param)
{
  char *rpos, *docend= param->doc + param->length;
  CHARSET_INFO *cs = param->cs;
  CHARSET_INFO *uc = get_charset(128,MYF(0)); // my_charset_ucs2_unicode_ci for unicode collation
  uchar   ustr[2], gram_buffer[6];
  uchar*  w_buffer;
  size_t w_buffer_len;
  
  // malloc working space.
  w_buffer_len    = uc->coll->strnxfrmlen(uc, 1);
  w_buffer        = (uchar*)my_malloc(w_buffer_len, MYF(0));
  
  int qmode = param->mode;
  // buffer is to be free-ed
  param->flags = MYSQL_FTFLAGS_NEED_COPY;
  MYSQL_FTPARSER_BOOLEAN_INFO bool_info_must ={ FT_TOKEN_WORD, 1, 0, 0, 0, ' ', 0 };
  
  int context=CTX_CONTROL;
  int depth=0;
  MYSQL_FTPARSER_BOOLEAN_INFO instinfo;
  MYSQL_FTPARSER_BOOLEAN_INFO baseinfos[16];
  instinfo = baseinfos[0] = bool_info_must;
  
  int ct=0;
  int wpos=0;
  int wlen=0;
  rpos = param->doc;
  while(rpos < param->doc + param->length){
    if(wpos < wlen){
      // we can use that weight
    }else{
      int broken=1;
      while(rpos < param->doc + param->length){
        my_wc_t wc;
        int convres;
        SEQFLOW sf = ctxscan(cs, rpos, docend, &wc, &convres, context);
        if(convres > 0){
          rpos += convres;
        }else if(convres == MY_CS_ILSEQ){
          rpos++;
          wc = '?';
        }else if(convres > MY_CS_TOOSMALL){
          rpos += (-convres);
          wc = '?';
        }else{
          break;
        }
        convres = uc->cset->wc_mb(uc, wc, (uchar*)ustr, (uchar*)(ustr+2));
        if(convres <= 0){
          break;
        }
        
        if(qmode == MYSQL_FTPARSER_FULL_BOOLEAN_INFO){
          if(sf==SF_TRUNC) sf=SF_CHAR; // trunc will not be supported
          
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
            if(sf == SF_LEFT_PAR){
              instinfo = baseinfos[depth];
              depth++;
              if(depth>16) depth=16;
              baseinfos[depth] = instinfo;
              instinfo.type = FT_TOKEN_LEFT_PAREN;
              param->mysql_add_word(param, gram_buffer, 0, &instinfo);
            }
            if(sf == SF_RIGHT_PAR){
              instinfo.type = FT_TOKEN_RIGHT_PAREN;
              param->mysql_add_word(param, gram_buffer, 0, &instinfo);
              depth--;
              if(depth<0) depth=0;
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
          if(sf == SF_WHITE || sf == SF_QUOTE_END || sf == SF_LEFT_PAR || sf == SF_RIGHT_PAR){
            instinfo = baseinfos[depth];
          }
          if(sf == SF_CHAR){
            broken = 0;
            break; // emit char
          }else{
            param->mode = MYSQL_FTPARSER_FULL_BOOLEAN_INFO; // reset phrase query
            ct = 0;
          }
        }else{
          broken = 0;
          break; // emit char
        }
      }
      if(broken) break;
      
      wlen=uc->coll->strnxfrm(uc, w_buffer, w_buffer_len, ustr, (size_t)2);
      // trim() because mysql binary image has padding.
      int c,mark;
      uint t_res= uc->sort_order_big[0][0x20 * uc->sort_order[0]];
      for(mark=0,c=0; c<wlen; c+=2){
        if(*(w_buffer+c) == t_res>>8 || *(w_buffer+c+1) == t_res&0xFF){
          // it is space or padding.
        }else{
          mark = c;
        }
      }
      wlen = mark+2;
      wpos = 0;
    }
    
    if(ct%2==0){
      gram_buffer[0] = w_buffer[wpos];
      gram_buffer[1] = w_buffer[wpos+1];
      gram_buffer[4] = w_buffer[wpos];
      gram_buffer[5] = w_buffer[wpos+1];
      
      if(ct!=0){
        if(qmode == MYSQL_FTPARSER_FULL_BOOLEAN_INFO){
          param->mysql_add_word(param, gram_buffer+2, 4, &instinfo);
          param->mode = MYSQL_FTPARSER_WITH_STOPWORDS;
        }else{
          param->mysql_add_word(param, gram_buffer+2, 4, NULL);
        }
      }
    }else{
      gram_buffer[2] = w_buffer[wpos];
      gram_buffer[3] = w_buffer[wpos+1];
      
      if(qmode == MYSQL_FTPARSER_FULL_BOOLEAN_INFO){
        param->mysql_add_word(param, gram_buffer, 4, &instinfo);
        param->mode = MYSQL_FTPARSER_WITH_STOPWORDS;
      }else{
        param->mysql_add_word(param, gram_buffer, 4, NULL);
      }
    }
    ct++;
    wpos+=2;
  }
  
  my_no_flags_free(w_buffer);
  return(0);
}


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
  0x0001,                     /* version                         */
  NULL,                       /* status variables                */
  NULL,                       /* system variables                */
  NULL
}
mysql_declare_plugin_end;

