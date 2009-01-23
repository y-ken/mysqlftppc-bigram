#include <my_global.h>
#include <my_sys.h>
#if HAVE_ICU
#include <unicode/unorm.h>
#include <unicode/ustring.h>

/**
 * @param src source string
 * @param src_len source string length
 * @param dst buffer to write
 * @param dst_capacity buffer capacity
 * @param mode same with ICU mode flag UNormalizationMode.
 * @param options same with ICU options flag
 * @return buffer size required. 0 on failure. When the value was larger than dst_capacity, you must realloc and retry.
 */
size_t uni_normalize(char* src, size_t src_len, char* dst, size_t dst_capacity, int mode, int options){
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
    u_strFromUTF8(s, s_alloc, NULL, src, (int32_t)src_len, &ustatus);
    if(U_FAILURE(ustatus)){
			ustatus = 0;
			s_alloc = u_strFromUTF8(NULL, 0, NULL, src, (int32_t)src_len, &ustatus) + 4; // prefill
      tmp = (UChar*)my_realloc(s, s_alloc, MYF(MY_WME));
      if(!tmp){
				my_free(s, MYF(0));
				return 0;
			}else{
				s=tmp;
			}
      ustatus = 0;
      u_strFromUTF8(s, s_alloc, NULL, src, (int32_t)src_len, &ustatus);
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
    d_len = unorm_normalize(s, -1, umode, (int32_t)options, d, d_alloc, &ustatus);
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
        d_len = unorm_normalize(s, -1, mode, (int32_t)options, d, d_alloc, &ustatus);
    }
    my_free(s, MYF(0));
    // encode UChar -> UTF-8
    u_strToUTF8(dst, (int32_t)dst_capacity, NULL, d, d_len, &ustatus);
    if(U_FAILURE(ustatus)){ // U_BUFFER_OVERFLOW_ERROR
			ustatus = 0;
			dst_alloc = u_strToUTF8(NULL, 0, NULL, d, d_len, &ustatus);
	    my_free(d, MYF(0));
			return dst_alloc;
    }
    my_free(d, MYF(0));
		return strlen(dst);
}
#endif
