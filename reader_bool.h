#include "reader.h"

class FtBoolReader : public FtCharReader {
	FtCharReader *src;
	bool strhead;
	bool quot;
public:
	FtBoolReader(FtCharReader *feed);
	~FtBoolReader();
	bool readOne(my_wc_t *wc, int *meta);
	void reset();
};

