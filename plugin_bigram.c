#include <stdlib.h>
#include <ctype.h>
#include <mysql/my_global.h>
#include <mysql/m_ctype.h>
#include <mysql/my_sys.h>

#include <mysql/plugin.h>

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
  char *end, *start, *next, *docend= param->doc + param->length;
  CHARSET_INFO *cs = param->cs;
  CHARSET_INFO *uc = get_charset(128,MYF(0)); // my_charset_ucs2_unicode_ci for unicode collation
  uint mblen=0;
  char*  buffer;
  size_t buffer_len;
  uchar* wbuffer;
  size_t wbuffer_len;
  
  // calculate mblen and malloc.
  start = param->doc;
  while(start < docend){
    uint (*mbcharlen)(CHARSET_INFO * __attribute__((unused)), uint) = cs->cset->mbcharlen;
    int step = (*mbcharlen)(cs, *(uchar*)start);
    start += step;
    if(step==0) break;
    mblen++;
  }
  buffer_len = uc->mbmaxlen * mblen;
  buffer = (char*)my_malloc(buffer_len, MYF(0));
  
  // convert into ucs2
  char* wpos=buffer;
  start = param->doc;
  while(start < docend){
    my_wc_t wc;
    my_charset_conv_mb_wc mb_wc = cs->cset->mb_wc;
    my_charset_conv_wc_mb wc_mb = uc->cset->wc_mb;
    int cnvres = 0;
    
    cnvres = (*mb_wc)(cs, &wc, (uchar*)start, docend);
    if(cnvres > 0){
      start += cnvres;
    }else if(cnvres == MY_CS_ILSEQ){
      start++;
      wc = '?';
    }else if(cnvres > MY_CS_TOOSMALL){
      start += (-cnvres);
      wc = '?';
    }else{
      break;
    }
    cnvres = (*wc_mb)(uc, wc, (uchar*)wpos, (uchar*)(buffer+buffer_len));
    if(cnvres > 0){
      wpos += cnvres;
    }else{
      break;
    }
  }
  
  // get by its weight
  wbuffer_len = uc->coll->strnxfrmlen(uc, mblen);
  wbuffer = (uchar*)my_malloc(wbuffer_len, MYF(0));
  wbuffer_len = uc->coll->strnxfrm(uc, wbuffer, wbuffer_len, buffer, buffer_len);
  // trim() because mysql binary image has padding.
  int c,mark;
  uint t_res= uc->sort_order_big[0][0x20 * uc->sort_order[0]];
  for(mark=0,c=0; c<wbuffer_len; c+=2){
    if(*(wbuffer+c) == t_res>>8 || *(wbuffer+c+1) == t_res&0xFF){
      // it is space.
    }else{
      mark = c;
    }
  }
  wbuffer_len = mark+2;
  // unit of wbuffer is always (uchar * 2)
  
  // buffer is to be free-ed
  param->flags = MYSQL_FTFLAGS_NEED_COPY;
  
  if(param->mode == MYSQL_FTPARSER_FULL_BOOLEAN_INFO){
    MYSQL_FTPARSER_BOOLEAN_INFO bool_info_must ={ FT_TOKEN_WORD, 1, 0, 0, 0, ' ', 0 };
    int pos=0;
    for(pos=0;pos<wbuffer_len-2;pos+=2){
      param->mysql_add_word(param, wbuffer + pos * sizeof(uchar), 4 * sizeof(uchar), &bool_info_must);
      param->mode = MYSQL_FTPARSER_WITH_STOPWORDS;
    }
  }else{
    int pos=0;
    for(pos=0;pos<wbuffer_len-2;pos+=2){
      param->mysql_add_word(param, wbuffer + pos * sizeof(uchar), 4 * sizeof(uchar), NULL);
    }
  }
  
  my_no_flags_free(buffer);
  my_no_flags_free(wbuffer);
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

