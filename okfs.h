#ifndef OKFS_OKFS_H
#define OKFS_OKFS_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define MAX_FILE_NAME_SIZE 50

typedef struct s_Inode_t {
    struct s_Inode_t* children;
    struct s_Inode_t* next;
    struct s_Inode_t* parent;
    bool isDirectory;
    char name[MAX_FILE_NAME_SIZE];
    void* data;
    uint32_t size;
} Inode_t;

typedef Inode_t* Inode;

/* TODO:
 * mkdir nome
 * mkfile nome "conteudo"
 * cgfile nome "novo conteudo"
 * delfile nome
 * cp nome dir
 * ls
 * */

#endif //OKFS_OKFS_H
