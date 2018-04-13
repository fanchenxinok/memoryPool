#include "autoLock.h"
#include "memoryPool.h"

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

static MemoryManage* pMemPoolInstance = NULL;
#define BIG_MEMORY_SIZE (5 * 1024 * 1024) //  默认为5 M
#define SMALL_MEMORY_MAX_SIZE  (1024)  // 默认 1024 字节
/* 默认的大内存块个数 */
#define BIGNODE_N ((BIG_MEMORY_SIZE) / (SMALL_MEMORY_MAX_SIZE))

static int s_total_big_size = BIG_MEMORY_SIZE;

/* 存储小内存各档次大小数组定义 */
static int s_small_memory_size[] = {SML8_S,
								SML16_S, 
								SML32_S,
								SML64_S,
								SML128_S,
								SML256_S,
								SML512_S,
								SML1024_S,
								BIGNODE_S};  //bytes

/* 存储小内存各档次节点数数组定义 */								
static int s_small_memory_num[] = {SML8_N,
								SML16_N,
								SML32_N,
								SML64_N,
								SML128_N,
								SML256_N,
								SML512_N,
								SML1024_N,
								BIGNODE_N};


/* 构造函数初始化内存池 */
MemoryManage::MemoryManage()
{
	int total_small_size = 0, big_size = 0, small_node_num = 0;
	int i = 0;
	/* 小内存总链表数 */
	total_small_list_head = sizeof(s_small_memory_size) / sizeof(s_small_memory_size[0]);
	
	for(; i < total_small_list_head; ++i){
		total_small_size += s_small_memory_size[i] * s_small_memory_num[i];
		small_node_num += s_small_memory_num[i];
	}

	total_small_node_num = small_node_num;
	/* 4字节对齐 */
	total_big_size = ((s_total_big_size) & (~(sizeof(int) - 1))) + sizeof(int);
	total_small_size = (total_small_size & (~(sizeof(int) - 1))) + sizeof(int);
	cout << "small node total num = " << total_small_node_num << endl;
	cout << "total small  size: " << total_small_size;
	cout << "; total big size: " << total_big_size << endl;

	/*  System malloc all memory */
	firstAddr = malloc(total_small_size + total_big_size);		
	if(firstAddr == NULL){
		cout << "malloc all memory fail!" << endl;
		exit(-1);
	}

	bigMmFirstAddr = (unsigned char*)firstAddr + total_small_size;
	smlListFirstAddr = NULL;

	/* create small memory list */
	SmlMemInit();

	/* create big memory list */
	BigMemInit();

	/* new lock */
	smLock = new myThreadLock();
	bgLock = new myThreadLock();
}

/* 析构函数释放内存池 */
MemoryManage::~MemoryManage()
{
	BigMemDeInit();  /* free list */
	SmlMemDeInit(); 
	
	total_big_size = 0;
	bigMmFirstAddr = NULL;

	if(firstAddr != NULL){
		free(firstAddr);
		firstAddr = NULL;
	}

	free(smLock);
	smLock = NULL;
	free(bgLock);
	bgLock = NULL;
}

/* 对外提供的Malloc 接口 */
void* MemoryManage::Malloc(size_t size)
{
	void* addr = NULL;
	if(size <= 0 || size > total_big_size)
		return NULL;
	
	/* 如果需要分配的size 大于等于
	大内存每个block的最小size */
	if(size >= bg_block_list.minSizePerBlock){
		/* 锁住 */
		AutoMutex auto_mutex(bgLock);
		//cout << "malloc big memory!" << endl;
		return MallocBigMemory(size);
	}
	else{
		/* 锁住 */
		AutoMutex auto_mutex(smLock);
		//cout << "malloc i = " << i << endl;
		return MallocSmlMemory(size);
	}
	
	return addr;
}

/* 对外提供的Free 接口 */
void MemoryManage::Free(void* addr)
{
	if(addr == NULL)
		return;

	if(addr >= bigMmFirstAddr){
		/* 锁住 */
		AutoMutex auto_mutex(bgLock);
		//cout << "free big memory!" << endl;
		FreeBigMemory(addr);
	}
	else{
		/* 锁住 */
		AutoMutex auto_mutex(smLock);
		FreeSmlMemory(addr);
	}

	return;
}

/* 获得大内存链表头节点 */
pBigNode MemoryManage::GetBigListHead() const
{
	return bg_block_list.pBlockListHead;
}

