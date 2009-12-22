#include "reader.h"

#include <cstdio>

FtCharReader::~FtCharReader(){}

bool FtCharReader::readOne(my_wc_t *wc, int *meta){
fprintf(stderr, "FtCharReader::readOne\n"); fflush(stderr);
	*wc = FT_EOS;
	*meta = FT_CHAR_CTRL;
	return false;
}

void FtCharReader::reset(){}


FtMemReader::FtMemReader(const char* buffer, std::size_t bufferLength, CHARSET_INFO *cs){
	cursor = directBuffer = buffer;
	directBufferLength =bufferLength;
	this->cs = cs;
}

FtMemReader::~FtMemReader(){}

bool FtMemReader::readOne(my_wc_t *wc, int *meta){
	if(cursor >= directBuffer+directBufferLength){
		*wc = FT_EOS;
		*meta = FT_CHAR_CTRL;
		return FALSE;
	}
	*meta = FT_CHAR_NORM;
	int cnvres=cs->cset->mb_wc(cs, wc, (uchar*)cursor, (uchar*)(directBuffer+directBufferLength));
	if(cnvres > 0){
		cursor+=cnvres;
	}else{
		cursor++;
		*wc='?';
	}
	return true;
}

void FtMemReader::reset(){
	cursor = directBuffer;
}


#if HAVE_ICU

WcIterator::WcIterator(FtCharReader *internal, int32_t internalLength, int32_t internalLength32){
//	fprintf(stderr,"WcIterator::WcIterator\n"); fflush(stderr);
	feed = internal;
	control = 0;
	controlLength = 0;
	cache = new UnicodeString(FT_CODEPOINTS_CAPACITY, (UChar32)0xFFFF, 0);
	cacheIterator = new StringCharacterIterator(const_cast<UnicodeString&>(*cache));
	formerLength = 0;
	former32Length = 0;
	eoc = false;
	
	pos = begin = 0;
	textLength = internalLength;
	textLength32 = internalLength32;
	end = textLength;
}

WcIterator::WcIterator(FtCharReader *internal){
//	fprintf(stderr,"WcIterator::WcIterator\n"); fflush(stderr);
	feed = internal;
	control = 0;
	controlLength = 0;
	cache = new UnicodeString(FT_CODEPOINTS_CAPACITY, (UChar32)0xFFFF, 0);
	cacheIterator = new StringCharacterIterator(const_cast<UnicodeString&>(*cache));
	formerLength = 0;
	former32Length = 0;
	eoc = false;
	
	pos = begin = 0;
	textLength = 0;
	textLength32 = 0;
	my_wc_t wc;
	int meta;
	while(!eoc){
		this->mirror();
		textLength += cache->length();
		textLength32 += cache->countChar32(0,cache->length());
	}
	this->reset();
	end = textLength;
}

WcIterator::~WcIterator(){
//	fprintf(stderr,"WcIterator::~WcIterator\n"); fflush(stderr);
	delete cache;
	delete cacheIterator;
}

CharacterIterator* WcIterator::clone() const {
//	fprintf(stderr,"WcIterator::clone\n"); fflush(stderr);
	return new WcIterator(feed, textLength, textLength32);
}

UBool WcIterator::operator== (const ForwardCharacterIterator &that) const {
//	fprintf(stderr,"WcIterator::operator==\n"); fflush(stderr);
	if(this==&that){ return TRUE; }
	if(getDynamicClassID() != that.getDynamicClassID()){ return FALSE; }
	WcIterator& realThat = (WcIterator&)that;
	return feed==realThat.feed && formerLength==realThat.formerLength && cacheIterator==realThat.cacheIterator;
}

int32_t WcIterator::hashCode() const {
//	fprintf(stderr,"WcIterator::hashCode\n"); fflush(stderr);
	return control+controlLength+(int32_t)cache+(int32_t)cacheIterator+formerLength+former32Length;
}

UChar WcIterator::nextPostInc(){
//	fprintf(stderr,"WcIterator::nextPostInc\n"); fflush(stderr);
	UChar ret = cacheIterator->current();
	this->next();
	pos = formerLength + cacheIterator->getIndex();
	return ret;
}

UChar32 WcIterator::next32PostInc(){
//	fprintf(stderr,"WcIterator::next32PostInc\n"); fflush(stderr);
	UChar32 ret = cacheIterator->current32();
	this->next32();
	pos = formerLength + cacheIterator->getIndex();
	return ret;
}

UBool WcIterator::hasNext(){
//	fprintf(stderr,"WcIterator::hasNext\n"); fflush(stderr);
	UBool ret = cacheIterator->hasNext();
	if(!ret && eoc==false){
		this->mirror();
		ret = cacheIterator->hasNext();
	}
	return ret;
}

