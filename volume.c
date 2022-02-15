#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include "file_reader.h"
#include "tested_declarations.h"
#include "rdebug.h"

struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector)
{
    if(pdisk ==NULL){
        errno=EFAULT;
        return NULL;
    }
    struct volume_t * open = (struct volume_t*) calloc(1, sizeof(struct volume_t));
    if(open==NULL){
        errno=ENOMEM;
        return NULL;
    }
    open->super = (struct f_super_t*) calloc(1, sizeof(struct f_super_t));
    if(open->super==NULL){
        errno=ENOMEM;
        free(open);
        return NULL;
    }
    open->disk = pdisk;
    if(disk_read(open->disk,(int32_t)first_sector,(void*)open->super,1)!=1){
        errno=EINVAL;
        free(open->super);
        free(open);
        return NULL;
    }
    if(open->super->bytes_per_sector==0){
        errno=EINVAL;
        free(open->super);
        free(open);
        return NULL;
    }


    open->total_sectors = (open->super->logical_sectors16 == 0) ? open->super->logical_sectors32 : open->super->logical_sectors16;
    open->fat_size = open->super->sectors_per_fat;
    open->root_dir_sectors = ((open->super->root_dir_capacity*32) + (open->super->bytes_per_sector-1)) / open->super->bytes_per_sector;
    open->first_data_sector = open->super->reserved_sectors + (open->super->fat_count * open->fat_size) + open->root_dir_sectors;
    open->first_fat_sector = open->super->reserved_sectors;
    open->data_sectors = open->total_sectors - (open->super->reserved_sectors + (open->super->fat_count * open->fat_size) + open->root_dir_sectors);
    open->total_clusters = open->data_sectors/ open->super->sectors_per_cluster;

    struct volume_t *pvolume = open;

    uint8_t *fat1 = calloc(pvolume->super->sectors_per_fat*pvolume->super->bytes_per_sector, sizeof(uint8_t));

    disk_read(pvolume->disk,pvolume->super->reserved_sectors,fat1,pvolume->super->sectors_per_fat);

    uint8_t *fat2 = calloc(pvolume->super->sectors_per_fat*pvolume->super->bytes_per_sector, sizeof(uint8_t));

    disk_read(pvolume->disk,pvolume->super->reserved_sectors+pvolume->super->sectors_per_fat,fat2,pvolume->super->sectors_per_fat);

    if(memcmp(fat1,fat2,pvolume->super->sectors_per_fat*pvolume->super->bytes_per_sector)!=0){
        errno=EINVAL;
        free(open->super);
        free(open);
        free(fat1);
        free(fat2);
        return NULL;
    }

    free(fat1);
    free(fat2);
    return open;
}
int fat_close(struct volume_t* pvolume)
{
    if(pvolume == NULL){
        errno=EFAULT;
        return -1;
    }
    free(pvolume->super);
    free(pvolume);
    return 0;
}



