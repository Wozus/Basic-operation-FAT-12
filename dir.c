#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include "file_reader.h"

struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path)
{
    if(pvolume == NULL || dir_path == NULL){
        errno = EFAULT;
        return NULL;
    }

    struct dir_t * dir = (struct dir_t*) calloc(1, sizeof(struct dir_t));
    if(dir == NULL){
        errno = ENOMEM;
        return NULL;
    }
    if(strcmp(dir_path,"\\")==0){
        uint16_t first_root_dir_sectors = pvolume->first_data_sector - pvolume->root_dir_sectors;


        dir->catalog = calloc(pvolume->super->root_dir_capacity*pvolume->super->bytes_per_sector, sizeof(char));
        if(dir->catalog == NULL){
            errno = ENOMEM;
            free(dir);
            return NULL;
        }
        disk_read(pvolume->disk,first_root_dir_sectors,(void*)dir->catalog,pvolume->super->root_dir_capacity);
        dir->back=pvolume;
    }
    else{
        //zadanie na 4
        free(dir);
        return NULL;
    }

    return dir;
}
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry)
{
    if(pdir == NULL || pentry==NULL){
        errno = EFAULT;
        return -1;
    }
    if(pdir->count*32 > pdir->back->super->root_dir_capacity * pdir->back->super->bytes_per_sector)
    {
        errno=ENXIO;
        return -1;
    }


    if(pdir->finish == 1 )
        return 1;
    struct dir_super_t *temp;
    do {
        temp = (struct dir_super_t *) ((char *) pdir->catalog + pdir->count * 32);
        pdir->count++;
    }while(*(uint8_t*)temp->name == 0xe5);

    if(*(uint8_t*)temp->name == 0x00)
    {
        pdir->finish=1;
        return 1;
    }

    int i=0;
    memset(pentry->name,0,12);
    for(int c=0;i<13;i++){
        if(temp->name[i] != ' ' && i<8) {
            pentry->name[i] = temp->name[i];
            c++;
        }
        else if(temp->ext[i-c] != ' ' && i-c <3) {
            if (i == c)
                pentry->name[i] = '.';
            pentry->name[i + 1] = temp->ext[i - c];
        }
        else break;
    }
    pentry->name[12] = '\0';

    pentry->size = temp->size;
    pentry->is_system = temp->system;
    pentry->is_readonly = temp->read_only;
    pentry->is_hidden = temp->hidden;
    pentry->is_directory = temp->directory;
    pentry->is_archived = temp->archive;
    pentry->first_cluster_low_bits = temp->first_cluster_low_bits;
    return 0;
}
int dir_close(struct dir_t* pdir)
{
    if(pdir==NULL){
        errno=EFAULT;
        return -1;
    }
    free(pdir->catalog);
    free(pdir);
    return 0;
}

