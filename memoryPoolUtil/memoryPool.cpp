#include "autoLock.h"
#include "memoryPool.h"

/*************************************************
(1)С�ڴ�:
     pUsedList[]: [8]  [16]  [32]  [64]  [128]......
			   �� ��  ��   ��   ��
			  [8]  [16]  [32]  [64]  [128]......
			   �� ��  ��   ��   ��
			  [8]  [16]  [32]  [64]  [128]
			    .......
			    
     pFreeList[]: [8]  [16]  [32]  [64]  [128]......
			   �� ��  ��   ��   ��
			  [8]  [16]  [32]  [64]  [128]......
			   �� ��  ��   ��   ��
			  [8]  [16]  [32]  [64]  [128]
			    .......

(2)���ڴ�:
	����ṹ: pBlockListHead[0]<->block1[size1]<->block2[size2]<->.........
	
	�����ڴ�: ����ڴ����һ��˫������������ÿ�η����ҵ�һ�����нڵ�A��
	�жϸÿ��нڵ�A ��size�Ƿ�������Ҫ����Ĵ�С������ýڵ�A ��
	�����Ժ���ʣ����ڻ������СblockSize ���½����ڵ�B��Ȼ��Ѹ�
	�ڵ�A ��ʹ�ñ�־��Ϊ1���½��ڵ�B �Ĵ�СΪA �ڵ�����ʣ����ڴ�ռ��С��

	�ͷ��ڴ�: �ҵ�Ҫ�ͷŵ��ڴ��ַ����Ľڵ㣬���ýڵ��ʹ�ñ�־��Ϊ0��
	ͬʱ�жϸýڵ��ǰ��ڵ��Ƿ�ʹ�ã����û�б�ʹ����ǰ��û��ʹ�õĽڵ�
	�ϲ�Ϊһ���ڵ�ʹ�ýڵ��size����С���ڴ���Ի��ա�
*************************************************/

static MemoryManage* pMemPoolInstance = NULL;
#define BIG_MEMORY_SIZE (5 * 1024 * 1024) //  Ĭ��Ϊ5 M
#define SMALL_MEMORY_MAX_SIZE  (1024)  // Ĭ�� 1024 �ֽ�
/* Ĭ�ϵĴ��ڴ����� */
#define BIGNODE_N ((BIG_MEMORY_SIZE) / (SMALL_MEMORY_MAX_SIZE))

static int s_total_big_size = BIG_MEMORY_SIZE;

/* �洢С�ڴ�����δ�С���鶨�� */
static int s_small_memory_size[] = {SML8_S,
								SML16_S, 
								SML32_S,
								SML64_S,
								SML128_S,
								SML256_S,
								SML512_S,
								SML1024_S,
								BIGNODE_S};  //bytes

/* �洢С�ڴ�����νڵ������鶨�� */								
static int s_small_memory_num[] = {SML8_N,
								SML16_N,
								SML32_N,
								SML64_N,
								SML128_N,
								SML256_N,
								SML512_N,
								SML1024_N,
								BIGNODE_N};


/* ���캯����ʼ���ڴ�� */
MemoryManage::MemoryManage()
{
	int total_small_size = 0, big_size = 0, small_node_num = 0;
	int i = 0;
	/* С�ڴ��������� */
	total_small_list_head = sizeof(s_small_memory_size) / sizeof(s_small_memory_size[0]);
	
	for(; i < total_small_list_head; ++i){
		total_small_size += s_small_memory_size[i] * s_small_memory_num[i];
		small_node_num += s_small_memory_num[i];
	}

	total_small_node_num = small_node_num;
	/* 4�ֽڶ��� */
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

/* ���������ͷ��ڴ�� */
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

/* �����ṩ��Malloc �ӿ� */
void* MemoryManage::Malloc(size_t size)
{
	void* addr = NULL;
	if(size <= 0 || size > total_big_size)
		return NULL;
	
	/* �����Ҫ�����size ���ڵ���
	���ڴ�ÿ��block����Сsize */
	if(size >= bg_block_list.minSizePerBlock){
		/* ��ס */
		AutoMutex auto_mutex(bgLock);
		//cout << "malloc big memory!" << endl;
		return MallocBigMemory(size);
	}
	else{
		/* ��ס */
		AutoMutex auto_mutex(smLock);
		//cout << "malloc i = " << i << endl;
		return MallocSmlMemory(size);
	}
	
	return addr;
}

/* �����ṩ��Free �ӿ� */
void MemoryManage::Free(void* addr)
{
	if(addr == NULL)
		return;

	if(addr >= bigMmFirstAddr){
		/* ��ס */
		AutoMutex auto_mutex(bgLock);
		//cout << "free big memory!" << endl;
		FreeBigMemory(addr);
	}
	else{
		/* ��ס */
		AutoMutex auto_mutex(smLock);
		FreeSmlMemory(addr);
	}

	return;
}

/* ��ô��ڴ�����ͷ�ڵ� */
pBigNode MemoryManage::GetBigListHead() const
{
	return bg_block_list.pBlockListHead;
}

/* ������ڴ�������ʼ�� */
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

/* �黹���ڴ�����ڵ���ڴ�ռ� */
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
	/* �жϿ�ʼ�����Ľڵ���С�Ƿ�����Ҫ�� */
	if((bg_block_list.pSearchStartNode->size < size) || (bg_block_list.pSearchStartNode->usedFlag == 1))
		bg_block_list.pSearchStartNode = bg_block_list.pBlockListHead->next;  //������������ͷ��ʼѰ��

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

					/* һƬ���ڴ��������ֻ��
					�γ�BIG_MEMORY_BLOCK_MAX_NUM ������ */
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
			/* free�ýڵ�ʱ���ж�������Ľڵ��Ƿ�ʹ�ã����δ��ʹ��������Ǻϲ���һ�����ڴ�飨�ڴ���Ƭ���գ� */
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

			/* free�ýڵ�ʱ���ж���ǰ��Ľڵ��Ƿ�ʹ�ã����δ��ʹ��������Ǻϲ���һ�����ڴ�飨�ڴ���Ƭ���գ� */
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

/* С�ڴ������Ľ�������ʼ�� */
void MemoryManage::SmlMemInit()
{
	/* �����������������ͷ�ڵ� */
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

/* ���С�ڴ�ص�index ����free ����Ľڵ�
    �����ýڵ���Ϊused, ת�Ƶ�used ������ȥ*/
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

/* ���ڵ��used ����ת�Ƶ�free ������ȥ */
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


