#include "ftbool.h"

SEQFLOW ctxscan(CHARSET_INFO *cs, char *src, char *src_end, my_wc_t *dst, int *readsize, int context){
    *readsize = cs->cset->mb_wc(cs, dst, (uchar*)src, (uchar*)src_end);
    if(*readsize <= 0){
      return SF_BROKEN; // break;
    }
    if(*dst=='\0') return SF_WHITE;
    
    if(!(context & CTX_ESCAPE)){
        if(*dst=='\\') return SF_ESCAPE;
        if(context & CTX_QUOTE){
            if(*dst=='"') return SF_QUOTE_END;
            return SF_CHAR;
        }
        if(*dst=='"') return SF_QUOTE_START;
        if(*dst=='(') return SF_LEFT_PAREN;
        if(*dst==')') return SF_RIGHT_PAREN;
        if(my_isspace(cs, *src)) return SF_WHITE;
        
        if(context & CTX_CONTROL){
            if(*dst=='+') return SF_PLUS;
            if(*dst=='-') return SF_MINUS;
            if(*dst=='>') return SF_WEAK;
            if(*dst=='<') return SF_STRONG;
            if(*dst=='~') return SF_WASIGN;
        }else{
            if(*dst=='*') return SF_TRUNC;
        }
    }
    return SF_CHAR;
}
