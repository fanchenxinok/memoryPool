#ifndef __MY_MEM_POOL__
#define __MY_MEM_POOL__
#include "autoLock.h"
#include "comDef.h"

/*************************************************
(1)小内存:
     pUsedList[]: [8]  [16]  [32]  [64]  [128]......
			   ↓ ↓  ↓   ↓   ↓
			  [8]  [16]  [32]  [64]  [128]......
			   ↓ ↓  ↓   ↓   ↓
			  [8]  [16]  [32]  [64]  [128]
			    .......
			    
     pFreeList[]: [8]  [16]  [32]  [64]  [128]......
			   ↓ ↓  ↓   ↓   ↓
			  [8]  [16]  [32]  [64]  [128]......
			   ↓ ↓  ↓   ↓   ↓
			  [8]  [16]  [32]  [64]  [128]
			    .......

(2)大内存:
	链表结构: pBlockListHead[0]<->block1[size1]<->block2[size2]<->.........
	
	分配内存: 大的内存块由一个双向链表来管理，每次分配找到一个空闲节点A，
	判断该空闲节点A 的size是否满足需要分配的大小，如果该节点A 分
	配完以后还有剩余大于或等于最小blockSize 则新建个节点B，然后把该
	节点A 的使用标志置为1，新建节点B 的大小为A 节点分配后剩余的内存空间大小。

	释放内存: 找到要释放的内存地址相符的节点，将该节点的使用标志置为0，
	同时判断该节点的前后节点是否被使用，如果没有被使用则将前后没被使用的节点
	合并为一个节点使该节点的size更大，小的内存得以回收。
*************************************************/

typedef struct smNode
{
	void* start_addr;
	struct smNode *next;
}SmNode, *pSmNode;

/* small memroy list struct */
typedef struct SmBlockList
{
	pSmNode pUsedList;
	pSmNode pFreeList;
	void **pBlockFirstAddr; /* 保存小内存池各个档次size 所分配内存的起始地址 */
	unsigned int *pBlockSize; /* 小内存池各个档次size 分配内存的总大小 */
}st_SmBlockList;

typedef struct bigNode
{
	void* start_addr;
	unsigned int size;
	unsigned char usedFlag;
	//unsigned char reserved[3]; 
	struct bigNode *next;
	struct bigNode *prev;
}BigNode, *pBigNode;

#define TEST_BIGMM (0)  /* Test the big memory alloc */

/* big memory list struct */
typedef struct BigBlockList
{
	pBigNode pBlockListHead;
	pBigNode pSearchStartNode;
	unsigned int blockCount;       //当前已使用和未使用的block个数
#if TEST_BIGMM  /* for test */
	unsigned int remainSize;       //所有未被使用的总内存
	unsigned int unUsedBlockNum;   //当前未被使用的block的个数
#endif
	unsigned int minSizePerBlock;  //每个block最小的分配size
}st_BigBlockList;

/* 小内存各档次size定义，最后一个用于分配大内存链表节点 */
enum
{
	SML_8 = 0,
	SML_16,
	SML_32,
	SML_64,
	SML_128,
	SML_256,
	SML_512,
	SML_1024,
	BIG_NODE,
	MAX_IDX
};

enum
{
	SML8_S = 8,
	SML16_S = 16,
	SML32_S = 32,
	SML64_S = 64,
	SML128_S = 128,
	SML256_S = 256,
	SML512_S = 512,
	SML1024_S = 1024,
	BIGNODE_S = sizeof(BigNode)  /* 大内存链表节点大小 */
};


/* 小内存各档次节点数 */
enum
{
	SML8_N = 3000,
	SML16_N = 3000,
	SML32_N = 3000,
	SML64_N = 3000,
	SML128_N = 3000,
	SML256_N = 1500,
	SML512_N = 1500,
	SML1024_N = 1500,
	//BIGNODE_N = 1000
};


class MemoryManage   //内存管理
{
public:
	MemoryManage();
	~MemoryManage();
	void* Malloc(size_t size);
	void Free(void* addr);
	pBigNode GetBigListHead() const;
private:
	MemoryManage(const MemoryManage&);  /* 将拷贝构造函数和赋值函数设置为private，防止建立多个拷贝 */
	MemoryManage& operator= (const MemoryManage&);
	
	void BigMemInit();
	void BigMemDeInit();
	void* MallocBigMemory(size_t size);
	void FreeBigMemory(void* addr);

	void SmlMemInit();
	void SmlMemDeInit();
	void* GetFreeSmNode(int index);  /*move the smNode from freeList to usedList; and return pSmNode->start_addr */
	void RetUsedSmNode(int index, void* addr); /* move the smNode from usedList to freeList */
	void* MallocSmlMemory(size_t size);
	void FreeSmlMemory(void* addr);

	void* firstAddr;              /* 记录内存池首地址 */
	void* smlListFirstAddr;  /* 记录管理小内存池链表的首地址 */
	int total_small_list_head;  /* 小内存池总的链表数 */
	int total_small_node_num; /* 小内存池总的链表节点数 */

	int total_big_size;
	void* bigMmFirstAddr;   /* 记录大内存池的首地址 */

	st_SmBlockList sm_block_list;
	st_BigBlockList bg_block_list;

	myThreadLock *smLock;  /**< 小内存分配线程锁*/
	myThreadLock *bgLock;   /**< 大内存分配线程锁*/
};


void innerCreateMemPool();
void innerDeleteMemPool();
MemoryManage* const innerGetMemPool();
void innerSetBigMemTotalSize(unsigned int size);

#endif

