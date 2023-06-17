#include "disk.h"
#include <stdio.h>
#include <assert.h>

FILE* disk_ptr = NULL;

void disk_create() {
    FILE* fptr = fopen(DISK_NAME, "wr");
    assert(fptr != NULL && "make_disk: cannot open file");

    char buff[BLOCK_SIZE];

    memset(buff, 0, BLOCK_SIZE);
    for (uint32_t cnt = 0; cnt < NUM_BLOCKS; ++cnt){
        fwrite(buff, sizeof(char), BLOCK_SIZE, fptr);
    }

    fclose(fptr);
}

void disk_start() {
    FILE* fptr = fopen(DISK_NAME, "r+b");
    assert(fptr != NULL && "disk_start: cannot open file");
    assert(disk_ptr == NULL && "disk_start: another disk is already started");

    // calculate file size
//    fseek(fptr, 0L, SEEK_END);
//    long sz = ftell(fptr);
//    printf("file size: %ld\n", sz);
//    fseek(fptr, 0L, SEEK_SET);

    disk_ptr = fptr;
}

void disk_close(){
    assert(disk_ptr != NULL && "disk_close: cannot close uninitialized disk");
    fclose(disk_ptr);
    disk_ptr = NULL;
}

void disk_write_block(uint block_idx, char* buff) {
    assert(disk_ptr != NULL && "disk_write_block: cannot close uninitialized disk");
    assert(block_idx < NUM_BLOCKS && "disk_write_block: block out of bounds");
    assert(fseek(disk_ptr, block_idx*BLOCK_SIZE, SEEK_SET) == 0 && "disk_write_block: fseek failed");

    fwrite(buff, sizeof(char), BLOCK_SIZE, disk_ptr);
}

void disk_read_block(uint block_idx, char* buff) {
    assert(disk_ptr != NULL && "disk_write_block: cannot close uninitialized disk");
    assert(block_idx < NUM_BLOCKS && "disk_write_block: block out of bounds");
    assert(fseek(disk_ptr, block_idx*BLOCK_SIZE, SEEK_SET) == 0 && "disk_write_block: fseek failed");

    fread(buff, sizeof(char), BLOCK_SIZE, disk_ptr);
}