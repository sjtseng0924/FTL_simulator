#include "ftl.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// 確定某個 block 不是正在被寫入的 hot block 或 cold block
static byte1 isOpenBlock(FTL *FTLptr, byte4 blockNo) {
    return (FTLptr->hotFrontier.valid && FTLptr->hotFrontier.blockNo == blockNo) ||
           (FTLptr->coldFrontier.valid && FTLptr->coldFrontier.blockNo == blockNo);
}

static void addBlockToGCList(FTL *FTLptr, byte4 blockNo) {
    Block *block = &(FTLptr->blocks[blockNo]);
    List *list = &(FTLptr->GCList[block->validPagesCnt]);
    block->GCNext = FTL_NULL;
    block->GCPrev = FTL_NULL;
    if (list->cnt == 0) {
        list->head = blockNo;
        list->tail = blockNo;
    } else {
        byte4 tailBlockNo = list->tail;
        FTLptr->blocks[tailBlockNo].GCNext = blockNo;
        block->GCPrev = tailBlockNo;
        list->tail = blockNo;
    }
    list->cnt++;
}

static void removeBlockFromGCList(FTL *FTLptr, byte4 blockNo, byte4 validPagesCnt) {
    Block *block = &(FTLptr->blocks[blockNo]);
    List *list = &(FTLptr->GCList[validPagesCnt]);

    if (list->cnt == 0) {
        return;
    }

    if (block->GCPrev != FTL_NULL) {
        FTLptr->blocks[block->GCPrev].GCNext = block->GCNext;
    } else if (list->head == blockNo) {
        list->head = block->GCNext;
    } else {
        return;
    }

    if (block->GCNext != FTL_NULL) {
        FTLptr->blocks[block->GCNext].GCPrev = block->GCPrev;
    } else if (list->tail == blockNo) {
        list->tail = block->GCPrev;
    }

    if (list->head == FTL_NULL) {
        list->tail = FTL_NULL;
    }
    if (list->tail == FTL_NULL) {
        list->head = FTL_NULL;
    }

    list->cnt--;
    block->GCNext = FTL_NULL;
    block->GCPrev = FTL_NULL;
}

static byte4 popBufferHead(WriteBuf *wbuf, byte8 *logicalPage) {
    byte4 idx = wbuf->head;
    *logicalPage = wbuf->logicalPages[idx];
    wbuf->head = wbuf->next[idx];
    if (wbuf->head != FTL_NULL) {
        wbuf->prev[wbuf->head] = FTL_NULL;
    } else {
        wbuf->tail = FTL_NULL;
    }
    wbuf->used[idx] = 0;
    wbuf->prev[idx] = FTL_NULL;
    wbuf->next[idx] = FTL_NULL;
    wbuf->count--;
    return idx;
}

void writeSector(FTL *FTLptr, byte8 sectorNum) {
    Config *C = &(FTLptr->config);
    WriteBuf *wbuf = &(FTLptr->writeBuf);
    byte8 logicalPage = sectorNum / C->sectorsInPage;

    if (wbuf->inBuffer[logicalPage]) {
        return;
    }
    // buffer 滿的時候 flush 最舊的 page
    if (wbuf->count == wbuf->capacity) {
        byte8 oldestPage;
        popBufferHead(wbuf, &oldestPage);
        wbuf->inBuffer[oldestPage] = 0;
        flushBufferedPage(FTLptr, oldestPage, HOT_DATA);
    }

    byte4 idx = FTL_NULL;
    for (byte4 i = 0; i < wbuf->capacity; i++) {
        if (!wbuf->used[i]) {
            idx = i;
            wbuf->used[i] = 1;
            wbuf->prev[i] = FTL_NULL;
            wbuf->next[i] = FTL_NULL;
            break;
        }
    }
    assert(idx != FTL_NULL);

    wbuf->logicalPages[idx] = logicalPage;
    if (wbuf->tail == FTL_NULL) {
        wbuf->head = idx;
        wbuf->tail = idx;
    } else {
        wbuf->next[wbuf->tail] = idx;
        wbuf->prev[idx] = wbuf->tail;
        wbuf->tail = idx;
    }
    wbuf->count++;
    wbuf->inBuffer[logicalPage] = 1;
}

