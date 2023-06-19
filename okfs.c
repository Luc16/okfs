#include "okfs.h"

Inode global_head = NULL;
Inode current_dir = NULL;
int cur_depth;
EmptyBlockData empty_block_head;
InodeIdx inode_block_border = {0, sizeof(uint32_t) + sizeof(InodeIdx)};

void read_inode_from_disk(Inode inode, InodeIdx idx) {
    char buff[sizeof(Inode_t)] = {0};
    disk_read(idx.block_idx, idx.block_off, buff, sizeof(Inode_t));
    memcpy(inode, buff, sizeof(Inode_t));
}

void write_inode_to_disk(Inode inode, InodeIdx idx) {
    char buff[sizeof(Inode_t)] = {0};
    memcpy(buff, inode, sizeof(Inode_t));
    disk_write(idx.block_idx, idx.block_off, buff, sizeof(Inode_t));
}

void read_empty_block_data_from_disk(EmptyBlockData* data, uint32_t idx) {
    char buff[BLOCK_SIZE] = {0};
    disk_read_block(idx, buff);
    memcpy(data, buff, sizeof(EmptyBlockData));
}

void write_empty_block_data_to_disk(EmptyBlockData* data, uint32_t idx) {
    char buff[BLOCK_SIZE] = {0};
    memcpy(buff, data, sizeof(EmptyBlockData));
    disk_write_block(idx, buff);
}

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

    read_inode_from_disk(global_head, inode_block_border);

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
    memcpy(buff, &empty_block_head.idx, sizeof(uint32_t));
    disk_write(0, 0, buff, sizeof(uint32_t));

    // save inode border
    memcpy(buff, &inode_block_border, sizeof(InodeIdx));
    disk_write(0, sizeof(uint32_t), buff, sizeof(InodeIdx));

    write_inode_to_disk(current_dir, current_dir->idx);
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
        read_inode_from_disk(&cur, idx);
        strcpy(dir_names[i], cur.name);
        idx = cur.parent;
    }

    printf("/");
    for (int i = 0; i < cur_depth; i++){
        printf("%s/", dir_names[i]);
    }
    printf(" -> ");
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
        read_inode_from_disk(&cur, idx);
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
                write_inode_to_disk(&prev, prev.idx);
            }
            break;
        }
        // end of dir children
        if (cur.next.block_off == 0 && cur.next.block_idx == 0) {
            cur.next = new->idx;
            write_inode_to_disk(&cur, cur.idx);
            break;
        }
        prev = cur;
        idx = cur.next;
    }
    return 0;
}

int okfs_mkdir(char name[MAX_FILE_NAME_SIZE]){
    InodeIdx prevIdx = inode_block_border;
    if (inode_block_border.block_off + sizeof(Inode_t) > BLOCK_SIZE) {
        inode_block_border.block_off = 0;
        inode_block_border.block_idx++;
        if (inode_block_border.block_idx > INODE_BLOCKS) {
            return -2; // out of inode blocks
        }
    }
    Inode_t new_dir = {
            {0, 0},
            {0, 0},
            current_dir->idx,
            inode_block_border,
            true,
            "\0",
            -1,
            0
    };

    strcpy(new_dir.name, name);

    if (inode_insert(current_dir, &new_dir) == -1){
        inode_block_border = prevIdx;
        return -1; // dir/file with that name already exists
    }

    write_inode_to_disk(&new_dir, inode_block_border);

    inode_block_border.block_off += sizeof(Inode_t);

    return 0;
}

