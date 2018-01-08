#include "memoryPool.h"
#include "memPoolApi.h"

void apl_resetBigMemSize(unsigned int size)
{
	innerDeleteMemPool();
	innerSetBigMemTotalSize(size);
	innerCreateMemPool();
}

void apl_createMemPool(unsigned int bigMmSize)
{
	innerSetBigMemTotalSize(bigMmSize);
	innerCreateMemPool();
}

void apl_deleteMemPool()
{
	innerDeleteMemPool();
}

void* apl_malloc(size_t size)
{
	MemoryManage* const mm_instance = innerGetMemPool();
	if(mm_instance != NULL){
		return mm_instance->Malloc(size);
	}
	return NULL;
}

void apl_free(void* addr)
{
       MemoryManage* const mm_instance = innerGetMemPool();
	mm_instance->Free(addr);
	return;
}

