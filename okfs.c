#include "okfs.h"

Inode global_head = NULL;
Inode current_dir = NULL;
int cur_depth;
EmptyBlockData empty_block_head;
InodeIdx inode_block_border = {0, sizeof(uint32_t) + sizeof(InodeIdx)};

/* Helper functions */
void inode_read_from_disk(Inode inode, InodeIdx idx) {
    char buff[sizeof(Inode_t)] = {0};
    disk_read(idx.block_idx, idx.block_off, buff, sizeof(Inode_t));
    memcpy(inode, buff, sizeof(Inode_t));
}

void inode_write_to_disk(Inode inode, InodeIdx idx) {
    char buff[sizeof(Inode_t)] = {0};
    memcpy(buff, inode, sizeof(Inode_t));
    disk_write(idx.block_idx, idx.block_off, buff, sizeof(Inode_t));
}

int inode_insert(Inode dir, Inode new){
    if (dir->children.block_idx == 0 && dir->children.block_off == 0){
        dir->children = new->idx;
        return 0;
    }
    Inode_t cur;
    Inode_t prev;
    InodeIdx idx = dir->children;
    while (true) {
        inode_read_from_disk(&cur, idx);
        if (strcmp(new->name, cur.name) == 0){
            return -1;
        }
        if (strcmp(new->name, cur.name) < 0) {
            if (idx.block_off == dir->children.block_off && idx.block_idx == dir->children.block_idx){
                dir->children = new->idx;
                new->next = cur.idx;

            } else {
                prev.next = new->idx;
                new->next = cur.idx;
                inode_write_to_disk(&prev, prev.idx);
            }
            break;
        }
        // end of dir children
        if (cur.next.block_off == 0 && cur.next.block_idx == 0) {
            cur.next = new->idx;
            inode_write_to_disk(&cur, cur.idx);
            break;
        }
        prev = cur;
        idx = cur.next;
    }
    return 0;
}

int inode_create(bool isDirectory, char name[MAX_FILE_NAME_SIZE], uint32_t file_idx, uint32_t file_size){
    InodeIdx prevIdx = inode_block_border;
    if (inode_block_border.block_off + sizeof(Inode_t) > BLOCK_SIZE) {
        inode_block_border.block_off = 0;
        inode_block_border.block_idx++;
        if (inode_block_border.block_idx > INODE_BLOCKS) {
            return -2; // out of inode blocks
        }
    }

    Inode_t new_inode = {
            {0, 0},
            {0, 0},
            current_dir->idx,
            inode_block_border,
            isDirectory,
            "\0",
            file_idx,
            file_size
    };
    strcpy(new_inode.name, name);
    if (inode_insert(current_dir, &new_inode) == -1){
        inode_block_border = prevIdx;
        return -1; // dir/file with that name already exists
    }
    inode_write_to_disk(&new_inode, new_inode.idx);

    inode_block_border.block_off += sizeof(Inode_t);

    return 0;
}

// TODO: use freed space
int inode_remove(Inode dir, char name[MAX_FILE_NAME_SIZE], Inode out_removed) {
    InodeIdx idx = dir->children;
    Inode_t cur;
    Inode_t prev;
    prev.idx.block_off = 0;
    prev.idx.block_idx = 0;

    while (true) {
        // end of dir children
        if (idx.block_off == 0 && idx.block_idx == 0) {
            return -1;
        }
        inode_read_from_disk(&cur, idx);
        if (!cur.isDirectory && strcmp(name, cur.name) == 0){
            if (prev.idx.block_off == 0 && prev.idx.block_idx == 0){
                dir->children = cur.next;
            } else {
                prev.next = cur.next;
                inode_write_to_disk(&prev, prev.idx);
            }
            *out_removed = cur;
            break;
        }
        idx = cur.next;
        prev = cur;
    }
    return 0;
}

void empty_block_data_read_from_disk(EmptyBlockData* data, uint32_t idx) {
    char buff[BLOCK_SIZE] = {0};
    disk_read_block(idx, buff);
    memcpy(data, buff, sizeof(EmptyBlockData));
}

