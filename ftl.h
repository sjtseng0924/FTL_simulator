#define FTL_NULL 0xffffffff
#define MAX_BUFFER_SIZE 1024
#define HOT_DATA 0
#define COLD_DATA 1
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef int64_t			byte8;
typedef uint32_t	    byte4;
typedef unsigned char	byte1;	

typedef struct { 
	// byte8	sectors;
	byte8   LPage;
	byte4	state; // 0 = free, 1 = valid, 2 = invalid
} Page;

typedef struct {
	Page	*pages;
	byte4	eraseCnt;
	byte4	validPagesCnt; 
	byte4	GCNext;
	byte4   GCPrev;
	byte4   FreeNext;
} Block;

typedef struct {
	byte4	head;
	byte4	tail;
	byte4	cnt;

	byte4 usedPage;
} List;

typedef struct {
	byte4	pageNo;
	byte4	blockNo;
	byte1	used;
} Table;

// 存 hot跟cold目前正在被寫入的是哪個block，下一個page是什麼
typedef struct {
	byte4	blockNo;
	byte4	nextPage;
	byte1	valid;
	byte1	temperature;
} WriteFrontier;

typedef struct {
	byte8   *logicalPages; //在buffer中的logical page
	byte4   *prev;
	byte4   *next;
	byte1   *used; // 目前buffer中的這格是否有資料
	byte1   *inBuffer;// 某個 logical page是否在buffer中
	byte4   head;
	byte4   tail;
	byte4   count;
	byte4   capacity;
} WriteBuf;


typedef struct {
	byte8	sectorSize;
	byte8	pageSize;
	byte8	blockSize;
	byte8	PSize;
	byte8	LSize;

	byte8   blocksInL;  //blocks in logical
	byte8   blocksInP;
	byte8   pagesInBlock;
	byte8   sectorsInPage; 

	byte1 	isActualWrite;
	byte1   enableHotCold;
} Config;

typedef struct {
	byte8 actualWrite;
	byte8 hostWrite;
	byte8 eraseTimes; //block
} Data;

typedef struct {
	Config   config;
	Block    *blocks;
	List     *GCList;
	List     FreeList;
	Table    *mapTable;
	WriteBuf writeBuf;
	WriteFrontier hotFrontier;
	WriteFrontier coldFrontier;
	Data recordData;
} FTL; 


void FTLinit(FTL *FTLptr,FILE *fp);
void FTLfree(FTL *FTLptr);
void writeSector(FTL *FTLptr, byte8 sectorNum);
void flushBuffer(FTL *FTLptr);
void flushBufferedPage(FTL *FTLptr, byte8 logicalPage, byte1 temperature);
void markPageInvalid(FTL *FTLptr, Table *entry);
void WriteNewPage(FTL *FTLptr, byte4 *newPageNo, byte4 *newBlockNo, byte1 temperature);
void GC(FTL *FTLptr);
void getVictim(FTL *FTLptr, byte4 *victimBlock);
void GCMovePage(FTL *FTLptr, byte4 *oldBlock, byte4 *oldPage);