void WcIterator::mirror(){
//	fprintf(stderr,"WcIterator::mirror\n"); fflush(stderr);
	formerLength += cache->length();
	former32Length += cache->countChar32(0, cache->length());
	cache->truncate(0);
	control = 0;
	controlLength = 0;
	
	int controls = 0;
	int i;
	for(i=0; i<FT_CODEPOINTS_CAPACITY; i++){
		my_wc_t wc;
		int meta;
		if(feed->readOne(&wc,&meta)){
			if(meta != FT_CHAR_NORM){
				metas[controls] = meta;
				controls++;
				wc = FT_EOS;
			}
			cache->append((UChar32)wc);
		}else{
			eoc=true;
			break;
		}
	}
	controlLength = controls;
	cacheIterator->setText(const_cast<UnicodeString&>(*cache));
	pos = formerLength + cacheIterator->getIndex();
}

void WcIterator::reset(){
//	fprintf(stderr,"WcIterator::reset\n"); fflush(stderr);
	feed->reset();
	
	cache->truncate(0);
	cacheIterator->setText(const_cast<UnicodeString&>(*cache));
	control = 0;
	controlLength = 0;
	formerLength = 0;
	former32Length = 0;
	eoc = false;
	pos = formerLength + cacheIterator->getIndex();
}

UChar WcIterator::first(){
//	fprintf(stderr,"WcIterator::first\n"); fflush(stderr);
	this->reset();
	this->mirror();
	UChar ret = cacheIterator->first();
	pos = formerLength + cacheIterator->getIndex();
	return ret;
}

UChar WcIterator::firstPostInc(){
//	fprintf(stderr,"WcIterator::firstPostInc\n"); fflush(stderr);
	this->reset();
	this->mirror();
	UChar ret = cacheIterator->firstPostInc();
	pos = formerLength + cacheIterator->getIndex();
	return ret;
}

UChar32 WcIterator::first32(){
//	fprintf(stderr,"WcIterator::first32\n"); fflush(stderr);
	this->reset();
	this->mirror();
	UChar32 ret = cacheIterator->first32();
	pos = formerLength + cacheIterator->getIndex();
	return ret;
}

UChar32 WcIterator::first32PostInc(){
//	fprintf(stderr,"WcIterator::first32PostInc\n"); fflush(stderr);
	this->reset();
	this->mirror();
	UChar32 ret = cacheIterator->first32PostInc();
	pos = formerLength + cacheIterator->getIndex();
	return ret;
}

UChar WcIterator::last(){
//	fprintf(stderr,"WcIterator::last\n"); fflush(stderr);
	while(eoc != true){
		this->mirror();
	}
	UChar ret = cacheIterator->last();
	pos = formerLength + cacheIterator->getIndex();
	return ret;
}

UChar32 WcIterator::last32(){
//	fprintf(stderr,"WcIterator::last32\n"); fflush(stderr);
	while(eoc != true){
		this->mirror();
	}
	UChar32 ret = cacheIterator->last32();
	pos = formerLength + cacheIterator->getIndex();
	return ret;
}

UChar WcIterator::setIndex(int32_t position){
//	fprintf(stderr,"WcIterator::setIndex(%d)\n", position); fflush(stderr);
	if(position < formerLength){
		this->reset();
	}
	while(formerLength + cache->length() < position && !eoc){
		this->mirror();
	}
	UChar ret = cacheIterator->setIndex(position-formerLength);
	pos = formerLength + cacheIterator->getIndex();
	return ret;
}

UChar32 WcIterator::setIndex32(int32_t position){
//	fprintf(stderr,"WcIterator::setIndex32\n"); fflush(stderr);
	this->setIndex(position);
	return cacheIterator->current32();
}

UChar WcIterator::current() const {
//	fprintf(stderr,"WcIterator::current\n"); fflush(stderr);
	return cacheIterator->current();
}

UChar32 WcIterator::current32() const {
//	fprintf(stderr,"WcIterator::current32\n"); fflush(stderr);
	return cacheIterator->current32();
}

UChar WcIterator::next(){
//	fprintf(stderr,"WcIterator::next\n"); fflush(stderr);
	UChar ret = cacheIterator->next();
	if(ret==CharacterIterator::DONE){
		if(control < controlLength){
			control++;
		}else if(!eoc){
			// reached cache end
			this->mirror();
			ret = cacheIterator->next();
		}
	}
	pos = formerLength + cacheIterator->getIndex();
	return ret;
}

UChar32 WcIterator::next32(){
//	fprintf(stderr,"WcIterator::next32\n"); fflush(stderr);
	UChar32 ret = cacheIterator->next32();
	if(ret==CharacterIterator::DONE){
		if(control < controlLength){
			control++;
		}else if(!eoc){
			// reached cache end
			this->mirror();
			ret = cacheIterator->next32();
		}
	}
	pos = formerLength + cacheIterator->getIndex();
	return ret;
}

