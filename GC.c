#include "ftl.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// Greedy
void GC(FTL *FTLptr) {
    flushBuffer(FTLptr);
    // get victim
    byte4 victimBlock;
    getVictim(FTLptr, &victimBlock);
    // move valid data
    Config *C = &(FTLptr->config);
    for(byte4 i = 0; i<C->pagesInBlock; i++) {
        if(FTLptr->blocks[victimBlock].pages[i].state == 1) {
            GCMovePage(FTLptr, &victimBlock, &i);
        }
    }
    // update victim block
    FTLptr->blocks[victimBlock].eraseCnt++;
    FTLptr->recordData.eraseTimes++;
    FTLptr->blocks[victimBlock].validPagesCnt = 0;
    for(byte4 i = 0; i<C->pagesInBlock; i++) {
        FTLptr->blocks[victimBlock].pages[i].state = 0;
    }
    FTLptr->blocks[victimBlock].GCNext = FTL_NULL;
    FTLptr->blocks[victimBlock].GCPrev = FTL_NULL;
    // join to FreeList
    FTLptr->blocks[victimBlock].FreeNext = FTL_NULL;
    if (FTLptr->FreeList.cnt == 0) {
        FTLptr->FreeList.head = victimBlock;
        FTLptr->FreeList.tail = victimBlock;
    } else {
        FTLptr->blocks[FTLptr->FreeList.tail].FreeNext = victimBlock;
        FTLptr->FreeList.tail = victimBlock;
    }
    FTLptr->FreeList.cnt++;
}


void getVictim(FTL *FTLptr, byte4 *victimBlock) {
    for(byte8 i=0;i<=FTLptr->config.pagesInBlock;i++) {
        if(FTLptr->GCList[i].cnt > 0) {
            *victimBlock = FTLptr->GCList[i].head;
            FTLptr->GCList[i].cnt--;
            // update list
            if(FTLptr->GCList[i].cnt!=0) {
                FTLptr->GCList[i].head = FTLptr->blocks[*victimBlock].GCNext;
            }
            break;
        }
    }
}

void GCMovePage(FTL *FTLptr, byte4 *oldBlock, byte4 *oldPage) {
    if(FTLptr->config.isActualWrite == 1) FTLptr->recordData.actualWrite++;      
    byte8 LPageNo =  FTLptr->blocks[*oldBlock].pages[*oldPage].LPage;
    FTLptr->blocks[*oldBlock].pages[*oldPage].state = 2;
    byte4 newPageNo, newBlockNo;
    // GC中還valid的block會被當作是cold data
    WriteNewPage(FTLptr, &newPageNo, &newBlockNo, COLD_DATA);
    FTLptr->blocks[newBlockNo].pages[newPageNo].LPage = LPageNo;
    Table *maptable = (FTLptr->mapTable);
    maptable[LPageNo].pageNo  = newPageNo;
    maptable[LPageNo].blockNo = newBlockNo;
    
}
