#include "mempool.h"

const char* ft_bytesFind(const char *haystack, size_t haystackLength, const char *needle, size_t needleLength){
	if(haystackLength < needleLength){ return NULL; }
	if(haystack < needle && haystack+haystackLength >= needle+needleLength){ return needle; }
	size_t map[0xFF];
	{
		int i=0;
		for(i=0; i<0x100; i++){ map[i] = needleLength+1; }
	}
	{
		int i=0;
		for(i=0; i<needleLength; i++){
			map[(unsigned char)needle[i]] = needleLength-i;
		}
	}
	{
		int i=0;
		for(i=0;i<=haystackLength-needleLength;){
			size_t j=0;
			while(j < needleLength && haystack[i+j]==needle[j]){
				j++;
			}
			if(j==needleLength){
				return (haystack+i);
			}
			i += map[(unsigned char)haystack[i+needleLength]];
		}
	}
	return NULL;
}

int ft_findShared(const char *base, size_t baseLength, size_t capacity, const char* token, size_t tokenLength){
	size_t sharedLength = tokenLength;
	while(sharedLength>0){
		if(baseLength+tokenLength-sharedLength>capacity){
			break;
		}
		int i=0;
		for(i=0;i<sharedLength;i++){
			if(base[baseLength-sharedLength+i]!=token[i]){
				break;
			}
		}
		if(i==sharedLength){
			return sharedLength;
		}
		sharedLength--;
	}
	return -1;
}


FtMemHeap::FtMemHeap(char* heaphead, size_t length, size_t capacity){
	this->head = heaphead;
	this->length = length;
	this->capacity = capacity;
}

FtMemHeap::~FtMemHeap(){
	free(this->head);
}


FtMemPool::FtMemPool(){
}

FtMemPool::~FtMemPool(){
	for(std::vector<FtMemHeap*>::iterator it=this->heaps.begin(); it!=this->heaps.end(); ++it){
		delete *it;
	}
}

void FtMemPool::addHeap(FtMemHeap *heap){
	this->heaps.push_back(heap);
}

const char* FtMemPool::findPool(const char* haystack, size_t haystackLength, const char* needle, size_t needleLength){
	const char * ret = ft_bytesFind(haystack, haystackLength, needle, needleLength);
	if(ret){ return ret; }
	for(std::vector<FtMemHeap*>::iterator it=heaps.begin(); it!=heaps.end(); ++it){
		FtMemHeap *o = *it;
		int shared = ft_findShared(o->head, o->length, o->capacity, needle, needleLength);
		if(shared>0){
			int i;
			for(i=0;i<needleLength-shared;i++){
				o->head[o->length+i]=needle[shared+i];
			}
			o->length = o->length+shared;
			return o->head-shared;
		}
	}
	return NULL;
}