void empty_block_data_write_to_disk(EmptyBlockData* data, uint32_t idx) {
    char buff[BLOCK_SIZE] = {0};
    memcpy(buff, data, sizeof(EmptyBlockData));
    disk_write_block(idx, buff);
}

void file_metadata_read_from_disk(FileMetadata* metadata, uint32_t idx) {
    char buff[sizeof(FileMetadata)] = {0};
    disk_read(idx, 0, buff, sizeof(FileMetadata));
    memcpy(&metadata->num_pointers, buff, sizeof(FileMetadata));
}

void file_metadata_write_to_disk(FileMetadata* metadata, uint32_t idx) {
    char buff[sizeof(FileMetadata)] = {0};
    memcpy(buff, &metadata->num_pointers, sizeof(FileMetadata));
    disk_write(idx, 0, buff, sizeof(FileMetadata));
}

uint32_t find_free_space(uint32_t num_blocks){
    EmptyBlockData data = empty_block_head;
    EmptyBlockData prev = {0, 0, 0};
    uint32_t space_idx = 0;
    while (true){
        if (data.size >= num_blocks){
            space_idx = data.idx;
            // split block data
            if (data.size > num_blocks) {
                data.idx += num_blocks;
                data.size -= num_blocks;
                empty_block_data_write_to_disk(&data, data.idx);

                if (prev.idx == 0) {
                    empty_block_head = data;
                } else {
                    prev.next = data.idx;
                    empty_block_data_write_to_disk(&prev, prev.idx);
                }
            } else {
                if (prev.idx == 0) {
                    if (data.next != 0) {
                        empty_block_data_read_from_disk(&data, data.next);
                        empty_block_head = data;
                    } else {
                        return 0;
                    }
                } else {
                    prev.next = data.next;
                    empty_block_data_write_to_disk(&prev, prev.idx);
                }

            }
            break;
        }
        if (data.next == 0) break;
        prev = data;
        empty_block_data_read_from_disk(&data, data.next);
    }
    return space_idx;
}

int file_create(char* content, size_t size, uint32_t* out_idx, uint32_t* out_num_blocks){
    size += sizeof(FileMetadata);
    uint32_t num_blocks = (size / (BLOCK_SIZE)) + (size % (BLOCK_SIZE) != 0);

    uint32_t free_block_idx = find_free_space(num_blocks);

    if (free_block_idx == 0) return -1;
    FileMetadata metadata = {1};
    FileMetadata metadata2;

    // writing first block
    file_metadata_write_to_disk(&metadata, free_block_idx);

    file_metadata_read_from_disk(&metadata2, free_block_idx);

    disk_write(free_block_idx, sizeof(FileMetadata), content, BLOCK_SIZE - sizeof(FileMetadata));

    for (uint32_t i = 1; i < num_blocks; i++) {
        disk_write_block(free_block_idx + i, &content[i*(BLOCK_SIZE)]);
    }

    *out_idx = free_block_idx;
    *out_num_blocks = num_blocks;
    return 0;
}

// TODO: merge???????
void file_delete(uint32_t block_idx, uint32_t size){
    EmptyBlockData new_data = {
            block_idx,
            empty_block_head.idx,
            size
    };
    empty_block_data_write_to_disk(&new_data, block_idx);
    empty_block_head = new_data;
}

