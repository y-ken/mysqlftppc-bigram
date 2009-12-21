#include <cstring>
#include "mempool.h"
#include "reader_bool.h"
#include "buffer.h"
#include "plugin_bigram.h"
#if HAVE_ICU
#include <unicode/uversion.h>
#include <unicode/uchar.h>
#include <unicode/uclean.h>
#include <unicode/unorm.h>
#endif
#include <my_sys.h>

#include <cstdio>


#if !defined(__attribute__) && (defined(__cplusplus) || !defined(__GNUC__)  || __GNUC__ == 2 && __GNUC_MINOR__ < 8)
#define __attribute__(A)
#endif

static void* icu_malloc(const void* context, size_t size){ return my_malloc(size,MYF(MY_WME)); }
static void* icu_realloc(const void* context, void* ptr, size_t size){ return my_realloc(ptr,size,MYF(MY_WME)); }
static void  icu_free(const void* context, void *ptr){ my_free(ptr,MYF(0)); }

static int bigram_parser_plugin_init(void *arg __attribute__((unused))){
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
	
	UErrorCode ustatus=U_ZERO_ERROR;
	u_setMemoryFunctions(NULL, icu_malloc, icu_realloc, icu_free, &ustatus);
	if(U_FAILURE(ustatus)){
		sprintf(errstr, "u_setMemoryFunctions failed. ICU status code %d\n", ustatus);
		fputs(errstr, stderr);
		fflush(stderr);
	}
#else
	strcat(bigram_info, ", without ICU");
#endif
	return 0;
}

static int bigram_parser_plugin_deinit(void *arg __attribute__((unused))){
	return 0;
}

static int bigram_parser_init(MYSQL_FTPARSER_PARAM *param __attribute__((unused))){
	param->ftparser_state = new FtMemPool();
	return 0;
}

static int bigram_parser_deinit(MYSQL_FTPARSER_PARAM *param __attribute__((unused))){
	delete (FtMemPool*)(param->ftparser_state);
	return 0;
}

static int bigram_parser_parse(MYSQL_FTPARSER_PARAM *param){
	FtCharReader *reader;
	FtMemReader memReader(const_cast<char*>(param->doc), (size_t)param->length, param->cs);
	reader = &memReader;
	
	MYSQL_FTPARSER_BOOLEAN_INFO info = { FT_TOKEN_WORD, 1, 0, 0, 0, ' ', 0 };
	
	if(param->mode == MYSQL_FTPARSER_FULL_BOOLEAN_INFO){
//fprintf(stderr, "MYSQL_FTPARSER_FULL_BOOLEAN_INFO\n");
//fflush(stderr);
		FtBoolReader boolParser(reader);
		reader = &boolParser;
#if HAVE_ICU
		// normalize
		if(bigram_unicode_normalize && strcmp(bigram_unicode_normalize, "OFF")!=0){
			UNormalizationMode mode = UNORM_NONE;
			if(strcmp(bigram_unicode_normalize, "C")==0) mode = UNORM_NFC;
			if(strcmp(bigram_unicode_normalize, "D")==0) mode = UNORM_NFD;
			if(strcmp(bigram_unicode_normalize, "KC")==0) mode = UNORM_NFKC;
			if(strcmp(bigram_unicode_normalize, "KD")==0) mode = UNORM_NFKD;
			if(strcmp(bigram_unicode_normalize, "FCD")==0) mode = UNORM_FCD;
		
			FtNormalizerReader normReader(reader, mode);
			if(bigram_unicode_version && strcmp(bigram_unicode_version, "3.2")==0){
				normReader.setOption(UNORM_UNICODE_3_2, TRUE);
			}
			reader = &normReader;
		}
#endif
		
		BigramBuffer bigramBuffer(param);
		bigramBuffer.setInfo(&info);
		
		char dummy = '\"';
		my_wc_t wc;
		int meta;
		while(reader->readOne(&wc, &meta)){
			// disable truncation operator in bigram boolean syntax.
			if(meta&FT_CHAR_TRUNC){ meta = FT_CHAR_NORM; }
			
			if(meta==FT_CHAR_NORM){
				bigramBuffer.append(wc);
			}else{
				bigramBuffer.flush();
				bigramBuffer.reset();
				
				if(meta==FT_CHAR_CTRL){       info.yesno = 0; }
				else if(meta&FT_CHAR_YES){    info.yesno = +1; }
				else if(meta&FT_CHAR_NO){     info.yesno = -1; }
				else if(meta&FT_CHAR_STRONG){ info.weight_adjust++; }
				else if(meta&FT_CHAR_WEAK){   info.weight_adjust--; }
				else if(meta&FT_CHAR_NEG){    info.wasign = !info.wasign; }
				
				if(meta&FT_CHAR_LEFT){
					info.type = FT_TOKEN_LEFT_PAREN;
					if(meta&FT_CHAR_QUOT){
						info.quot = &dummy;
					}
					param->mysql_add_word(param, NULL, 0, &info);
					info.type = FT_TOKEN_WORD;
				}else if(meta&FT_CHAR_RIGHT){
					info.type = FT_TOKEN_RIGHT_PAREN;
					param->mysql_add_word(param, NULL, 0, &info);
					if(meta&FT_CHAR_QUOT){
						info.quot = NULL;
					}
					info.type = FT_TOKEN_WORD;
				}
			}
		}
		bigramBuffer.flush();
	}else{
//fprintf(stderr, "non MYSQL_FTPARSER_FULL_BOOLEAN_INFO\n");
//fflush(stderr);
#if HAVE_ICU
		// normalize
		if(bigram_unicode_normalize && strcmp(bigram_unicode_normalize, "OFF")!=0){
			UNormalizationMode mode = UNORM_NONE;
			if(strcmp(bigram_unicode_normalize, "C")==0) mode = UNORM_NFC;
			if(strcmp(bigram_unicode_normalize, "D")==0) mode = UNORM_NFD;
			if(strcmp(bigram_unicode_normalize, "KC")==0) mode = UNORM_NFKC;
			if(strcmp(bigram_unicode_normalize, "KD")==0) mode = UNORM_NFKD;
			if(strcmp(bigram_unicode_normalize, "FCD")==0) mode = UNORM_FCD;
		
			FtNormalizerReader normReader(reader, mode);
			if(bigram_unicode_version && strcmp(bigram_unicode_version, "3.2")==0){
				normReader.setOption(UNORM_UNICODE_3_2, TRUE);
			}
			reader = &normReader;
		}
#endif
		BigramBuffer bigramBuffer(param);
		bigramBuffer.setInfo(&info);
		
		my_wc_t wc;
		int meta;
		while(reader->readOne(&wc, &meta)){
			bigramBuffer.append(wc);
		}
		bigramBuffer.flush();
	}
	return 0;
}

