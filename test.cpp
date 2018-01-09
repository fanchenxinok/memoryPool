#include <iostream>
#include "memPoolApi.h"
using namespace std;
/* test memory pool */
int main()
{
	apl_createMemPool(5*1024*1024);
	int *p = (int*)apl_malloc(sizeof(int) * 10);
	if(p){
		int i = 0;
		for(; i < 10; i++)
			p[i] = i;
	}
	cout << p[8] << endl;
	apl_free(p);
	p = NULL;
	//apl_deleteMemPool();
	apl_resetBigMemSize(10*1024*1024);
	p = (int*)apl_malloc(10*1024);
	apl_free(p);
	p = NULL;
	apl_deleteMemPool();
	return 0;
}
