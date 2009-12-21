#include "buffer.h"

#define FT_INITIAL_BUFFER 32
#define FT_CODEPOINTS_CAPACITY 32

FtMemBuffer::FtMemBuffer(CHARSET_INFO *cs){
	this->cs = cs;
	this->cursor = 0;
	this->bufferLength = FT_INITIAL_BUFFER;
	this->buffer = (char*)malloc(sizeof(char)*this->bufferLength);
}

FtMemBuffer::~FtMemBuffer(){
	free(this->buffer);
}

void FtMemBuffer::append(my_wc_t wc){
	if(this->cursor + this->cs->mbmaxlen > this->bufferLength){
		this->bufferLength *= 2;
		this->buffer = (char*)realloc(this->buffer, sizeof(char)*this->bufferLength);
	}
	if(this->buffer){
		int cnvres = this->cs->cset->wc_mb(this->cs, wc, (uchar*)(this->buffer+this->cursor), (uchar*)(this->buffer+this->bufferLength));
		if(cnvres>0){
			this->cursor += cnvres;
		}else{
			// error_log ?
			this->buffer[this->cursor]='?';
			this->cursor++;
		}
	}
}

void FtMemBuffer::flush(){}

void FtMemBuffer::reset(){
	this->cursor = 0;
}

char* FtMemBuffer::getBuffer(size_t *length, size_t *capacity){
	*length = this->cursor;
	*capacity = this->bufferLength;
	return this->buffer;
}

bool FtMemBuffer::detach(){
	this->cursor = 0;
	this->bufferLength = FT_INITIAL_BUFFER;
	this->buffer = (char*)malloc(sizeof(char)*this->bufferLength);
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