/**
void dumpInfo(MYSQL_FTPARSER_BOOLEAN_INFO *info){
	char a = '_';
	if(info->quot){ a=*(info->quot); }
	fprintf(stderr, "parser_info type:%d yesno:%d weight_adjust:%d wasign:%d trunc:%d byte:%c quot:%c\n",
			info->type, info->yesno, info->weight_adjust, info->wasign, info->trunc, info->prev, a);
	fflush(stderr);
}
*/

BigramBuffer::BigramBuffer(MYSQL_FTPARSER_PARAM *param){
	this->param = param;
	pool = (FtMemPool*)param->ftparser_state;
	membuffer = new FtMemBuffer(param->cs);
	used = 0;
	bigramQuote = false;
	info = NULL;
	dummy = '"';
}

BigramBuffer::~BigramBuffer(){
	delete membuffer;
}

void BigramBuffer::setInfo(MYSQL_FTPARSER_BOOLEAN_INFO *ptr){
	info = ptr;
}

void BigramBuffer::tokenFlush(){
	membuffer->reset();
	membuffer->append(wcs[0]);
	membuffer->append(wcs[1]);
	membuffer->flush();
	
	size_t bigramLength;
	size_t capacity;
	char *bigram = membuffer->getBuffer(&bigramLength, &capacity);
	
	if(bigramLength > 0){
		// string find
		info->type = FT_TOKEN_WORD;
		const char *save = pool->findPool(param->doc, param->length, const_cast<char*>(bigram), bigramLength);
		if(save){
//			if(info) dumpInfo(info);
			if(info){ param->mysql_add_word(param, (char*)save, bigramLength, info); }
		}else{
			membuffer->detach();
//			dumpInfo(info);
			if(info){ param->mysql_add_word(param, bigram, bigramLength, info); }
			pool->addHeap(new FtMemHeap(bigram, bigramLength, capacity));
		}
	}
}

void BigramBuffer::append(my_wc_t wc){
	if(used == 2){
		if(param->mode==MYSQL_FTPARSER_FULL_BOOLEAN_INFO && !bigramQuote && !info->quot){
			bigramQuote = true;
			info->quot = &dummy;
			info->type = FT_TOKEN_LEFT_PAREN;
//			dumpInfo(info);
			if(info){ param->mysql_add_word(param, NULL, 0, info); }
			info->type = FT_TOKEN_WORD;
		}
		
		this->tokenFlush();
		wcs[0]=wcs[1];
		wcs[1]=wc;
	}else{
		wcs[used]=wc;
		used++;
	}
}

void BigramBuffer::flush(){
	if(used==2){
		this->tokenFlush();
		if(param->mode==MYSQL_FTPARSER_FULL_BOOLEAN_INFO && bigramQuote){
			bigramQuote = false;
			info->type = FT_TOKEN_RIGHT_PAREN;
//			dumpInfo(info);
			if(info){ param->mysql_add_word(param, NULL, 0, info); }
			info->quot = NULL;
			info->type = FT_TOKEN_WORD;
		}
	}
	used = 0;
}

void BigramBuffer::reset(){
	bigramQuote = false;
	used = 0;
	membuffer->reset();
}

static int bigram_unicode_version_check(MYSQL_THD thd, struct st_mysql_sys_var *var, void *save, struct st_mysql_value *value){
	char buf[4];
	int len=4;
	const char *str;
	
	str = value->val_str(value,buf,&len);
	if(!str){ return -1; }
	*(const char**)save=str;
	if(len==3){
		if(memcmp(str, "3.2", len)==0){ return 0; }
	}
	if(len==7){
		if(memcmp(str, "DEFAULT", len)==0){ return 0; }
	}
	return -1;
}

static int bigram_unicode_normalize_check(MYSQL_THD thd, struct st_mysql_sys_var *var, void *save, struct st_mysql_value *value){
	char buf[4];
	int len=4;
	const char *str;
	
	str = value->val_str(value,buf,&len);
	if(!str) return -1;
	*(const char**)save=str;
	if(len==1){
		if(str[0]=='C'){ return 0; }
		if(str[0]=='D'){ return 0; }
	}else if(len==2){
		if(str[0]=='K' && str[1]=='C'){ return 0; }
		if(str[0]=='K' && str[1]=='D'){ return 0; }
	}else if(len==3){
		if(str[0]=='F' && str[1]=='C' && str[2]=='D'){ return 0; }
		if(str[0]=='O' && str[1]=='F' && str[2]=='F'){ return 0; }
	}
	return -1;
}

