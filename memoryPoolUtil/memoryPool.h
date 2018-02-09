#ifndef __MY_MEM_POOL__
#define __MY_MEM_POOL__
#include "autoLock.h"
#include "comDef.h"

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
	void **pBlockFirstAddr; /* ����С�ڴ�ظ�������size �������ڴ����ʼ��ַ */
	unsigned int *pBlockSize; /* С�ڴ�ظ�������size �����ڴ���ܴ�С */
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
	unsigned int blockCount;       //��ǰ��ʹ�ú�δʹ�õ�block����
#if TEST_BIGMM  /* for test */
	unsigned int remainSize;       //����δ��ʹ�õ����ڴ�
	unsigned int unUsedBlockNum;   //��ǰδ��ʹ�õ�block�ĸ���
#endif
	unsigned int minSizePerBlock;  //ÿ��block��С�ķ���size
}st_BigBlockList;

/* С�ڴ������size���壬���һ�����ڷ�����ڴ�����ڵ� */
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
	BIGNODE_S = sizeof(BigNode)  /* ���ڴ�����ڵ��С */
};


/* С�ڴ�����νڵ��� */
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


class MemoryManage   //�ڴ����
{
public:
	MemoryManage();
	~MemoryManage();
	void* Malloc(size_t size);
	void Free(void* addr);
	pBigNode GetBigListHead() const;
private:
	MemoryManage(const MemoryManage&);  /* ���������캯���͸�ֵ��������Ϊprivate����ֹ����������� */
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

	void* firstAddr;              /* ��¼�ڴ���׵�ַ */
	void* smlListFirstAddr;  /* ��¼����С�ڴ��������׵�ַ */
	int total_small_list_head;  /* С�ڴ���ܵ������� */
	int total_small_node_num; /* С�ڴ���ܵ�����ڵ��� */

	int total_big_size;
	void* bigMmFirstAddr;   /* ��¼���ڴ�ص��׵�ַ */

	st_SmBlockList sm_block_list;
	st_BigBlockList bg_block_list;

	myThreadLock *smLock;  /**< С�ڴ�����߳���*/
	myThreadLock *bgLock;   /**< ���ڴ�����߳���*/
};


void innerCreateMemPool();
void innerDeleteMemPool();
MemoryManage* const innerGetMemPool();
void innerSetBigMemTotalSize(unsigned int size);

#endif

