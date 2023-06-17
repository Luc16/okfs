#include "okfs.h"

void inode_insert(Inode tree, Inode new){
    for (Inode cur = tree->children; cur != NULL; cur = cur->next){
        if (strcmp(new->name, cur->name) > 0) {
            Inode temp = cur->next;
            cur->next = new;
            new->next = temp;
            break;
        }
        if (cur->next == NULL) {
            cur->next = new;
            break;
        }
    }
}