/* 管理大内存的链表初始化 */
void MemoryManage::BigMemInit()
{
	bg_block_list.blockCount = 1;
	pBigNode pHead = (pBigNode)GetFreeSmNode(total_small_list_head - 1);
	if(pHead == NULL){
		cout << "BigMemInit() malloc pHead error!!!!" << endl;
		exit(-1);
	}
	
	bg_block_list.pBlockListHead = pHead;
	pHead->size = 0;
	pHead->start_addr = NULL;
	pHead->usedFlag = 1;
	pHead->prev = NULL;
	
	pBigNode pNew = (pBigNode)GetFreeSmNode(total_small_list_head - 1);
	if(pNew == NULL){
		cout << "BigMemInit() malloc pNew error!!!!" << endl;
		exit(-1);
	}

	pNew->next = NULL;
	pNew->size = total_big_size;
	pNew->start_addr = bigMmFirstAddr;
	pNew->usedFlag = 0;

	pHead->next = pNew;
	pNew->prev = pHead;
	bg_block_list.pSearchStartNode = pNew;

	#if TEST_BIGMM
	bg_block_list.unUsedBlockNum = 1;
	bg_block_list.remainSize = total_big_size;
	#endif
	bg_block_list.minSizePerBlock = (SMALL_MEMORY_MAX_SIZE  & (~(sizeof(int) - 1))) + sizeof(int);

	return;
}

/* 归还大内存链表节点的内存空间 */
void MemoryManage::BigMemDeInit()
{
	pBigNode pCur = bg_block_list.pBlockListHead;
	while(pCur){
		pBigNode pTmp = pCur;
		pCur = pCur->next;
		RetUsedSmNode(total_small_list_head - 1, pTmp);
		pTmp = NULL;
	}

	cout << "*************** BigMemDeInit Success !!!*******************" << endl;
}

void* MemoryManage::MallocBigMemory(size_t mem_size)
{
	size_t size = mem_size;
	/* 判断开始搜索的节点块大小是否满足要求 */
	if((bg_block_list.pSearchStartNode->size < size) || (bg_block_list.pSearchStartNode->usedFlag == 1))
		bg_block_list.pSearchStartNode = bg_block_list.pBlockListHead->next;  //如果不满足则从头开始寻找

	size_t sTmp = mem_size;
	pBigNode pCur = bg_block_list.pSearchStartNode;
	while(1)
	{
		if(pCur != NULL ){
			if(size < bg_block_list.minSizePerBlock)
				size = bg_block_list.minSizePerBlock;

			if((pCur->usedFlag == 0) && (pCur->size >= size)){	
				if((pCur->size - size) >= bg_block_list.minSizePerBlock){
					pBigNode pNew = (pBigNode)GetFreeSmNode(total_small_list_head - 1);
					if(pNew == NULL){
						cout << "********************************" << endl;
						return NULL;
					}

					/* 一片大内存区域最多只能
					形成BIG_MEMORY_BLOCK_MAX_NUM 块区域 */
					bg_block_list.blockCount++;
					if(bg_block_list.blockCount > s_small_memory_num[BIG_NODE]){
						cout << "@@@@@@@@@@@@@@@@@@@@@@@" << endl;
						return NULL;
					}

					pNew->next = pCur->next;
					if(pCur->next != NULL)
						pCur->next->prev = pNew;
					pNew->size = pCur->size - size;
					pNew->start_addr = (unsigned char*)pCur->start_addr + size;
					pNew->usedFlag = 0;

					pCur->usedFlag = 1;
					pCur->next = pNew;
					pNew->prev = pCur;
					pCur->size = size;

					bg_block_list.pSearchStartNode = pNew;
					
					#if TEST_BIGMM
					bg_block_list.remainSize -= size;
					#endif
					
					return pCur->start_addr;
				}
				else{
					pCur->usedFlag = 1;
					#if TEST_BIGMM
					bg_block_list.unUsedBlockNum--;
					bg_block_list.remainSize -= size;
					#endif
					return pCur->start_addr;
				}
			}
			else{
				pCur = pCur->next;
			}
		}
		else
			break;
	}

	cout << "Return NULL#########, sTmp = " << sTmp << ";size= " << size << endl;
	return NULL;
}