int resolvePath(const char* path, size_t path_size, Inode out_inode){
    char name[MAX_FILE_NAME_SIZE];
    uint32_t path_idx = 0;
    Inode_t dir = *current_dir;
    while (true){
        uint32_t name_idx = 0;
        while (path[path_idx] != '/'){
            name[name_idx++] = path[path_idx++];
            if (name_idx >= MAX_FILE_NAME_SIZE) return -1;
            if (path_idx >= path_size) break;
        }
        path_idx++; // jump the '/'
        name[name_idx] = '\0';

        if (name[0] == '.' && name[1] == '.'){
            if (dir.parent.block_off == UINT32_MAX && dir.parent.block_idx == UINT32_MAX) continue;

            inode_read_from_disk(&dir, dir.parent);

        } else {
            InodeIdx idx = dir.children;
            while (true) {
                // end of dir children
                if (idx.block_off == 0 && idx.block_idx == 0) {
                    return -1;
                }
                Inode_t cur;
                inode_read_from_disk(&cur, idx);
                if (cur.isDirectory && strcmp(name, cur.name) == 0){
                    dir = cur;
                    break;
                }
                idx = cur.next;
            }
        }
        if (path_idx >= path_size) {
            *out_inode = dir;
            return 0;
        }
    }
}

/* OKFS functions */

void okfs_create(){
    disk_create();
    disk_start();

    // Initialize the initial inode
    Inode_t head = {
            {0, 0},
            {0, 0},
            {UINT32_MAX, UINT32_MAX},
            inode_block_border,
            true,
            "\0",
            -1,
            0
    };

    // write the super block to disk (block 0)
    char buff[BLOCK_SIZE] = {0};

    memcpy(buff, &head, sizeof(Inode_t));
    disk_write(inode_block_border.block_idx, inode_block_border.block_off, buff, sizeof(Inode_t));

    EmptyBlockData all_memory = {
            INODE_BLOCKS + 1,
            0,
            ((NUM_BLOCKS)*(BLOCK_SIZE)) - INODE_BLOCKS
    };
    memcpy(buff, &all_memory, sizeof(EmptyBlockData));
    disk_write_block(all_memory.idx, buff);

    memcpy(buff, &all_memory.idx, sizeof(uint32_t));
    disk_write(0, 0, buff, sizeof(uint32_t));

    memcpy(buff, &inode_block_border, sizeof(InodeIdx));
    disk_write(0, sizeof(uint32_t), buff, sizeof(InodeIdx));

    disk_close();
}

void okfs_mount(){
    disk_start();

    global_head = (Inode) malloc(sizeof(Inode_t));
    current_dir = global_head;
    cur_depth = 0;

    inode_read_from_disk(global_head, inode_block_border);

    uint32_t free_block_idx;
    char buff[BLOCK_SIZE] = {0};
    disk_read(0, 0, buff, sizeof(uint32_t));
    memcpy(&free_block_idx, buff, sizeof(uint32_t));

    disk_read(0, sizeof(uint32_t), buff, sizeof(InodeIdx));
    memcpy(&inode_block_border, buff, sizeof(InodeIdx));

    disk_read_block(free_block_idx, buff);
    memcpy(&empty_block_head, buff, sizeof(EmptyBlockData));

    inode_block_border.block_off += sizeof(Inode_t);

}

void okfs_unmount(){
    // save emptyblock head pos
    char buff[sizeof(InodeIdx)] = {0};
    char buff2[sizeof(InodeIdx)] = {0};
    memcpy(buff, &empty_block_head.idx, sizeof(uint32_t));
    disk_write(0, 0, buff, sizeof(uint32_t));

    // save inode border
    memcpy(buff, &inode_block_border, sizeof(InodeIdx));
    disk_write(0, sizeof(uint32_t), buff, sizeof(InodeIdx));

    inode_write_to_disk(current_dir, current_dir->idx);
    if (current_dir != global_head)
        free(current_dir);
    free(global_head);
    disk_close();
}

void okfs_print_cur_dir_path(){
    if (current_dir == global_head) {
        printf("/ -> ");
        return;
    }
    char dir_names[cur_depth][MAX_FILE_NAME_SIZE];
    Inode_t cur = *current_dir;
    strcpy(dir_names[cur_depth - 1], cur.name);

    InodeIdx idx = current_dir->parent;
    for (int i = cur_depth - 2; i >= 0; i--) {
        inode_read_from_disk(&cur, idx);
        strcpy(dir_names[i], cur.name);
        idx = cur.parent;
    }

    printf("/");
    for (int i = 0; i < cur_depth; i++){
        printf("%s/", dir_names[i]);
    }
    printf(" -> ");
}

