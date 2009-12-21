#include "reader.h"

// initial capacity may be the other value.
#define FT_CODEPOINTS_CAPACITY 32

FtMemReader::FtMemReader(const char* buffer, std::size_t bufferLength, CHARSET_INFO *cs){
	this->directBuffer = buffer;
	this->directBufferLength =bufferLength;
	this->cs = cs;
	this->cursor = this->directBuffer;
}

FtMemReader::~FtMemReader(){}

bool FtMemReader::readOne(my_wc_t *wc, int *meta){
	if(this->cursor >= this->directBuffer+directBufferLength){
		*wc = FT_EOS;
		*meta = FT_CHAR_CTRL;
		return FALSE;
	}
	*meta = FT_CHAR_NORM;
	int cnvres=this->cs->cset->mb_wc(cs, wc, (uchar*)this->cursor, (uchar*)(this->directBuffer+directBufferLength));
	if(cnvres > 0){
		this->cursor+=cnvres;
	}else{
		this->cursor++;
		*wc='?';
	}
	return true;
}

void FtMemReader::reset(){
	this->cursor = this->directBuffer;
}

FtWideReader::FtWideReader(const my_wc_t *buffer, size_t bufferLength){
	this->cursor = buffer;
	this->buffer = buffer;
	this->bufferLength = bufferLength;
}

FtWideReader::~FtWideReader(){}

bool FtWideReader::readOne(my_wc_t *wc, int *meta){
	if(this->cursor < this->buffer+this->bufferLength){
		*wc = *(this->cursor);
		*meta = FT_CHAR_NORM;
		this->cursor++;
		return true;
	}
	*wc = FT_EOS;
	*meta = FT_CHAR_CTRL;
	return false;
}

void FtWideReader::reset(){
	this->cursor = this->buffer;
}

#if HAVE_ICU

WcIterator::WcIterator(FtCharReader *internal){
	reader = internal;
	control = 0;
	controlLength = 0;
	metas = (int*)malloc(sizeof(int)*FT_CODEPOINTS_CAPACITY);
	cache = new UnicodeString(0, (UChar32)'\0', FT_CODEPOINTS_CAPACITY);
	cacheIterator = new StringCharacterIterator(const_cast<UnicodeString&>(*cache));
	formerLength = 0;
	former32Length = 0;
	eoc = false;
}

WcIterator::~WcIterator(){
	free(metas);
	delete cache;
	delete cacheIterator;
}

CharacterIterator* WcIterator::clone() const {
	return new WcIterator(reader);
}

UBool WcIterator::operator== (const ForwardCharacterIterator &that) const {
	if(this==&that){ return TRUE; }
	if(getDynamicClassID() != that.getDynamicClassID()){ return FALSE; }
	WcIterator& realThat = (WcIterator&)that;
	return reader==realThat.reader && formerLength==realThat.formerLength && cacheIterator==realThat.cacheIterator;
}

int32_t WcIterator::hashCode() const {
	return control+controlLength+(int32_t)cache+(int32_t)cacheIterator+formerLength+former32Length;
}

UChar WcIterator::nextPostInc(){
	UChar ret = cacheIterator->current();
	this->next();
	return ret;
}

UChar32 WcIterator::next32PostInc(){
	UChar32 ret = cacheIterator->current32();
	this->next32();
	return ret;
}

UBool WcIterator::hasNext(){
	UBool ret = cacheIterator->hasNext();
	if(!ret && eoc==false){
		this->mirror();
		ret = cacheIterator->hasNext();
	}
	return ret;
}