void MemoryManage::FreeBigMemory(void* addr)
{
	pBigNode pCur = bg_block_list.pBlockListHead->next;
	while(1)
	{
		if(pCur == NULL){
			cout << "free big memory error!!! " << endl;
			break;
		}

		if((pCur->usedFlag == 1) && (pCur->start_addr == addr)){
			#if TEST_BIGMM
			bg_block_list.remainSize += pCur->size;
			#endif
			/* free该节点时，判断它后面的节点是否被使用，如果未被使用则把它们合并成一个大内存块（内存碎片回收） */
			pBigNode pBack = pCur->next;
			pBigNode pPrev = pCur->prev;
			while((pBack != NULL) && (pBack->usedFlag == 0))
			{
				pCur->next = pBack->next;
				if(pBack->next != NULL)
					pBack->next->prev = pCur;
				pCur->size += pBack->size;
				pBigNode pfree = pBack;
				pBack = pBack->next;
				if(bg_block_list.pSearchStartNode == pfree){
					bg_block_list.pSearchStartNode = pCur;
				}
				
				RetUsedSmNode(total_small_list_head - 1 ,pfree);  /* free node */
				pfree = NULL;

				bg_block_list.blockCount--;
				#if TEST_BIGMM
				bg_block_list.unUsedBlockNum--;
				#endif
			}

			/* free该节点时，判断它前面的节点是否被使用，如果未被使用则把它们合并成一个大内存块（内存碎片回收） */
			while((pPrev != NULL) && (pPrev->usedFlag == 0))
			{
				pCur->prev = pPrev->prev;
				if(pPrev->prev != NULL)
					pPrev->prev->next = pCur;
				pCur->size += pPrev->size;
				pCur->start_addr = pPrev->start_addr;
				pBigNode pfree = pPrev;
				pPrev = pPrev->prev;
				if(bg_block_list.pSearchStartNode == pfree){
					bg_block_list.pSearchStartNode = pCur;
				}
				
				RetUsedSmNode(total_small_list_head - 1 ,pfree);  /* free node */
				pfree = NULL;
				
				bg_block_list.blockCount--;
				#if TEST_BIGMM
				bg_block_list.unUsedBlockNum--;
				#endif
			}

			pCur->usedFlag = 0;
			#if TEST_BIGMM
			bg_block_list.unUsedBlockNum++;
			#endif
			return;
		}
		else
			pCur = pCur->next;
	}
	return;
}

/* 小内存池链表的建立及初始化 */
void MemoryManage::SmlMemInit()
{
	/* 建立各个档次链表的头节点 */
	smlListFirstAddr = malloc(total_small_list_head * 2 * sizeof(SmNode)  /* total small memory list head space */
					+ total_small_node_num * sizeof(SmNode));   /* total small memory list node space*/
	if(smlListFirstAddr == NULL){
		cout << "malloc smlListFirstAddr fail!!! " << endl;
		exit(-1);

	}

	void* smListNodeAddr = smlListFirstAddr;

	memset(smListNodeAddr, 0, sizeof(SmNode) * total_small_list_head * 2);
	sm_block_list.pFreeList = (pSmNode)smListNodeAddr;
	sm_block_list.pUsedList = (pSmNode)(sm_block_list.pFreeList + total_small_list_head);

	sm_block_list.pBlockSize = (unsigned int*)malloc(sizeof(unsigned int) * total_small_list_head);
	if(sm_block_list.pBlockSize == NULL){
		cout << "malloc pBlockSize error!!" << endl;
		free(smlListFirstAddr);
		smlListFirstAddr = NULL;
		exit(-1);
	}

	memset(sm_block_list.pBlockSize, 0, sizeof(unsigned int) * total_small_list_head);
	
	sm_block_list.pBlockFirstAddr = (void**)malloc(sizeof(void*) * total_small_list_head);
	if(sm_block_list.pBlockFirstAddr == NULL){
		cout << "malloc blockfirst addr error!!" << endl;
		free(sm_block_list.pBlockSize);
		sm_block_list.pBlockSize = NULL;
		free(smlListFirstAddr);
		smlListFirstAddr = NULL;
		exit(-1);
	}

	memset(sm_block_list.pBlockFirstAddr, 0, sizeof(void*) * total_small_list_head);

	pSmNode pNodeAddr = (pSmNode)smListNodeAddr + total_small_list_head * 2;
	/*  create small memory list */
	int count = 0, i = 0;
	for(i = 0; i < total_small_list_head; ++i)
	{
		int j = 0;
		static int preCount = 0;
		pSmNode pTmp = &sm_block_list.pFreeList[i];
		sm_block_list.pFreeList[i].next = NULL;
		sm_block_list.pFreeList[i].start_addr = NULL;
		for(j = 0; j < s_small_memory_num[i]; ++j)
		{
			pSmNode pNew = pNodeAddr;
			++pNodeAddr;
			pNew->start_addr = (unsigned char*)firstAddr + count;
			pNew->next = NULL;
			count += s_small_memory_size[i];
			pTmp->next = pNew;
			pTmp = pNew;
		}
		sm_block_list.pBlockSize[i] = count - preCount;
		preCount = count;
		sm_block_list.pBlockFirstAddr[i] = sm_block_list.pFreeList[i].next->start_addr;
	}

	return;
}

