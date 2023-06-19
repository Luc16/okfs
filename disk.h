#ifndef OKFS_DISK_H
#define OKFS_DISK_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define BLOCK_SIZE (1 << 10)
//#define NUM_BLOCKS (1 << 20)
#define NUM_BLOCKS (1 << 15)
#define DISK_NAME "../okfs.disk"

void disk_create();
void disk_start();
void disk_close();
void disk_write_block(uint32_t block_idx, char* buff);
void disk_read_block(uint32_t block_idx, char* buff);
void disk_write(uint32_t block_idx, uint32_t block_off, char* buff, uint32_t size);
void disk_read(uint32_t block_idx, uint32_t block_off, char* buff, uint32_t size);



#endif //OKFS_DISK_H
