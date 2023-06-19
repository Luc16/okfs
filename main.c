#include "okfs.h"
#include "disk.h"

void parseLine(const char* line, size_t lineSize, char* cmd, char* name, char* content){
    uint count = 0;
    uint idx = 0;
    for (uint i = 0; i < lineSize; i++){
        switch (count) {
            case 0:
                if (line[i] == ' ' || line[i] == '\n') {
                    count = 1;
                    cmd[idx] = '\0';
                    idx = 0;
                    break;
                }
                cmd[idx++] = line[i];
                break;
            case 1:
                if (line[i] == ' ' || line[i] == '\n') {
                    count = 2;
                    name[idx] = '\0';
                    idx = 0;
                    break;
                }
                name[idx++] = line[i];
                break;
            case 2:
                if (idx < BLOCK_SIZE) content[idx++] = line[i];
                break;
            default:
                break;
        }
    }
    if (count == 2) content[idx] = '\0';
}

int main() {
    char cmd[50] = {0};
    char name[50] = {0};
    char content[4*BLOCK_SIZE] = {0};
    size_t totalSize;
    char *line = NULL;

    printf("Do you want to create a new file system? (y/n)\n");
    size_t lineSize = getline(&line, &totalSize, stdin);
    if (line[0] == 'y' || line[0] == 'Y') okfs_create();

    okfs_mount();

    while (true) {
        okfs_print_cur_dir_path();
        lineSize = getline(&line, &totalSize, stdin);

        parseLine(line, lineSize, cmd, name, content);
//        printf("cmd: %s\nname: %s\ncontent: %s\n", cmd, name, content);

        if (strcmp("exit", cmd) == 0){
            break;
        } else if (strcmp("ls", cmd) == 0){
            okfs_ls();
        } else if (strcmp("mkdir", cmd) == 0){
            int res = okfs_mkdir(name);
            if (res == -1) printf("File/Dir with name %s already exists in folder\n", name);
            if (res == -2) printf("Inode space is full, no more files or directories can be added\n");
        } else if (strcmp("cd", cmd) == 0){
            int res = okfs_cd(name);
            if (res == -1) printf("Directory %s not found\n", name);
        } else if (strcmp("mkfile", cmd) == 0){
            size_t size = strlen(content);
            content[strlen(content) - 1] = '\0';
            int res = okfs_mkfile(name, content, size);
            if (res == -1) printf("File/Dir with name %s already exists in folder\n", name);
            if (res == -2) printf("Inode space is full, no more files or directories can be added\n");
            if (res == -3) printf("Memory is full, no more files or directories can be added\n");
        } else if (strcmp("cat", cmd) == 0){
            int res = okfs_cat(name);
            if (res == -1) printf("File %s not found\n", name);
        } else if (strcmp("delfile", cmd) == 0){
            int res = okfs_delfile(name);
            if (res == -1) printf("File %s not found\n", name);
        } else if (strcmp("cgfile", cmd) == 0){
            size_t size = strlen(content);
            content[strlen(content) - 1] = '\0';
            int res = okfs_cgfile(name, content, size);
            if (res == -1) printf("File/Dir with name %s not found in folder\n", name);
            if (res == -3) printf("Memory is full, no more files or directories can be added\n");
        } else if (strcmp("mvfile", cmd) == 0){
            size_t size = strlen(content);
            content[strlen(content) - 1] = '\0';
            int res = okfs_mvfile(name, content, size - 1);
            if (res == -1) printf("File/Dir with name %s not found in folder\n", name);
            if (res == -2) printf("Could not resolve path %s\n", content);
            if (res == -3) printf("File/Dir with name %s already exists in folder destination\n", name);
        } else {
            printf("Invalid command: %s\n", cmd);
        }
        free(line);
        line = NULL;
        memset(content, 0, 4*BLOCK_SIZE);
    }
    okfs_unmount();
    return 0;
}
