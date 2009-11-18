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
    UErrorCode ustatus = U_ZERO_ERROR;
    // UChar source
    UChar *s;
    int32_t s_length, s_capacity;
    // UChar normalized
    UChar *d;
    int32_t d_length, d_capacity;
    // UTF8 normalzied
    int32_t dst_alloc;
	
    // convert UTF-8 -> UChar
	u_strFromUTF8(NULL, 0, &s_length, src, (int32_t)src_len, &ustatus);
	if(U_FAILURE(ustatus) && ustatus!=U_BUFFER_OVERFLOW_ERROR){
		char buf[1024];
		sprintf(buf,"ICU u_strFromUTF8(pre-flighting) error with %d\n", ustatus);
		fputs(buf, stderr); fflush(stderr);
		return 0;
	}else{
	    ustatus = U_ZERO_ERROR;
	}
	s_capacity = (s_length+7)/8*8; // for '\0' termination
    s = (UChar*)my_malloc(s_capacity*sizeof(UChar), MYF(MY_WME));
    if(!s){
		fputs("malloc failure\n", stderr); fflush(stderr);
		return 0;
	}
    s = u_strFromUTF8(s, s_length, NULL, src, (int32_t)src_len, &ustatus);
    if(U_FAILURE(ustatus)){
		char buf[1024];
		sprintf(buf,"ICU u_strFromUTF8 error with %d\n", ustatus);
		fputs(buf, stderr); fflush(stderr);
		my_free(s, MYF(0));
		return 0;
	}else{
	    ustatus = U_ZERO_ERROR;
	}
	
    // normalize
	d_length = unorm_normalize(s, s_length, umode, (int32_t)opt, NULL, 0, &ustatus);
	if(U_FAILURE(ustatus) && ustatus!=U_BUFFER_OVERFLOW_ERROR){
		char buf[1024];
		sprintf(buf,"ICU unorm_normalize(pre-flighting) error with %d\n", ustatus);
		fputs(buf, stderr); fflush(stderr);
		my_free(s, MYF(0));
		return 0;
	}else{
	    ustatus = U_ZERO_ERROR;
	}
	d_capacity = (d_length+7)/8*8;
    d = (UChar*)my_malloc(d_capacity*sizeof(UChar), MYF(MY_WME));
    if(!d){
		fputs("malloc failure\n", stderr); fflush(stderr);
		my_free(s, MYF(0));
		return 0;
    }
	d_length = unorm_normalize(s, s_length, umode, (int32_t)opt, d, d_capacity, &ustatus);
	if(U_FAILURE(ustatus)){
		char buf[1024];
		sprintf(buf,"ICU unorm_normalize error with %d\n", ustatus);
		fputs(buf, stderr); fflush(stderr);
		my_free(s, MYF(0));
		my_free(d, MYF(0));
		return 0;
	}else{
	    ustatus = U_ZERO_ERROR;
	}
	my_free(s, MYF(0));
	
    // encode UChar -> UTF-8
    u_strToUTF8(dst, (int32_t)dst_capacity, &dst_alloc, d, d_length, &ustatus);
    my_free(d, MYF(0));
	return (size_t)dst_alloc;
}
#endif
