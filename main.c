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
    disk_create();
    disk_start();
    char cmd[50];
    char name[50];
    char content[BLOCK_SIZE];
    size_t totalSize;
    char *line = NULL;
    while (true) {
        printf("dir -> ");
        size_t lineSize = getline(&line, &totalSize, stdin);

        parseLine(line, lineSize, cmd, name, content);
        printf("cmd: %s\nname: %s\ncontent: %s\n", cmd, name, content);

        if (strcmp("exit", cmd) == 0){
            break;
        } else if (strcmp("ls", cmd) == 0){

        } else if (strcmp("mkfile", cmd) == 0){

        }
        free(line);
        line = NULL;
    }
    disk_close();
    return 0;
}