void flushBufferedPage(FTL *FTLptr, byte8 logicalPage, byte1 temperature) {
    if (FTLptr->config.isActualWrite == 1) {
        FTLptr->recordData.actualWrite++;
    }

    Table *maptable = FTLptr->mapTable;
    if (maptable[logicalPage].used == 1) {
        markPageInvalid(FTLptr, &maptable[logicalPage]);
    }

    byte4 newPageNo, newBlockNo;
    WriteNewPage(FTLptr, &newPageNo, &newBlockNo, temperature);

    FTLptr->blocks[newBlockNo].pages[newPageNo].LPage = logicalPage;
    maptable[logicalPage].pageNo = newPageNo;
    maptable[logicalPage].blockNo = newBlockNo;
    maptable[logicalPage].used = 1;
}

// 把整個 buffer flush，trace 讀完的時候會用到
void flushBuffer(FTL *FTLptr) {
    WriteBuf *wbuf = &(FTLptr->writeBuf);
    while (wbuf->count > 0) {
        byte8 logicalPage;
        popBufferHead(wbuf, &logicalPage);
        wbuf->inBuffer[logicalPage] = 0;
        flushBufferedPage(FTLptr, logicalPage, HOT_DATA);
    }
}

void markPageInvalid(FTL *FTLptr, Table *entry) {
    byte4 blockNo = entry->blockNo;
    Block *block = &(FTLptr->blocks[blockNo]);
    byte4 oldValidPagesCnt = block->validPagesCnt;
    if (block->pages[entry->pageNo].state != 1) {
        return;
    }
    block->pages[entry->pageNo].state = 2;
    assert(block->validPagesCnt > 0);
    block->validPagesCnt--;
    if (isOpenBlock(FTLptr, blockNo)) {
        return;
    }
    removeBlockFromGCList(FTLptr, blockNo, oldValidPagesCnt);
    addBlockToGCList(FTLptr, blockNo);
}

void WriteNewPage(FTL *FTLptr, byte4 *newPageNo, byte4 *newBlockNo, byte1 temperature) {
    if (!FTLptr->config.enableHotCold) {
        temperature = HOT_DATA;
    }
    WriteFrontier *frontier = (temperature == COLD_DATA)
        ? &(FTLptr->coldFrontier)
        : &(FTLptr->hotFrontier);
    // 目前的 frontier 還沒有綁定一個 block
    // 就從 FreeList 拿一個 block 來寫
    if (!frontier->valid) {
        assert(FTLptr->FreeList.cnt > 0);
        assert(FTLptr->FreeList.head != FTL_NULL);
        frontier->blockNo = FTLptr->FreeList.head;
        Block *newBlock = &(FTLptr->blocks[frontier->blockNo]);
        FTLptr->FreeList.head = newBlock->FreeNext;
        if (FTLptr->FreeList.head == FTL_NULL) {
            FTLptr->FreeList.tail = FTL_NULL;
        }
        FTLptr->FreeList.cnt--;
        newBlock->FreeNext = FTL_NULL;
        newBlock->validPagesCnt = 0;
        newBlock->GCNext = FTL_NULL;
        newBlock->GCPrev = FTL_NULL;
        for (byte4 i = 0; i < FTLptr->config.pagesInBlock; i++) {
            newBlock->pages[i].state = 0;
        }
        frontier->nextPage = 0;
        frontier->valid = 1;
        frontier->temperature = temperature;
    }

    *newBlockNo = frontier->blockNo;
    *newPageNo = frontier->nextPage;
    FTLptr->blocks[*newBlockNo].pages[*newPageNo].state = 1;
    FTLptr->blocks[*newBlockNo].validPagesCnt++;
    frontier->nextPage++;
    // 這個 block 寫滿了，下一次要換一個 block 來寫
    if (frontier->nextPage >= FTLptr->config.pagesInBlock) {
        addBlockToGCList(FTLptr, frontier->blockNo);
        frontier->blockNo = FTL_NULL;
        frontier->nextPage = 0;
        frontier->valid = 0;
    }
}