void WcIterator::mirror(){
	formerLength += cache->length();
	former32Length += cache->countChar32(0, cache->length());
	cache->truncate(0);
	control = 0;
	controlLength = 0;
	
	int controls = 0;
	int i;
	for(i=0; i<FT_CODEPOINTS_CAPACITY; i++){
		int wmeta;
		my_wc_t wc;
		if(reader->readOne(&wc,&wmeta)){
			if(wmeta != FT_CHAR_NORM){
				metas[controls] = wmeta;
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
}

void WcIterator::reset(){
	reader->reset();
	control = 0;
	controlLength = 0;
	formerLength = 0;
	former32Length = 0;
	cache->truncate(0);
	eoc = false;
	this->mirror();
}

UChar WcIterator::first(){
	this->reset();
	return this->cacheIterator->first();
}

UChar WcIterator::firstPostInc(){
	this->reset();
	return this->cacheIterator->firstPostInc();
}

UChar32 WcIterator::first32(){
	this->reset();
	return this->cacheIterator->first32();
}

UChar32 WcIterator::first32PostInc(){
	this->reset();
	return cacheIterator->first32PostInc();
}

UChar WcIterator::last(){
	while(eoc != true){
		this->mirror();
	}
	return cacheIterator->last();
}

UChar32 WcIterator::last32(){
	while(eoc != true){
		this->mirror();
	}
	return cacheIterator->last();
}

UChar WcIterator::setIndex(int32_t position){
	if(position >= formerLength && position < formerLength + cache->length()){
		return cacheIterator->setIndex(position - formerLength);
	}
	if(position < formerLength){
		this->reset();
	}
	UChar ret;
	int32_t seek = formerLength;
	while(seek < position){
		ret = this->next();
		seek++;
	}
	return ret;
}

UChar32 WcIterator::setIndex32(int32_t position){
	if(position >= former32Length && position < former32Length + cache->countChar32(0,INT32_MAX)){
		return cacheIterator->setIndex32(position-former32Length);
	}
	if(position < former32Length){
		this->reset();
	}
	UChar32 ret;
	int32_t seek = former32Length;
	while(seek < position){
		ret = this->next32();
		seek++;
	}
	return ret;
}

UChar WcIterator::current() const {
	return cacheIterator->current();
}

UChar32 WcIterator::current32() const {
	return cacheIterator->current32();
}

UChar WcIterator::next(){
	UChar ret = cacheIterator->next();
	if(ret==CharacterIterator::DONE){
		if(this->control < this->controlLength){
			this->control++;
		}else{
			// reached cache end
			this->mirror();
			ret = cacheIterator->next();
		}
	}
	return ret;
}

UChar32 WcIterator::next32(){
	UChar32 ret = this->cacheIterator->next32();
	if(ret==CharacterIterator::DONE){
		if(this->control < this->controlLength){
			this->control++;
		}else{
			// reached cache end
			this->mirror();
			ret = this->cacheIterator->next32();
		}
	}
	return ret;
}

UChar WcIterator::previous(){
	UChar ret = this->cacheIterator->previous();
	if(ret==CharacterIterator::DONE){
		if(this->control > 0){
			this->control--;
		}else{
			// reached cache end
			int32_t last = this->former32Length;
			this->reset();
			while(this->former32Length < last-FT_CODEPOINTS_CAPACITY){
				this->mirror();
			}
			return this->cacheIterator->last();
		}
	}
	return ret;
}

UChar32 WcIterator::previous32(){
	UChar32 ret = this->cacheIterator->previous32();
	if(ret==CharacterIterator::DONE){
		if(this->control > 0){
			this->control--;
		}else{
			// reached cache end
			int32_t last = this->former32Length;
			this->reset();
			while(this->former32Length < last-FT_CODEPOINTS_CAPACITY){
				this->mirror();
			}
			return this->cacheIterator->last32();
		}
	}
	return ret;
}

UBool WcIterator::hasPrevious(){
	if(this->cacheIterator->hasPrevious()){
		return true;
	}
	if(this->formerLength > 0){
		return true;
	}
	return false;
}

int32_t WcIterator::move(int32_t delta, EOrigin origin){
	if(origin==kStart){
		if(delta >= formerLength && delta < formerLength + cache->length()){
			cacheIterator->move(delta-formerLength, kStart);
			return delta;
		}else if(delta>=0){
			this->reset();
			while(delta > formerLength + cache->length()){
				this->mirror();
			}
			cacheIterator->move(delta-formerLength, kStart);
		}else{
			// invalid. index can't be negative.
		}
		return delta;
	}else if(origin==kCurrent){
		int32_t cachePosition = cacheIterator->move(0,kStart);
		if(delta >= -cachePosition && delta < cache->length()-cachePosition){
			return this->formerLength + cacheIterator->move(delta, kCurrent);
		}
		return this->move(this->formerLength + cachePosition + delta, kStart);
	}else if(origin==kEnd){
		this->last();
		if(delta > -cache->length()){
			return this->formerLength + cacheIterator->move(delta, kEnd);
		}
		return this->move(formerLength + cache->length() + delta, kStart);
	}else{
		// invalid origin.
	}
}

int32_t WcIterator::move32(int32_t delta, EOrigin origin){
	if(origin==kStart){
		if(delta >= former32Length && delta < former32Length + cache->countChar32(0,cache->length())){
			cacheIterator->move32(delta-former32Length, kStart);
			return delta;
		}else if(delta>=0){
			this->reset();
			while(delta > formerLength+cache->countChar32(0,cache->length())){
				this->mirror();
			}
			cacheIterator->move32(delta-former32Length, kStart);
		}else{
			// invalid. index can't be negative.
		}
		return delta;
	}else if(origin==kCurrent){
		int32_t cachePosition = this->cacheIterator->move32(0,kStart);
		if(delta >= -cachePosition && delta < cache->countChar32(0,cache->length())-cachePosition){
			return this->former32Length + this->cacheIterator->move(delta, kCurrent);
		}
		return this->move32(this->former32Length + cachePosition + delta, kStart);
	}else if(origin==kEnd){
		this->last();
		if(delta > -cache->countChar32(0,cache->length())){
			return former32Length + cacheIterator->move32(delta, kEnd);
		}
		return this->move32(former32Length + cache->countChar32(0,cache->length()) + delta, kStart);
	}else{
		// invalid origin.
	}
}

void WcIterator::getText(UnicodeString &result){
	reader->reset();
	formerLength = 0;
	former32Length = 0;
	cache->truncate(0);
	eoc = false;
	
	result.truncate(0);
	while(eoc != true){
		this->mirror();
		result.append(*cache);
	}
}

bool WcIterator::isEnd(){
	return this->eoc;
}

int WcIterator::getPreviousControlMeta(){
	if(this->control>0){
		return this->metas[this->control-1];
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
	UChar32 token = normalizer->next();
	*wc = (my_wc_t)token;
	if(token==CharacterIterator::DONE){
		if(wrapper->isEnd()){
			*meta = FT_CHAR_CTRL;
			return false;
		}else{
			*meta = wrapper->getPreviousControlMeta();
		}
	}else{
		*meta = FT_CHAR_NORM;
	}
	return true;
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

