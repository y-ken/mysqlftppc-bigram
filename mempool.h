#include <vector>
#include <cstdlib>

class FtMemHeap {
public:
	FtMemHeap(char* heaphead, size_t length, size_t capacity);
	~FtMemHeap();
	char* head;
	size_t length;
	size_t capacity;
};

class FtMemPool {
	std::vector<FtMemHeap*> heaps;
public:
	FtMemPool();
	~FtMemPool();
	void addHeap(FtMemHeap *heap);
	const char* findPool(const char* haystack, size_t haystackLength, const char* head, size_t length);
};

