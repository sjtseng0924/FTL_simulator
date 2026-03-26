#include "ftl.h"
#include <stdlib.h>
#include <stdio.h>

void print(FTL *FTLptr) {
    for(int i=0;i<0;i++) {
        if(FTLptr->mapTable[i].used) {
            printf("%d: %d block %d page\n",i , FTLptr->mapTable[i].blockNo, FTLptr->mapTable[i].pageNo);
        }
    }
    printf("%u\n", FTLptr->FreeList.cnt);
    printf("%u\n", FTLptr->FreeList.head);
    printf("%u\n", FTLptr->GCList[0].cnt);
    printf("%u\n", FTLptr->GCList[0].head);
}

int main() {
    FTL ftl;

    FILE *TraceFile, *ConfigFile;
    char *config_file = "./files/config.txt";
    char *trace_file  = "./files/rawfile0_20G.txt";

    printf("Initializing %s ...\n", config_file);
    ConfigFile = fopen(config_file, "r");
    FTLinit(&ftl, ConfigFile);

    // printf("Logical Addree Size: %lld, Phy Addr size: %lld\n", ftl.config.LSize, ftl.config.PSize);

    char operation[100], buffer[100];
    byte8 tag, sectorNum, len, cnt;
    TraceFile = fopen(trace_file, "r");
    // int cnt = 0;

    printf("Fill Logical Block (%lld bytes) ...\n", ftl.config.LSize);
    // fill logical size
    sectorNum = 0;
    len = 33554432; // 16G / 512B
    ftl.config.isActualWrite = 0;
    while(len > 0){
        writeSector(&ftl, sectorNum);
        sectorNum += 1;
        len -= 1;
    }
    flushBuffer(&ftl);
    
    // sectorNum = 0;
    // len = 33554432; // 16G / 512B
    // cnt = 0;
    // while(len > 0){
    //     cnt++;
    //     printf("%lld\n",cnt);
    //     ftl.recordData.hostWrite++;
    //     writeSector(&ftl, sectorNum);
    //     sectorNum += 24;
    //     len -= 24;
    //     if(ftl.FreeList.cnt <= 100) {
    //         GC(&ftl);
    //     }
    // }

    printf("Start %s ...\n", trace_file);
    // start for consume trace
    cnt = 0;
    ftl.config.isActualWrite = 1;
    while(fgets(buffer, 100, TraceFile) != NULL){
        cnt++;
    //     if(cnt>209900) {
    //         printf("FreeList cnt: %u\n"
    //    "GCList 0 cnt: %u\n"
    //    "GCList 1 cnt: %u\n"
    //    "buf logical: %lld\n",
    //    ftl.FreeList.cnt,
    //    ftl.GCList[0].cnt,
    //    ftl.GCList[1].cnt,
    //    ftl.writeBuf.currentPage);

    //     }
        // printf("%lld %lld\n", cnt,ftl.FreeList.cnt);
        char temp_len[32];
        sscanf(buffer, "%lld %99s %lld %31s", &tag, operation, &sectorNum, temp_len);
        len = (byte8) atof(temp_len);

        // If over the bound, in this simulation, I will ignore that operation.
        if(sectorNum + len > ftl.config.LSize / ftl.config.sectorSize || len == 0){
            // printf("X");
            continue;
        } 

        // In this simulation, We just deal with the write request.
        switch(operation[0]) {
        case 'R':
            break;
        case 'W':
            while(1){
                ftl.recordData.hostWrite++;
                writeSector(&ftl, sectorNum);
                sectorNum += 1;
                len -= 1;

                // threshold for GC
                if(ftl.FreeList.cnt <= 100) {
                    GC(&ftl);
                }

                if(len <= 0)break;
            }
            break;
        default:
            break;
        }
    } 

    flushBuffer(&ftl);
    fclose(TraceFile);

    ftl.recordData.actualWrite *= ftl.config.pageSize;
    ftl.recordData.hostWrite *= ftl.config.sectorSize;
    printf("host    write: %lld bytes\n"
           "actual  write: %lld bytes\n"
           "amplification: %.6f\n"
           , ftl.recordData.hostWrite, ftl.recordData.actualWrite
           , ftl.recordData.hostWrite == 0 ? 0.0 :
             (double)ftl.recordData.actualWrite / (double)ftl.recordData.hostWrite);

    
        
    FTLfree(&ftl);
}
