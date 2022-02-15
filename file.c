#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include "file_reader.h"
#include "tested_declarations.h"
#include "rdebug.h"

uint16_t get_val (const void* buff,int last,int prev_val)
{
    uint16_t result=0;
    if(prev_val % 2 == 0){
        uint16_t first = *((uint8_t*)buff+last);
        uint16_t second = *((uint8_t*)buff+last+1);


        result |= (second<<8) & 0x0f00;
        result |= first & 0x00ff;
    }
    else{
        uint16_t first = *((uint8_t*)buff+last);
        uint16_t second = *((uint8_t*)buff+last+1);

        result |= (second<<4) & 0x0ff0;
        result |= (first>>4) & 0x000f;
    }
    return result;
}

struct clusters_chain_t *get_chain_fat12(const void * const buffer, size_t size, uint16_t first_cluster)
{
    if(buffer == NULL || size <=0)return NULL;
    if(first_cluster >= 0xFF8)return NULL;

    struct clusters_chain_t * str_clusters = (struct clusters_chain_t*) calloc(1,sizeof(struct clusters_chain_t));
    if(!str_clusters)return NULL;

    str_clusters->clusters = (uint16_t*) calloc(1,sizeof(uint16_t));
    if(str_clusters->clusters==NULL)
    {
        free(str_clusters);
        return NULL;
    }

    str_clusters->clusters[0] = first_cluster;
    int prev_val = first_cluster;
    int last = first_cluster*3/2;

    str_clusters->size=1;
    uint16_t  result = get_val(buffer,last,prev_val);


    if(result < 0xFF8){
        for(int i=1;;i++) {

            result = get_val(buffer,last,prev_val);
            if(result >= 0xFF8)break;

            str_clusters->clusters = (uint16_t *) realloc(str_clusters->clusters,sizeof(uint16_t) * (str_clusters->size + 1));
            if (str_clusters->clusters == NULL) {
                free(str_clusters);
                return NULL;
            }
            str_clusters->clusters[i] = result;
            str_clusters->size++;
            prev_val = result;
            last = result*3/2;
        }
    }

    return str_clusters;
}

struct file_t* file_open(struct volume_t* pvolume, const char* file_name)
{
   if(pvolume==NULL || file_name == NULL){
       errno = EFAULT;
       return NULL;
   }

   struct file_t* file = (struct file_t*) calloc(1, sizeof(struct file_t));
   if(file == NULL){
       errno = ENOMEM;
       return NULL;
   }
   file->back = pvolume;
   file->cluster = calloc(file->back->super->sectors_per_cluster*file->back->super->bytes_per_sector, sizeof(char));

   struct dir_t *temp = dir_open(pvolume,"\\");
   struct dir_entry_t entry;
   do{

       int er = dir_read(temp,&entry);
       if(er == 1)
       {
           free(file->cluster);
           free(file);
           dir_close(temp);
           return NULL;
       }
       if(er == -1){
           free(file->cluster);
           free(file);
           dir_close(temp);
           return NULL;
       }
   }while(strcmp(entry.name,file_name)!=0);

   if(entry.is_directory==1){
       free(file->cluster);
       free(file);
       dir_close(temp);
       return NULL;
   }

    file->size = entry.size;
    uint8_t *fat = calloc(file->back->super->sectors_per_fat*file->back->super->bytes_per_sector, sizeof(uint8_t));


    disk_read(pvolume->disk,pvolume->super->reserved_sectors,fat,pvolume->super->sectors_per_fat);
    file->chain = get_chain_fat12(fat,pvolume->super->sectors_per_cluster*512,entry.first_cluster_low_bits);
    file->actual_cluster=0;

    disk_read(pvolume->disk,file->back->first_data_sector+(file->chain->clusters[0] - 2)*pvolume->super->sectors_per_cluster,file->cluster,pvolume->super->sectors_per_cluster);

    free(fat);
    dir_close(temp);
    return file;
}
int file_close(struct file_t* stream)
{
    if(stream==NULL){
        errno=EFAULT;
        return -1;
    }
    free(stream->chain->clusters);
    free(stream->chain);
    free(stream->cluster);
    free(stream);
    return 0;
}
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream)
{
    if(ptr == NULL || stream == NULL){
        errno=EFAULT;
        return -1;
    }
    if(size==0 || nmemb==0)
        return 0;
    unsigned i=0;
    for(;i<(size*nmemb);i++,stream->count++)
    {
        if((uint32_t)stream->count >= stream->back->super->sectors_per_cluster*stream->back->super->bytes_per_sector) {
            if (file_seek(stream, 0, SEEK_CUR) == -1) {
                errno = ERANGE;
                return -1;
            }
        }

        if((uint32_t)(stream->count + (stream->actual_cluster) * stream->back->super->bytes_per_sector*stream->back->super->sectors_per_cluster) == stream->size){
            break;
        }
        *((char*)ptr+i) = *(stream->cluster+stream->count);
    }
    return i/size;
}
int32_t file_seek(struct file_t* stream, int32_t offset, int whence){

    if(stream == NULL){
        errno=EFAULT;
        return -1;
    }
    int new_offset = offset%(stream->back->super->sectors_per_cluster*stream->back->super->bytes_per_sector);
    int cluster_index = offset/(stream->back->super->sectors_per_cluster*stream->back->super->bytes_per_sector);

    if(whence == SEEK_SET){

        if(cluster_index * stream->back->super->sectors_per_cluster*stream->back->super->bytes_per_sector + new_offset > (int)stream->size){
            errno=ENXIO;
            return -1;
        }

        stream->actual_cluster = cluster_index;
        disk_read(stream->back->disk,stream->back->first_data_sector+(stream->chain->clusters[cluster_index] - 2)*stream->back->super->sectors_per_cluster,stream->cluster,stream->back->super->sectors_per_cluster);
        stream->count = new_offset;
    }
    else if(whence == SEEK_CUR){
        //set actual claster * rozmiar clustra + count + offset
        return file_seek(stream,stream->actual_cluster*stream->back->super->sectors_per_cluster*stream->back->super->bytes_per_sector+stream->count+offset,SEEK_SET);
    }
    else if(whence == SEEK_END){
        //SEEK_SET rozmiar - offset
        return file_seek(stream,stream->size+offset,SEEK_SET);
    }
    else{
        errno = EINVAL;
        return -1;
    }
    return offset;
}
