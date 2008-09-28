#include <my_global.h>
#include <my_sys.h>
#if HAVE_ICU
#include <unicode/unorm.h>
#include <unicode/ustring.h>

/**
 * @param mode same with ICU mode flag UNormalizationMode.
 */
char* uni_normalize(char* src, size_t src_len, char* dst, size_t dst_capacity, size_t *dst_used, int mode, int options, int *status){
    *status = 0;
    UNormalizationMode umode = (UNormalizationMode)mode;
    // status holder
    UErrorCode ustatus=0;
    // UChar source
    UChar *s;
    int32_t s_alloc;
    int32_t s_len;
    // UChar normalized
    UChar *d;
    int32_t d_alloc;
    int32_t d_len;
    // UTF8 normalzied
    char *dst_new;
    int32_t dst_alloc;
    int32_t dst_len;
    
    // init
    *dst_used = 0;
    // convert UTF-8 -> UChar
    s_alloc = (int32_t)src_len;
    s = (UChar*)my_malloc(sizeof(UChar)*s_alloc, MYF(MY_WME));
    if(!s){
      *status=-1;
      return dst;
    }
    ustatus = 0;
    s = u_strFromUTF8(s, s_alloc, &s_len, src, (int32_t)src_len, &ustatus);
    if(s_alloc < s_len || U_FAILURE(ustatus)){
      s_alloc = s_len;
      s = (UChar*)my_realloc(s, sizeof(UChar)*s_alloc, MYF(MY_WME));
      if(s){
        ustatus = 0;
        s = u_strFromUTF8(s, s_alloc, &s_len, src, (int32_t)src_len, &ustatus);
        if(U_FAILURE(ustatus)) *status = ustatus;
      }else{
        *status = -3;
      }
    }
    if(*status != 0){
        my_free(s, MYF(0));
        return dst;
    }
    // normalize
    d_alloc = s_len + 32;
    d = (UChar*)my_malloc(sizeof(UChar)*d_alloc, MYF(MY_WME));
    if(!d){
      *status = -5;
      return dst;
    }
    d_len = unorm_normalize(s, s_len, umode, options, d, d_alloc, &ustatus);
    while(ustatus == U_BUFFER_OVERFLOW_ERROR){
        d_alloc = d_alloc*2;
        d = (UChar*)my_realloc(d, sizeof(UChar)*d_alloc, MYF(MY_WME));
        if(!d){
            *status = -6;
            break;
        }
        ustatus = 0;
        d_len = unorm_normalize(s, s_len, mode, options, d, d_alloc, &ustatus);
    }
    if(U_FAILURE(ustatus)){
      *status = ustatus;
      return dst;
    }
    my_free(s, MYF(0));
    if(*status != 0){
        my_free(d, MYF(0));
        return dst;
    }
    // encode UChar -> UTF-8
    dst_alloc = (int32_t)dst_capacity;
    dst_new = u_strToUTF8(dst, dst_alloc, &dst_len, d, d_len, &ustatus);
    my_free(d, MYF(0));
    if(dst_used) *dst_used = d_len;
    if(ustatus == U_BUFFER_OVERFLOW_ERROR){
        *status = -8;
    }
    *status=ustatus;
    if(dst_new) return dst_new;
    return dst;
}
#endif
