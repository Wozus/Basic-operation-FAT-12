#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include "file_reader.h"
#include "tested_declarations.h"
#include "rdebug.h"

#define sector_size 512

struct disk_t* disk_open_from_file(const char* volume_file_name)
{
    if(volume_file_name == NULL){
        errno = EFAULT;
        return NULL;
    }
    FILE * tmp = fopen(volume_file_name,"rb");
    if(tmp ==NULL){
        errno = ENONET;
        return NULL;
    }
    struct disk_t * disk = (struct disk_t*) calloc(1, sizeof(struct disk_t));
    if(disk == NULL){
        errno = ENOMEM;
        fclose(tmp);
        return NULL;

    }
    disk->f_disk = tmp;

    return disk;
}
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read)
{
    if(pdisk==NULL || buffer==NULL){
        errno=EFAULT;
        return -1;
    }
    if(fseek(pdisk->f_disk,first_sector*sector_size,SEEK_SET)!=0) {
        errno = ERANGE;
        return -1;
    }
    for(int i=0;i<sectors_to_read;i++)
    {
        if(fread((char*)buffer+(i*sector_size),sector_size,1,pdisk->f_disk)!=1){
            errno = ERANGE;
            return -1;
        }
    }

    return sectors_to_read;
}
int disk_close(struct disk_t* pdisk)
{
    if(pdisk==NULL){
        errno=EFAULT;
        return -1;
    }
    if(pdisk->f_disk != NULL)
        fclose(pdisk->f_disk);
    free(pdisk);
    return 0;
}


