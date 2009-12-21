#include "mysqldep.h"

class FtCharBuffer {
public:
	virtual ~FtCharBuffer(){};
	virtual void append(my_wc_t wc){};
	virtual void flush(){};
	virtual void reset(){};
};

class FtMemBuffer : public FtCharBuffer {
	CHARSET_INFO *cs;
	size_t cursor;
	char *buffer;
	size_t bufferLength;
public:
	FtMemBuffer(CHARSET_INFO *cs);
	~FtMemBuffer();
	void append(my_wc_t wc);
	void flush();
	void reset();
	//
	char* getBuffer(size_t *length, size_t *capacity);
	bool detach();
};

class FtWideBuffer : public FtCharBuffer {
	my_wc_t* buffer;
	size_t bufferLength;
	size_t cursor;
public:
	FtWideBuffer();
	~FtWideBuffer();
	void append(my_wc_t wc);
	void flush();
	void reset();
	//
	const my_wc_t* getWideBuffer(size_t *length);
	bool detach();
};


#if HAVE_ICU
#include <unicode/unistr.h>
#include <unicode/unorm.h>

// post-normalization
class FtNormalizerBuffer : public FtCharBuffer {
	FtCharBuffer *buffer;
	UnicodeString *cache;
	UNormalizationMode mode;
	int32_t options;
public:
	FtNormalizerBuffer(FtCharBuffer *internal, UNormalizationMode mode);
	~FtNormalizerBuffer();
	void append(my_wc_t wc);
	void flush();
	void reset();
	// FtNormalizerBuffer
	void setOption(int32_t option, UBool value);
};

#endif
