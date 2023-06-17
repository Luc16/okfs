#ifndef OKFS_DISK_H
#define OKFS_DISK_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#define BLOCK_SIZE 1 << 10
//#define NUM_BLOCKS 1 << 20
#define NUM_BLOCKS 1 << 15
#define DISK_NAME "../okfs.disk"

void disk_create();
void disk_start();
void disk_close();
void disk_write_block(uint block_idx, char* buff);
void disk_read_block(uint block_idx, char* buff);



#endif //OKFS_DISK_H
