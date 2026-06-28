#ifndef BMP_H
#define BMP_H

#include "types.h"

typedef struct {
    uint16 type;
    uint32 size;
    uint16 reserved1;
    uint16 reserved2;
    uint32 offset;
} __attribute__((packed)) BMP_FILE_H;

typedef struct {
    uint32 size;
    sint32 width;
    sint32 height;
    uint16 planes;
    uint16 bpp;
    uint32 compression;
    uint32 image_size;
    sint32 x_pixels_per_meter;
    sint32 y_pixels_per_meter;
    uint32 colors__used;
    uint32 colors_important;
} __attribute__((packed)) BMP_INFO_H;

typedef struct {
    int width;
    int height;
    uint32* data;
} Bitmap;

Bitmap* load_bmp(char* filename);
int save_bmp(const char* filename, Bitmap* bmp);
void free_bmp(Bitmap* bmp);

Bitmap* load_wah_image(char* filename);
int save_wah_image(const char* filename, Bitmap* bmp);

void draw_bmp(char* filename, int x, int y);
void draw_bmp_fit(char* filename, int x, int y, int dst_w, int dst_h);
void draw_bmp_gs(char* filename, int x, int y, int dst_w, int dst_h);

void predraw_bmp_icn(char* pixel_data, int src_w, int src_h, int src_row_padded, int x, int y, int dst_w, int dst_h);
void predraw_bmp_fit(char* pixel_data, int src_w, int src_h, int src_row_padded, int x, int y, int dst_w, int dst_h);

void draw_objbmp(Bitmap* bmp, int x, int y, int dst_w, int dst_h);

#endif