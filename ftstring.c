#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>
#include "ftstring.h"

/**
 * FTSTRING has 3 modes:
 *  A. represent a subsequence of a given string and keep the string as-is.
 *  B. represent a subsequence of a given string and will rewrite the string if necessary.
 *  C. represent a string using internal buffer.
 */

static void ftstring_expand(FTSTRING *str, int capacity){
  if(str->buffer_length >= capacity){ return; }
  int len=1;
  while(len<capacity){
    len=len<<1;
  }
  
  if(str->buffer == NULL){
    str->buffer = my_malloc(len, MYF(MY_WME));
  }else{
		char* tmp = my_realloc(str->buffer, len, MYF(MY_WME));
		if(tmp){ str->buffer = tmp; }
  }
  str->buffer_length = len;
}

int ftstring_internal(FTSTRING *str){
  if(str->rewritable) return 0;
  if(str->start == str->buffer) return 1;
  return 0;
}

char* ftstring_head(FTSTRING *str){
  return str->start;
}

int ftstring_length(FTSTRING *str){
  return str->length;
}

void ftstring_reset(FTSTRING *str){
  str->length=0;
}

void ftstring_destroy(FTSTRING *str){
  if(str->buffer){
    my_free(str->buffer);
  }
}

void ftstring_bind(FTSTRING *str, char* src, int rewritable){
  str->start = src;
  str->length = 0;
  str->rewritable = rewritable;
}

void ftstring_unbind(FTSTRING *str){
  ftstring_expand(str, str->length);
  memcpy(str->buffer, str->start, str->length);
  str->start = str->buffer;
  str->rewritable = 0;
}

void ftstring_append(FTSTRING *str, char* src, int length){
  char* str_end = str->start + str->length;
  if(str_end == src){ // it was continuous.
    str->length += length;
    return;
  }
  // then we must rebuild the buffer.
  if(str->rewritable){
    if(str_end < src){
      memmove(str_end, src, length);
      str->length += length;
      return;
    }
    ftstring_unbind(str);
  }
  
  ftstring_expand(str, str->length+length);
  if(str->start != str->buffer){ // The first time to migrate to buffer.
    memcpy(str->buffer, str->start, str->length);
    str->start = str->buffer;
  }
  memcpy(str->start + str->length, src, length);
  str->length += length;
}