void okfs_ls(){
    if (current_dir->children.block_idx == 0 && current_dir->children.block_off == 0){
        return;
    }
    InodeIdx idx = current_dir->children;
    while (true) {
        Inode_t cur;
        read_inode_from_disk(&cur, idx);
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
        read_inode_from_disk(&cur, current_dir->parent);
        write_inode_to_disk(current_dir, current_dir->idx);
        free(current_dir);

        current_dir = (Inode) malloc(sizeof(Inode_t));
        *current_dir = cur;

        cur_depth--;
        return 0;
    }
    if (current_dir->children.block_idx == 0 && current_dir->children.block_off == 0){
        return -1;
    }
    InodeIdx idx = current_dir->children;
    while (true) {
        Inode_t cur;
        read_inode_from_disk(&cur, idx);
        if (cur.isDirectory && strcmp(name, cur.name) == 0){
            write_inode_to_disk(current_dir, current_dir->idx);
            if (current_dir != global_head) free(current_dir);
            current_dir = (Inode) malloc(sizeof(Inode_t));
            *current_dir = cur;
            break;
        }
        // end of dir children
        if (cur.next.block_off == 0 && cur.next.block_idx == 0) {
            return -1;
        }
        idx = cur.next;
    }
    cur_depth++;
    return 0;
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
                write_empty_block_data_to_disk(&data, data.idx);

                if (prev.idx == 0) {
                    empty_block_head = data;
                } else {
                    prev.next = data.idx;
                    write_empty_block_data_to_disk(&prev, prev.idx);
                }
            } else {
                if (prev.idx == 0) {
                    if (data.next != 0) {
                        read_empty_block_data_from_disk(&data, data.next);
                        empty_block_head = data;
                    } else {
                        return 0;
                    }
                } else {
                    prev.next = data.next;
                    write_empty_block_data_to_disk(&prev, prev.idx);
                }

            }
            break;
        }
        if (data.next == 0) break;
        prev = data;
        read_empty_block_data_from_disk(&data, data.next);
    }
    return space_idx;
}

int okfs_mkfile(char name[MAX_FILE_NAME_SIZE], char* content, size_t size){
    InodeIdx prevIdx = inode_block_border;
    if (inode_block_border.block_off + sizeof(Inode_t) > BLOCK_SIZE) {
        inode_block_border.block_off = 0;
        inode_block_border.block_idx++;
        if (inode_block_border.block_idx > INODE_BLOCKS) {
            return -2; // out of inode blocks
        }
    }

    uint32_t num_blocks = (size / (NUM_BLOCKS)) + (size % (NUM_BLOCKS) != 0);

    uint32_t free_block_idx = find_free_space(num_blocks);

    if (free_block_idx == 0) return -3;

    for (uint32_t i = 0; i < num_blocks; i++) {
        disk_write_block(free_block_idx + i, &content[i*(BLOCK_SIZE)]);
    }

    Inode_t new_file = {
            {0, 0},
            {0, 0},
            current_dir->idx,
            inode_block_border,
            false,
            "\0",
            free_block_idx,
            num_blocks
    };

    strcpy(new_file.name, name);

    if (inode_insert(current_dir, &new_file) == -1){
        inode_block_border = prevIdx;
        return -1; // dir/file with that name already exists
    }

    write_inode_to_disk(&new_file, inode_block_border);

    inode_block_border.block_off += sizeof(Inode_t);

    return 0;
}

int okfs_cat(char name[MAX_FILE_NAME_SIZE]) {
    if (current_dir->children.block_idx == 0 && current_dir->children.block_off == 0){
        return -1;
    }
    InodeIdx idx = current_dir->children;
    while (true) {
        Inode_t cur;
        read_inode_from_disk(&cur, idx);
        if (!cur.isDirectory && strcmp(name, cur.name) == 0){
            char buff[BLOCK_SIZE];
            for (uint32_t i = 0; i < cur.size; i++){
                disk_read_block(cur.file_idx + i, buff);
                printf("%s", buff);
            }
            printf("\n");
            break;
        }
        // end of dir children
        if (cur.next.block_off == 0 && cur.next.block_idx == 0) {
            return -1;
        }
        idx = cur.next;
    }
    return 0;
}

