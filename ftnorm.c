#if HAVE_ICU
#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>

#include <unicode/unorm.h>
#include <unicode/ustring.h>

#include "ftnorm.h"

/**
 * @param mode same with ICU mode flag UNormalizationMode.
 */
size_t uni_normalize(char* src, size_t src_len, char* dst, size_t dst_capacity, int mode, int opt){
    UNormalizationMode umode = (UNormalizationMode)mode;
    // status holder
    UErrorCode ustatus=0;
    // UChar source
    UChar *s;
    int32_t s_alloc;
    // UChar normalized
    UChar *d;
    int32_t d_alloc;
    int32_t d_len;
    // UTF8 normalzied
    int32_t dst_alloc;
    // realloc check
		UChar *tmp;
		
    // convert UTF-8 -> UChar
    s_alloc = (int32_t)src_len*2;
    s = (UChar*)my_malloc(s_alloc, MYF(MY_WME));
    if(!s){
			return 0;
    }
    ustatus = 0;
    s = u_strFromUTF8(s, s_alloc, NULL, src, (int32_t)src_len, &ustatus);
    if(U_FAILURE(ustatus)){
			ustatus = 0;
			s_alloc = (size_t)u_strFromUTF8(NULL, 0, NULL, src, (int32_t)src_len, &ustatus) + 4; // prefill
      tmp = (UChar*)my_realloc(s, s_alloc, MYF(MY_WME));
      if(!tmp){
				my_free(s, MYF(0));
				return 0;
			}else{
				s=tmp;
			}
      ustatus = 0;
      s = u_strFromUTF8(s, s_alloc, NULL, src, (int32_t)src_len, &ustatus);
      if(U_FAILURE(ustatus)){
				my_free(s, MYF(0));
				return 0;
			}
    }
    // normalize
    d_alloc = strlen((char*)s) + 32;
    d = (UChar*)my_malloc(d_alloc, MYF(MY_WME));
    if(!d){
			return 0;
    }
    d_len = unorm_normalize(s, -1, umode, (int32_t)opt, d, d_alloc, &ustatus);
    if(ustatus == U_BUFFER_OVERFLOW_ERROR){
				d_alloc = d_len + 8;
        tmp = (UChar*)my_realloc(d, d_alloc, MYF(MY_WME));
        if(!tmp){
						my_free(d, MYF(0));
						return 0;
        }else{
					d=tmp;
				}
        ustatus = 0;
        d_len = unorm_normalize(s, -1, mode, (int32_t)opt, d, d_alloc, &ustatus);
    }
    my_free(s, MYF(0));
    // encode UChar -> UTF-8
    u_strToUTF8(dst, (int32_t)dst_capacity, NULL, d, d_len, &ustatus);
    my_free(d, MYF(0));
    if(ustatus == U_BUFFER_OVERFLOW_ERROR){
			return (size_t)u_strToUTF8(NULL, 0, NULL, d, d_len, &ustatus);
    }
		return strlen(dst);
}
#endif
