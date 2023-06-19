#ifndef OKFS_OKFS_H
#define OKFS_OKFS_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "disk.h"

#define MAX_FILE_NAME_SIZE 50
#define INODE_BLOCKS 64

typedef struct {
    uint32_t block_idx, block_off;
} InodeIdx;

typedef struct {
    InodeIdx children;
    InodeIdx next;
    InodeIdx parent;
    InodeIdx idx;
    bool isDirectory;
    char name[MAX_FILE_NAME_SIZE];
    uint32_t file_idx;
    uint32_t size;
} Inode_t;

typedef Inode_t* Inode;

typedef struct {
    uint32_t idx;
    uint32_t next;
    uint32_t size;
} EmptyBlockData;

void okfs_create();
void okfs_mount();
void okfs_unmount();
int okfs_mkdir(char name[50]);
void okfs_ls();
int okfs_cd(char name[50]);
void okfs_print_cur_dir_path();
int okfs_mkfile(char name[MAX_FILE_NAME_SIZE], char* content, size_t size);
int okfs_cat(char name[MAX_FILE_NAME_SIZE]);
int okfs_delfile(char name[MAX_FILE_NAME_SIZE]);
int okfs_cgfile(char name[MAX_FILE_NAME_SIZE], char* content, size_t size);

/* TODO:
 * mv nome dir
 * cp nome dir
 * */

#endif //OKFS_OKFS_H