int okfs_mkdir(char name[MAX_FILE_NAME_SIZE]){
    return inode_create(true, name, UINT32_MAX, 0);
}

void okfs_ls(){
    InodeIdx idx = current_dir->children;
    while (true) {
        // end of dir children
        if (idx.block_off == 0 && idx.block_idx == 0) {
            return;
        }
        Inode_t cur;
        inode_read_from_disk(&cur, idx);
        printf("%s ", cur.name);
        // end of dir children
        if (cur.next.block_off == 0 && cur.next.block_idx == 0) {
            printf("\n");
            break;
        }
        idx = cur.next;
    }
}

int okfs_cd(char name[MAX_FILE_NAME_SIZE]){
    if (name[0] == '.' && name[1] == '.'){
        if (current_dir == global_head) return 0;
        else if (current_dir->parent.block_off == 0 && current_dir->parent.block_idx == 0) {
            cur_depth--;
            current_dir = global_head;
            return 0;
        }

        Inode_t cur;
        inode_read_from_disk(&cur, current_dir->parent);
        inode_write_to_disk(current_dir, current_dir->idx);
        free(current_dir);

        current_dir = (Inode) malloc(sizeof(Inode_t));
        *current_dir = cur;

        cur_depth--;
        return 0;
    }

    InodeIdx idx = current_dir->children;
    while (true) {
        // end of dir children
        if (idx.block_off == 0 && idx.block_idx == 0) {
            return -1;
        }
        Inode_t cur;
        inode_read_from_disk(&cur, idx);
        if (cur.isDirectory && strcmp(name, cur.name) == 0){
            inode_write_to_disk(current_dir, current_dir->idx);
            if (current_dir != global_head) free(current_dir);
            current_dir = (Inode) malloc(sizeof(Inode_t));
            *current_dir = cur;
            break;
        }
        idx = cur.next;
    }
    cur_depth++;
    return 0;
}

int okfs_mkfile(char name[MAX_FILE_NAME_SIZE], char* content, size_t size){
    uint32_t block_idx, num_blocks;
    int res = file_create(content, size, &block_idx, &num_blocks);
    if (res == -1) return -3;
    return inode_create(false, name, block_idx, num_blocks);
}

int okfs_cat(char name[MAX_FILE_NAME_SIZE]) {
    InodeIdx idx = current_dir->children;
    while (true) {
        // end of dir children
        if (idx.block_off == 0 && idx.block_idx == 0) {
            return -1;
        }
        Inode_t cur;
        inode_read_from_disk(&cur, idx);
        if (!cur.isDirectory && strcmp(name, cur.name) == 0){
            char buff[BLOCK_SIZE + 1];
            buff[BLOCK_SIZE] = '\0';
            disk_read_block(cur.file_idx, buff);
            printf("%s", buff + sizeof(FileMetadata));
            for (uint32_t i = 1; i < cur.size; i++){
                disk_read_block(cur.file_idx + i, buff);
                printf("%s", buff);
            }
            printf("\n");
            break;
        }
        idx = cur.next;
    }
    return 0;
}

int okfs_delfile(char name[MAX_FILE_NAME_SIZE]){
    Inode_t inode;
    int res = inode_remove(current_dir, name, &inode);
    if (res == -1) return -1;
    FileMetadata metadata;
    file_metadata_read_from_disk(&metadata, inode.file_idx);
    if (--metadata.num_pointers == 0) file_delete(inode.file_idx, inode.size);
    else file_metadata_write_to_disk(&metadata, inode.file_idx);
    return 0;
}