// TODO merge???????
void delete_file(uint32_t block_idx, uint32_t size){
    EmptyBlockData new_data = {
            block_idx,
            empty_block_head.idx,
            size
    };
    write_empty_block_data_to_disk(&new_data, block_idx);
    empty_block_head = new_data;
}

// TODO: use freed space
int inode_remove(Inode dir, char name[MAX_FILE_NAME_SIZE], Inode out_removed) {
    InodeIdx idx = dir->children;
    Inode_t cur;
    Inode_t prev;
    prev.idx.block_off = 0;
    prev.idx.block_idx = 0;

    while (true) {
        read_inode_from_disk(&cur, idx);
        if (!cur.isDirectory && strcmp(name, cur.name) == 0){
            if (prev.idx.block_off == 0 && prev.idx.block_idx == 0){
                dir->children = cur.next;
            } else {
                prev.next = cur.next;
            }
            *out_removed = cur;
            break;
        }
        // end of dir children
        if (cur.next.block_off == 0 && cur.next.block_idx == 0) {
            return -1;
        }
        idx = cur.next;
        prev = cur;
    }
    return 0;
}

int okfs_delfile(char name[MAX_FILE_NAME_SIZE]){
    Inode_t inode;
    int res = inode_remove(current_dir, name, &inode);
    if (res == -1) return -1;
    delete_file(inode.file_idx, inode.size);
    return 0;
}

int okfs_cgfile(char name[MAX_FILE_NAME_SIZE], char* content, size_t size){
    if (current_dir->children.block_idx == 0 && current_dir->children.block_off == 0){
        return -1;
    }
    InodeIdx idx = current_dir->children;
    Inode_t cur;
    while (true) {
        read_inode_from_disk(&cur, idx);
        if (!cur.isDirectory && strcmp(name, cur.name) == 0){
            break;
        }
        // end of dir children
        if (cur.next.block_off == 0 && cur.next.block_idx == 0) {
            return -1;
        }
        idx = cur.next;
    }

    uint32_t num_blocks = (size / (NUM_BLOCKS)) + (size % (NUM_BLOCKS) != 0);
    uint32_t free_block_idx = find_free_space(num_blocks);
    if (free_block_idx == 0) return -3;

    for (uint32_t i = 0; i < num_blocks; i++) {
        disk_write_block(free_block_idx + i, &content[i*(BLOCK_SIZE)]);
    }

    uint32_t prev_idx = cur.file_idx;
    uint32_t prev_size = cur.size;
    cur.file_idx = free_block_idx;
    cur.size = num_blocks;

    write_inode_to_disk(&cur, cur.idx);

    delete_file(prev_idx, prev_size);

    return 0;


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

            read_inode_from_disk(&dir, dir.parent);

        } else {

            InodeIdx idx = dir.children;
            if (idx.block_idx == 0 && idx.block_off == 0){
                return -1;
            }

            while (true) {
                Inode_t cur;
                read_inode_from_disk(&cur, idx);
                if (cur.isDirectory && strcmp(name, cur.name) == 0){
                    dir = cur;
                    break;
                }
                // end of dir children
                if (cur.next.block_off == 0 && cur.next.block_idx == 0) {
                    return -1;
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

int okfs_mvfile(char name[MAX_FILE_NAME_SIZE], char* path, size_t path_size){
    Inode_t inode;
    int res = inode_remove(current_dir, name, &inode);
    if (res == -1) return -1;

    Inode_t new_dir;
    res = resolvePath(path, path_size, &new_dir);
    if (res == -1) {
        inode_insert(current_dir, &inode);
        return -2;
    }

    res = inode_insert(&new_dir, &inode);
    if (res == -1) {
        inode_insert(current_dir, &inode);
        return -3;
    }
    inode.parent = new_dir.idx;
    inode.next.block_off = 0;
    inode.next.block_idx = 0;
    write_inode_to_disk(&inode, inode.idx);
    write_inode_to_disk(&new_dir, new_dir.idx);
    return 0;
}


