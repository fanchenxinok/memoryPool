#ifndef __MEM_API__
#define __MEM_API__

void apl_resetBigMemSize(unsigned int size);
void apl_createMemPool(unsigned int bigMmSize);
void apl_deleteMemPool();

void* apl_malloc(size_t size);
void apl_free(void* addr);

#endif
