#include "buffer.h"

#define FT_INITIAL_BUFFER 32

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

