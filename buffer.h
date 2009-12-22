#include "mysqldep.h"

class FtCharBuffer {
public:
	virtual ~FtCharBuffer(){};
	virtual void append(my_wc_t wc) = 0;
	virtual void flush() = 0;
	virtual void reset() = 0;
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

