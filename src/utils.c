#include "utils.h"
#include "serial.h"
#include "console.h"
#include "fat32.h"
#include "kheap.h"
#include "string.h"
#include "bmp.h"
#include "types.h"

void panic(char *msg){
    printf("\nbweh. rock is down! \nhalt is requested due to: %s\n", msg);
    kprint("\nbweh. rock is down! \nhalt is requested due to: %s\n", msg);
    asm volatile("hlt");
}


preloaded_t* preload_bmp(const char* filename) {
    char* bmp_data = fat_read_file((char*)filename);
    if (!bmp_data) return NULL;

    BMP_FILE_H* fh = (BMP_FILE_H*)bmp_data;
    if (fh->type != 0x4D42) {
        kfree(bmp_data);
        return NULL;
    }

    BMP_INFO_H* ih = (BMP_INFO_H*)(bmp_data + sizeof(BMP_FILE_H));
    if (ih->bpp != 24 || ih->width <= 0 || ih->height <= 0) {
        kfree(bmp_data);
        return NULL;
    }

    preloaded_t* preloaded = (preloaded_t*)kmalloc(sizeof(preloaded_t));
    if (!preloaded) {
        kfree(bmp_data);
        return NULL;
    }

    preloaded->width = ih->width;
    preloaded->height = ih->height;
    preloaded->row_padded = (preloaded->width * 3 + 3) & ~3;
    
    uint32 image_size = preloaded->row_padded * preloaded->height;
    preloaded->pixel_data = (char*)kmalloc(image_size);
    if (!preloaded->pixel_data) {
        kfree(preloaded);
        kfree(bmp_data);
        return NULL;
    }

    memcpy(preloaded->pixel_data, bmp_data + fh->offset, image_size);

    kfree(bmp_data);
    return preloaded;
}

void free_preloaded_bmp(preloaded_t* bmp) {
    if (bmp) {
        if (bmp->pixel_data) {
            kfree(bmp->pixel_data);
        }
        kfree(bmp);
    }
}