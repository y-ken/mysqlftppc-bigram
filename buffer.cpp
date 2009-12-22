#include "buffer.h"

#define FT_INITIAL_BUFFER 32
#define FT_CODEPOINTS_CAPACITY 32

FtMemBuffer::FtMemBuffer(CHARSET_INFO *cs){
	this->cs = cs;
	cursor = 0;
	bufferLength = FT_INITIAL_BUFFER;
	buffer = (char*)malloc(sizeof(char)*bufferLength);
}

FtMemBuffer::~FtMemBuffer(){
	free(buffer);
}

void FtMemBuffer::append(my_wc_t wc){
	if(cursor + cs->mbmaxlen > bufferLength){
		bufferLength *= 2;
		buffer = (char*)realloc(buffer, sizeof(char)*bufferLength);
	}
	if(buffer){
		int cnvres = cs->cset->wc_mb(cs, wc, (uchar*)(buffer+cursor), (uchar*)(buffer+bufferLength));
		if(cnvres>0){
			cursor += cnvres;
		}else{
			// error_log ?
			buffer[cursor]='?';
			cursor++;
		}
	}
}

void FtMemBuffer::flush(){}

void FtMemBuffer::reset(){
	cursor = 0;
}

char* FtMemBuffer::getBuffer(size_t *length, size_t *capacity){
	*length = cursor;
	*capacity = bufferLength;
	return buffer;
}

bool FtMemBuffer::detach(){
	cursor = 0;
	bufferLength = FT_INITIAL_BUFFER;
	buffer = (char*)malloc(sizeof(char)*bufferLength);
	return true;
}


FtWideBuffer::FtWideBuffer(){
	this->cursor = 0;
	this->bufferLength = FT_INITIAL_BUFFER;
	this->buffer = (my_wc_t*)malloc(sizeof(my_wc_t)*this->bufferLength);
}

FtWideBuffer::~FtWideBuffer(){
	free(this->buffer);
}

void FtWideBuffer::append(my_wc_t wc){
	if(this->cursor>=this->bufferLength){
		this->bufferLength = this->bufferLength * 2;
		this->buffer = (my_wc_t*)realloc(this->buffer, sizeof(my_wc_t)*this->bufferLength);
	}
	this->buffer[this->cursor] = wc;
	this->cursor++;
}

void FtWideBuffer::flush(){}

void FtWideBuffer::reset(){
	this->cursor = 0;
}

const my_wc_t* FtWideBuffer::getWideBuffer(size_t *length){
	*length = this->cursor;
	return const_cast<my_wc_t*>(this->buffer);
}

bool FtWideBuffer::detach(){
	this->cursor = 0;
	this->bufferLength = FT_INITIAL_BUFFER;
	this->buffer = (my_wc_t*)malloc(sizeof(my_wc_t)*this->bufferLength);
	return true;
}


#if HAVE_ICU
#include <unicode/normlzr.h>

FtNormalizerBuffer::FtNormalizerBuffer(FtCharBuffer *internal, UNormalizationMode mode){
	this->mode = mode;
	buffer = internal;
	cache = new UnicodeString();
}

FtNormalizerBuffer::~FtNormalizerBuffer(){
	delete buffer;
	delete cache;
}

void FtNormalizerBuffer::append(my_wc_t wc){
	cache->append((UChar32)wc);
}

void FtNormalizerBuffer::flush(){
	UChar32 u;
	Normalizer norm(const_cast<UnicodeString&>(*cache), mode);
	if(this->options|UNORM_UNICODE_3_2){
		norm,setOption(UNORM_UNICODE_3_2, true);
	}
	while((u=norm.next())!=Normalizer::DONE){ // XXX: DONE ?
		buffer->append((my_wc_t)u);
	}
	cache->truncate(0);
}

void FtNormalizerBuffer::reset(){
	this->buffer->reset();
	this->cache->truncate(0);
}

void FtNormalizerBuffer::setOption(int32_t option, UBool value){
	if(value){
		this->options |= option;
	}else{
		this->options &= (~option);
	}
}

#endif