UChar WcIterator::previous(){
//	fprintf(stderr,"WcIterator::previous\n"); fflush(stderr);
	UChar ret = cacheIterator->previous();
	if(ret==CharacterIterator::DONE){
		if(this->control > 0){
			this->control--;
		}else if(formerLength > 0){
			// reached cache end
			int32_t stop = this->former32Length;
			this->reset();
			while(stop < formerLength+cache->length()){
				this->mirror();
			}
			ret = cacheIterator->last();
		}
	}
	pos = formerLength + cacheIterator->getIndex();
	return ret;
}

UChar32 WcIterator::previous32(){
//	fprintf(stderr,"WcIterator::previous32\n"); fflush(stderr);
	this->previous();
	return cacheIterator->current32();
}

UBool WcIterator::hasPrevious(){
//	fprintf(stderr,"WcIterator::hasPrevious\n"); fflush(stderr);
	if(cacheIterator->hasPrevious()){
		return TRUE;
	}
	if(formerLength > 0){
		return TRUE;
	}
	return FALSE;
}

int32_t WcIterator::move(int32_t delta, EOrigin origin){
//	fprintf(stderr,"WcIterator::move(%d,%d)\n",delta,origin); fflush(stderr);
	if(origin==kStart){
		if(delta >= formerLength && delta < formerLength + cache->length()){
			cacheIterator->move(delta-formerLength, kStart);
		}else if(delta>=0){
			if(delta < formerLength){
				this->reset();
			}
			while(delta > formerLength + cache->length()){
				this->mirror();
			}
			cacheIterator->move(delta-formerLength, kStart);
		}else{
//			fprintf(stderr, "invalid index can't be nagative %d", delta); fflush(stderr);
			return -1;
		}
		pos = formerLength + cacheIterator->getIndex();
		return delta;
	}else if(origin==kCurrent){
		int32_t target = formerLength + cacheIterator->move(0,kCurrent) + delta;
		if(target>=0 && target<end){
			return this->move(target, kStart);
		}else{
//			fprintf(stderr, "invalid kCurrent index target %d %d\n", target, formerLength); fflush(stderr);
		}
	}else if(origin==kEnd){
		int32_t target = end + delta;
		if(target>=0 && target<end){
			return this->move(target, kStart);
		}else{
//			fprintf(stderr, "invalid kEnd index target %d\n", target); fflush(stderr);
		}
	}
	return -1;
}

int32_t WcIterator::move32(int32_t delta, EOrigin origin){
//	fprintf(stderr,"WcIterator::move32\n"); fflush(stderr);
	if(origin==kStart){
		if(delta >= former32Length && delta < former32Length + cache->countChar32(0,cache->length())){
			cacheIterator->move32(delta-former32Length, kStart);
		}else if(delta>=0){
			if(delta < former32Length){
				this->reset();
			}
			while(delta > former32Length + cache->countChar32(0,cache->length())){
				this->mirror();
			}
			cacheIterator->move32(delta-former32Length, kStart);
		}else{
			// invalid. index can't be negative.
			return -1;
		}
		pos = formerLength + cacheIterator->getIndex();
		return delta;
	}else if(origin==kCurrent){
		int32_t target = former32Length + cacheIterator->move32(0,kStart) + delta;
		if(target>=0 && target<end){
			return this->move32(target, kStart);
		}
	}else if(origin==kEnd){
		int32_t target = textLength32 + delta;
		if(target>=0 && target<end){
			return this->move32(target, kStart);
		}
	}
	return -1;
}

void WcIterator::getText(UnicodeString &result){
//	fprintf(stderr,"WcIterator::getText\n"); fflush(stderr);
	
	int32_t target = this->move(0,kStart);
	this->reset();
	while(eoc!=true){
		this->mirror();
		result.append(*cache);
	}
	this->move(target,kStart);
}

int WcIterator::getPreviousControlMeta(){
//	fprintf(stderr,"WcIterator::getPreviousControlMeta\n"); fflush(stderr);
	if(control>0){
		return metas[control-1];
	}else{
		// maybe reached at heading of the string.
		return FT_CHAR_CTRL;
	}
}


FtNormalizerReader::FtNormalizerReader(FtCharReader *internal, UNormalizationMode mode){
	this->mode = mode;
	wrapper = new WcIterator(internal);
	normalizer = new Normalizer(*wrapper, mode);
	eos = false;
}

FtNormalizerReader::~FtNormalizerReader(){
	delete wrapper;
	delete normalizer;
}

bool FtNormalizerReader::readOne(my_wc_t *wc, int *meta){
	if(normalizer->getIndex() < normalizer->endIndex()){
		UChar32 token = normalizer->next();
		*wc = (my_wc_t)token;
		if(token==CharacterIterator::DONE){
			*meta = wrapper->getPreviousControlMeta();
		}else{
			*meta = FT_CHAR_NORM;
		}
		return true;
	}else{
		*wc = FT_EOS;
		*meta = FT_CHAR_CTRL;
		return false;
	}
}

void FtNormalizerReader::reset(){
	wrapper->reset();
	normalizer->reset();
	eos = false;
}

void FtNormalizerReader::setOption(int32_t option, UBool value){
	normalizer->setOption(option,value);
}

#endif