int okfs_cgfile(char name[MAX_FILE_NAME_SIZE], char* content, size_t size){
    InodeIdx idx = current_dir->children;
    Inode_t cur;
    while (true) {
        // end of dir children
        if (idx.block_off == 0 && idx.block_idx == 0) {
            return -1;
        }
        inode_read_from_disk(&cur, idx);
        if (!cur.isDirectory && strcmp(name, cur.name) == 0){
            break;
        }
        idx = cur.next;
    }

    uint32_t block_idx, num_blocks;
    int res = file_create(content, size, &block_idx, &num_blocks);
    if (res == -1) return -3;

    uint32_t prev_idx = cur.file_idx;
    uint32_t prev_size = cur.size;
    cur.file_idx = block_idx;
    cur.size = num_blocks;

    inode_write_to_disk(&cur, cur.idx);

    FileMetadata metadata;
    file_metadata_read_from_disk(&metadata, prev_idx);

    if (--metadata.num_pointers == 0) file_delete(prev_idx, prev_size);
    else file_metadata_write_to_disk(&metadata, prev_idx);

    return 0;


}

int okfs_mvfile(char name[MAX_FILE_NAME_SIZE], char* path, size_t path_size){
    Inode_t inode;
    int res = inode_remove(current_dir, name, &inode);
    if (res == -1) return -1;

    Inode_t new_dir;
    res = resolvePath(path, path_size, &new_dir);
    if (res == -1) {
        inode_insert(current_dir, &inode);
        inode_write_to_disk(&inode, inode.idx);
        return -2;
    }

    inode.parent = new_dir.idx;
    inode.next.block_off = 0;
    inode.next.block_idx = 0;
    res = inode_insert(&new_dir, &inode);
    if (res == -1) {
        inode.parent = current_dir->idx;
        inode.next.block_off = 0;
        inode.next.block_idx = 0;
        inode_insert(current_dir, &inode);
        inode_write_to_disk(&inode, inode.idx);
        return -3;
    }

    inode_write_to_disk(&inode, inode.idx);
    inode_write_to_disk(&new_dir, new_dir.idx);
    return 0;
}

int okfs_cpfile(char name[MAX_FILE_NAME_SIZE], char* new_name, size_t new_name_size){
    if (new_name_size > MAX_FILE_NAME_SIZE) return -3;
    new_name[MAX_FILE_NAME_SIZE - 1] = '\0';

    InodeIdx idx = current_dir->children;
    Inode_t cur;
    while (true) {
        // end of dir children
        if (idx.block_off == 0 && idx.block_idx == 0) {
            return -1;
        }
        inode_read_from_disk(&cur, idx);
        if (!cur.isDirectory && strcmp(name, cur.name) == 0){
            break;
        }
        idx = cur.next;
    }

    FileMetadata metadata;
    file_metadata_read_from_disk(&metadata, cur.file_idx);
    metadata.num_pointers++;
    file_metadata_write_to_disk(&metadata, cur.file_idx);

    return inode_create(false, new_name, cur.file_idx, cur.size);
}

int okfs_inode_info(char name[MAX_FILE_NAME_SIZE]){
    InodeIdx idx = current_dir->children;
    while (true) {
        // end of dir children
        if (idx.block_off == 0 && idx.block_idx == 0) {
            return -1;
        }
        Inode_t cur;
        inode_read_from_disk(&cur, idx);
        if (strcmp(name, cur.name) == 0){
            printf("*---------------------------*\n");
            printf("Inode info:\n");
            printf("name: %s\n", cur.name);
            printf( "idx: idx = %d, off = %d\n", cur.idx.block_idx, cur.idx.block_off);
            printf( "parent: idx = %d, off = %d\n", cur.parent.block_idx, cur.parent.block_off);
            printf( "children: idx = %d, off = %d\n", cur.children.block_idx, cur.children.block_off);
            printf( "next: idx = %d, off = %d\n", cur.next.block_idx, cur.next.block_off);
            printf("is directory: %d\n", cur.isDirectory);
            printf("file idx: %d\n", cur.file_idx);
            printf("file size: %d\n", cur.size);
            printf("*---------------------------*\n");
            break;
        }
        idx = cur.next;
    }
    return 0;
}