void MemoryManage::SmlMemDeInit()
{
	free(sm_block_list.pBlockSize);
	sm_block_list.pBlockSize = NULL;
	
	free(sm_block_list.pBlockFirstAddr);
	sm_block_list.pBlockFirstAddr = NULL;

	free(smlListFirstAddr);
	smlListFirstAddr = NULL;

	cout << "*************** SmlMemDeInit Success !!!*******************" << endl;
}

/* 获得小内存池第index 个的free 链表的节点
    并将该节点标记为used, 转移到used 链表中去*/
void* MemoryManage::GetFreeSmNode(int index)
{
	if(index >= total_small_list_head){
		cout << "invail index !!!!!!!!" << endl;
		return NULL;
	}
	
	int i = index;
	void* addr = sm_block_list.pFreeList[i].next->start_addr;
	pSmNode ptmp = sm_block_list.pFreeList[i].next;
	sm_block_list.pFreeList[i].next = sm_block_list.pFreeList[i].next->next;

	if(ptmp == sm_block_list.pUsedList[i].next){
		cout << "very fatal error!!!!  ptmp == sm_block_list.pUsedList[i].next" << endl;
		exit(-1);
	}
	ptmp->next = sm_block_list.pUsedList[i].next;
	sm_block_list.pUsedList[i].next = ptmp;

	return addr;
}

/* 将节点从used 链表转移到free 链表中去 */
void MemoryManage::RetUsedSmNode(int index, void* addr)
{
	if(index >= total_small_list_head){
		cout << "invail index !!!!!!!!" << endl;
		return;
	}
	
	int i = index;
	pSmNode pCur = sm_block_list.pUsedList[i].next;
	pSmNode pPre = &sm_block_list.pUsedList[i];
	while(1)
	{
		if(pCur == NULL)
			break;
		if(pCur->start_addr == addr)
			break;
		pPre = pCur;
		pCur = pCur->next;
		if(pPre == pCur){
			cout << "firstAddr = " << firstAddr << ", pCurAddr = " << pCur->start_addr << endl;
			exit(-1);
		}
		//cout << "free small memory find node, pCur = " << pCur << endl;
	}

	if(pCur != NULL){
		pPre->next = pCur->next;
		pCur->next = sm_block_list.pFreeList[i].next;
		sm_block_list.pFreeList[i].next = pCur;
	}
	else{
		cout << "i = " << i << " free small memory fail!!!!!" << endl;
	}

	return;
}

void* MemoryManage::MallocSmlMemory(size_t size)
{
	int i = 0; 
	for(; i < total_small_list_head - 1; i++){
		if(size <= s_small_memory_size[i])
			break;
	}

	while(1)
	{
		/* if this size free list have free node */
		if(sm_block_list.pFreeList[i].next != NULL){
			break;
		}
		else{  /* if this size free list doesn't have free node, then go to search big size free list */
			++i;
			if(i >= total_small_list_head - 1){  /* if all big size free list is used */
				cout << "small  freeList is FULL!!!!!!!!!!!" << endl;
				return NULL;
			}
		}
	}

	return GetFreeSmNode(i);
}

void MemoryManage::FreeSmlMemory(void *addr)
{
	int i = 0; 
	for(; i < total_small_list_head - 1; i++)
	{
		if((addr >= sm_block_list.pBlockFirstAddr[i]) 
			&& (addr < (unsigned char*)sm_block_list.pBlockFirstAddr[i] + sm_block_list.pBlockSize[i])){
			break;
		}
	}

	RetUsedSmNode(i, addr);

	return;
}


void innerCreateMemPool()
{
	if(pMemPoolInstance) return;
	pMemPoolInstance = new MemoryManage();
	return;
}

void innerDeleteMemPool()
{
	delete pMemPoolInstance;
	return;
}

MemoryManage* const innerGetMemPool()
{
	return pMemPoolInstance;
}

void innerSetBigMemTotalSize(unsigned int size)
{
	s_total_big_size = size;
	int block_num = s_total_big_size / SMALL_MEMORY_MAX_SIZE;
	s_small_memory_num[BIG_NODE] = block_num;
}


