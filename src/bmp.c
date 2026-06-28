#include "bmp.h"
#include "lzav.h"
#include "fat32.h"
#include "graphics.h"
#include "vesa.h"
#include "string.h"
#include "kheap.h"
#include "serial.h"

// alloc bmp and the pixel buffer, free if fail
static Bitmap* bmp_alloc(uint32 width, uint32 height) {
    Bitmap* bmp = (Bitmap*)kmalloc(sizeof(Bitmap));
    if (!bmp) return NULL;
    bmp->width = width;
    bmp->height = height;
    bmp->data = (uint32*)kmalloc(width * height * sizeof(uint32));
    if (!bmp->data) { kfree(bmp); return NULL; }
    return bmp;
}

// will kfree *out_data when done
static char* bmp_open(char* filename, char** out_data, int* out_w, int* out_h, int* out_row_padded) {
    char* bmp_data = fat_read_file(filename);
    if (!bmp_data) return NULL;

    BMP_FILE_H* fh = (BMP_FILE_H*)bmp_data;
    if (fh->type != 0x4D42) { kfree(bmp_data); return NULL; }

    BMP_INFO_H* ih =
        (BMP_INFO_H*)(bmp_data + sizeof(BMP_FILE_H));

    if (ih->bpp != 24) { kfree(bmp_data); return NULL; }

    *out_data = bmp_data;
    *out_w = ih->width;
    *out_h = ih->height;
    *out_row_padded = (ih->width * 3 + 3) & ~3;
    return bmp_data + fh->offset;
}

static void bmp_fill_row(Bitmap* bmp, uint8* row_buf, int row, uint32 row_size) {
    uint32 pixel_bytes = bmp->width * 3;
    uint32* src_row = &bmp->data[(bmp->height - 1 - row) * bmp->width];

    for (int x = 0; x < bmp->width; x++) {
        uint32 color = src_row[x];
        row_buf[x*3 + 0] = (color >> 0) & 0xFF;
        row_buf[x*3 + 1] = (color >> 8) & 0xFF;
        row_buf[x*3 + 2] = (color >> 16) & 0xFF;
    }
    for (uint32 p = pixel_bytes; p < row_size; p++) row_buf[p] = 0;
}

//is_icon for skip white
static void predraw_bmp_scaled(char* pixel_data, int src_w, int src_h, int src_row_padded, int x, int y, int dst_w, int dst_h, bool screen_clip, bool is_icon) {
    if (!pixel_data || dst_w <= 0 || dst_h <= 0 || src_w <= 0) return;

    bool bottom_up = src_h > 0;
    int abs_src_h = bottom_up ? src_h : -src_h;
    if (abs_src_h <= 0) return;

    int screen_width = screen_clip ? vbe_get_width() : 0;
    int screen_height = screen_clip ? vbe_get_height() : 0;

    for (int dy = 0; dy < dst_h; dy++) {
        int screen_y = y + dy;
        if (screen_clip && (screen_y < 0 || screen_y >= screen_height)) continue;

        int sy = is_icon
               ? (dy * abs_src_h + dst_h / 2) / dst_h
               : (dy * abs_src_h) / dst_h;
        if (sy >= abs_src_h) sy = abs_src_h - 1;
        if (sy < 0) sy = 0;

        int row_index = bottom_up ? (abs_src_h - 1 - sy) : sy;
        char* src_row = pixel_data + row_index * src_row_padded;

        for (int dx = 0; dx < dst_w; dx++) {
            int screen_x = x + dx;
            if (screen_clip && (screen_x < 0 || screen_x >= screen_width)) continue;

            int sx = is_icon
                   ? (dx * src_w + dst_w / 2) / dst_w
                   : (dx * src_w) / dst_w;
            if (sx >= src_w) sx = src_w - 1;
            if (sx < 0) sx = 0;

            uint8 b = src_row[sx*3 + 0];
            uint8 g = src_row[sx*3 + 1];
            uint8 r = src_row[sx*3 + 2];
            uint32 color = RGB(r, g, b);

            if (is_icon && color == RGB(255, 255, 255)) continue;

            pixel(screen_x, screen_y, color);
        }
    }
}

void predraw_bmp_fit(char* pixel_data, int src_w, int src_h, int src_row_padded, int x, int y, int dst_w, int dst_h) {
    predraw_bmp_scaled(pixel_data, src_w, src_h, src_row_padded, x, y, dst_w, dst_h, true, false);
}

void predraw_bmp_icn(char* pixel_data, int src_w, int src_h, int src_row_padded, int x, int y, int dst_w, int dst_h) {
    predraw_bmp_scaled(pixel_data, src_w, src_h, src_row_padded, x, y, dst_w, dst_h, false, true);
}

void draw_bmp(char* filename, int x, int y) {
    char* bmp_data;
    int w, h, row_padded;
    char* pixels = bmp_open(filename, &bmp_data, &w, &h, &row_padded);
    if (!pixels) return;

    for (int i = 0; i < h; i++) {
        char* row = pixels + (h - 1 - i) * row_padded;
        for (int j = 0; j < w; j++)
            pixel(x + j, y + i,
                  RGB((uint8)row[j*3 + 2],
                          (uint8)row[j*3 + 1],
                          (uint8)row[j*3 + 0]));
    }
    kfree(bmp_data);
}

void draw_bmp_gs(char* filename, int x, int y, int dst_w, int dst_h) {
    char* bmp_data;
    int src_w, src_h, row_padded;
    char* pixels = bmp_open(filename, &bmp_data, &src_w, &src_h, &row_padded);
    if (!pixels) return;
    if (dst_w > 0 && dst_h > 0)
        predraw_bmp_icn(pixels, src_w, src_h, row_padded, x, y, dst_w, dst_h);
    kfree(bmp_data);
}

Bitmap* load_bmp(char* filename) {
    char* bmp_data = fat_read_file(filename);
    if (!bmp_data) return NULL;

    BMP_FILE_H* fh = (BMP_FILE_H*)bmp_data;
    if (fh->type != 0x4D42) { kfree(bmp_data); return NULL; }

    BMP_INFO_H* ih =
        (BMP_INFO_H*)(bmp_data + sizeof(BMP_FILE_H));

    if ((ih->bpp != 24 && ih->bpp != 32) || ih->compression != 0) {
        kfree(bmp_data); return NULL;
    }

    int abs_h = ih->height < 0 ? -ih->height : ih->height;
    if (ih->width <= 0 || abs_h <= 0 || ih->width > 2048 || abs_h > 2048 || (uint64)ih->width * abs_h > 262144) {
        kfree(bmp_data); return NULL;
    }

    Bitmap* bmp = bmp_alloc(ih->width, abs_h);
    if (!bmp) { kfree(bmp_data); return NULL; }

    bool bottom_up = ih->height > 0;
    int bytes_per_pixel = ih->bpp / 8;
    int row_padded = (bmp->width * bytes_per_pixel + 3) & ~3;
    char* pixel_data = bmp_data + fh->offset;

    for (int i = 0; i < abs_h; i++) {
        int src_row = bottom_up ? i : (abs_h - 1 - i);
        char* row = pixel_data + src_row * row_padded;
        int dst_row = abs_h - 1 - i;
        for (int j = 0; j < bmp->width; j++)
            bmp->data[dst_row * bmp->width + j] =
                RGB((uint8)row[j*bytes_per_pixel + 2],
                        (uint8)row[j*bytes_per_pixel + 1],
                        (uint8)row[j*bytes_per_pixel + 0]);
    }

    kfree(bmp_data);
    return bmp;
}

void free_bmp(Bitmap* bmp) {
    if (!bmp) return;
    kfree(bmp->data);
    kfree(bmp);
}

typedef struct {
    Bitmap* bmp;
    uint32 row_size;
    uint32 header_size;
    uint8* row_buf;
    int last_row;
    BMP_FILE_H file_header;
    BMP_INFO_H info_header;
} BMPWriteContext;

static int bmp_fill_fchunk(void* ctx, uint32 offset, uint32 size, char* buffer) {
    BMPWriteContext* w = (BMPWriteContext*)ctx;
    const uint32 header_size = w->header_size;
    uint32 bytes_written = 0;

    while (bytes_written < size) {
        uint32 pos = offset + bytes_written;

        if (pos < header_size) {
            uint32 tocopy = (size - bytes_written < header_size - pos)
                          ? size - bytes_written : header_size - pos;
            uint32 copied = 0;

            if (pos < sizeof(BMP_FILE_H)) {
                uint32 part = sizeof(BMP_FILE_H) - pos;
                if (part > tocopy) part = tocopy;
                memcpy(buffer + bytes_written,
                       (uint8*)&w->file_header + pos, part);
                copied += part;
                pos += part;
            }
            if (copied < tocopy) {
                uint32 info_pos = pos - sizeof(BMP_FILE_H);
                memcpy(buffer + bytes_written + copied,
                       (uint8*)&w->info_header + info_pos, tocopy - copied);
            }
            
            bytes_written += tocopy;
            continue;
        }

        uint32 image_pos = pos - header_size;
        uint32 row = image_pos / w->row_size;
        uint32 row_offset = image_pos % w->row_size;
        uint32 tocopy = w->row_size - row_offset;
        if (tocopy > size - bytes_written) tocopy = size - bytes_written;

        if (row >= (uint32)w->bmp->height) {
            memset(buffer + bytes_written, 0, tocopy);
        } else {
            if (w->last_row != (int)row) {
                bmp_fill_row(w->bmp, w->row_buf, row, w->row_size);
                w->last_row = (int)row;
            }
            memcpy(buffer + bytes_written, w->row_buf + row_offset, tocopy);
        }
        bytes_written += tocopy;
    }
    return (int)size;
}

int save_bmp(const char* filename, Bitmap* bmp) {
    if (!bmp || !bmp->data) return -1;

    int row_size = (bmp->width * 3 + 3) & ~3;
    uint32 image_size = row_size * bmp->height;
    uint32 file_size = sizeof(BMP_FILE_H) + sizeof(BMP_INFO_H) + image_size;

    BMPWriteContext w = {
        .bmp = bmp,
        .row_size = row_size,
        .header_size = sizeof(BMP_FILE_H) + sizeof(BMP_INFO_H),
        .last_row = -1,
        .row_buf = (uint8*)kmalloc(row_size),
        .file_header = {
            .type = 0x4D42,
            .size = file_size,
            .reserved1 = 0, .reserved2 = 0,
            .offset = sizeof(BMP_FILE_H) + sizeof(BMP_INFO_H)
        },
        .info_header = {
            .size = sizeof(BMP_INFO_H),
            .width = bmp->width,
            .height = bmp->height,
            .planes = 1,
            .bpp = 24,
            .compression = 0,
            .image_size = image_size,
            .x_pixels_per_meter = 0, .y_pixels_per_meter = 0,
            .colors__used = 0, .colors_important = 0
        }
    };
    if (!w.row_buf) return -1;

    int result = fat_write_file_call(filename, file_size, &w, bmp_fill_fchunk);
    kfree(w.row_buf);
    return result;
}

// wah2 is delta-encoded and LZAV-compressed, huge glow-up from wah1 (raw uncomp)
typedef struct {
    uint8 magic[4];
    uint32 width;
    uint32 height;
} __attribute__((packed)) WAHIMAGEHEADER;

typedef struct {
    uint8 magic[4];
    uint32 width;
    uint32 height;
    uint32 compressed_size; // byte len of lzav blob
} __attribute__((packed)) WAH2_HEADER;

static const uint8 WAH1_MAGIC[4] = {'W', 'A', 'H', '1'};
static const uint8 WAH2_MAGIC[4] = {'W', 'A', 'H', '2'};

typedef struct {
    const uint8* header;
    uint32 header_size;
    const uint8* data;
    uint32 data_size;
} FlatWriteCtx;

static int flat_fill_chunk(void* ctx, uint32 offset, uint32 size, char* buffer) {
    FlatWriteCtx* w = (FlatWriteCtx*)ctx;

    uint32 written = 0;
    while (written < size) {
        uint32 pos = offset + written;
        uint32 remaining = size - written;

        if (pos < w->header_size) {
            uint32 n = w->header_size - pos;
            if (n > remaining) n = remaining;
            memcpy(buffer + written, w->header + pos, n);
            written += n;
        } else {
            uint32 data_pos = pos - w->header_size;
            uint32 n = w->data_size - data_pos;
            if (n > remaining) n = remaining;
            memcpy(buffer + written, w->data + data_pos, n);
            written += n;
        }
    }
    return (int)size;
}

static Bitmap* decode_wah1(const char* raw_data, uint32 file_size) {
    const WAHIMAGEHEADER* hdr = (const WAHIMAGEHEADER*)raw_data;

    if (hdr->width == 0 || hdr->height == 0 ||
        hdr->width > 2048 || hdr->height > 2048 ||
        (uint64)hdr->width * hdr->height > 262144 ||
        file_size != sizeof(WAHIMAGEHEADER) + hdr->width * hdr->height * 3)
        return NULL;

    Bitmap* bmp = bmp_alloc(hdr->width, hdr->height);
    if (!bmp) return NULL;

    const uint8* px = (const uint8*)raw_data + sizeof(WAHIMAGEHEADER);
    uint32 count = hdr->width * hdr->height;
    for (uint32 i = 0; i < count; i++)
        bmp->data[i] = RGB(px[i*3 + 2], px[i*3 + 1], px[i*3 + 0]);

    return bmp;
}

static void delta_encode(uint8* buf, uint32 size) {
    for (uint32 i = size - 1; i >= 3; i--)
        buf[i] -= buf[i - 3];
}

static void delta_decode(uint8* buf, uint32 size) {
    for (uint32 i = 3; i < size; i++)
        buf[i] += buf[i - 3];
}

int save_wah_image(const char* filename, Bitmap* bmp) {
    if (!bmp || !bmp->data) return -1;

    uint32 pixel_count = bmp->width * bmp->height;
    uint32 rgb_size = pixel_count * 3;

    //raw buf
    uint8* rgb_data = (uint8*)kmalloc(rgb_size);
    if (!rgb_data) return -1;

    for (uint32 i = 0; i < pixel_count; i++) {
        uint32 color = bmp->data[i];
        rgb_data[i*3 + 0] = (color >> 0) & 0xFF;
        rgb_data[i*3 + 1] = (color >> 8) & 0xFF;
        rgb_data[i*3 + 2] = (color >> 16) & 0xFF;
    }

    delta_encode(rgb_data, rgb_size);

    int cap = lzav_compress_bound(rgb_size);
    uint8* comp_buf = (uint8*)kmalloc(cap);
    if (!comp_buf) { kfree(rgb_data); return -1; }

    int comp_size = lzav_compress_default(rgb_data, comp_buf, rgb_size, cap);
    kfree(rgb_data);

    if (comp_size <= 0) { kfree(comp_buf); return -1; }

    WAH2_HEADER hdr;
    memcpy(hdr.magic, WAH2_MAGIC, 4);
    hdr.width = bmp->width;
    hdr.height = bmp->height;
    hdr.compressed_size = (uint32)comp_size;

    FlatWriteCtx w = {
        .header = (const uint8*)&hdr,
        .header_size = sizeof(hdr),
        .data = comp_buf,
        .data_size = (uint32)comp_size
    };
    int result = fat_write_file_call(filename, sizeof(hdr) + comp_size,
                                   &w, flat_fill_chunk);
    kfree(comp_buf);
    return result;
}

static Bitmap* decode_wah2(const char* raw_data, uint32 file_size) {
    const WAH2_HEADER* hdr = (const WAH2_HEADER*)raw_data;

    if (file_size < sizeof(WAH2_HEADER)) return NULL;

    if (hdr->width == 0 || hdr->height == 0 ||
        hdr->width > 2048 || hdr->height > 2048 ||
        (uint64)hdr->width * hdr->height > 262144)
        return NULL;

    uint32 rgb_size = hdr->width * hdr->height * 3;

    if (hdr->compressed_size == 0 ||
        hdr->compressed_size != file_size - sizeof(WAH2_HEADER))
        return NULL;

    uint8* rgb_data = (uint8*)kmalloc(rgb_size);
    if (!rgb_data) return NULL;

    const uint8* comp_data = (const uint8*)raw_data + sizeof(WAH2_HEADER);
    int result = lzav_decompress(comp_data, rgb_data,
                                 hdr->compressed_size, rgb_size);
    if (result != (int)rgb_size) {
        kfree(rgb_data);
        return NULL;
    }

    delta_decode(rgb_data, rgb_size);

    Bitmap* bmp = bmp_alloc(hdr->width, hdr->height);
    if (!bmp) { kfree(rgb_data); return NULL; }

    uint32 pixel_count = hdr->width * hdr->height;
    for (uint32 i = 0; i < pixel_count; i++)
        bmp->data[i] = RGB(rgb_data[i*3 + 2],
                                rgb_data[i*3 + 1],
                                rgb_data[i*3 + 0]);

    kfree(rgb_data);
    return bmp;
}

Bitmap* load_wah_image(char* filename) {
    if (!filename) return NULL;

    FAT_dirent* de = fat_find_file(filename);
    if (!de) return NULL;
    uint32 file_size = de->Size;
    kfree(de);

    if (file_size < sizeof(WAHIMAGEHEADER)) return NULL;

    char* raw_data = fat_read_file(filename);
    if (!raw_data) return NULL;

    Bitmap* bmp;
    if (memcmp(raw_data, WAH2_MAGIC, 4) == 0)
        bmp = decode_wah2(raw_data, file_size);
    else if (memcmp(raw_data, WAH1_MAGIC, 4) == 0)
        bmp = decode_wah1(raw_data, file_size);
    else
        bmp = NULL;

    kfree(raw_data);
    return bmp;
}

void draw_objbmp(Bitmap* bmp, int x, int y, int dst_w, int dst_h) {
    if (!bmp || !bmp->data) return;

    int src_w = bmp->width;
    int src_h = bmp->height;
    int screen_width = vbe_get_width();
    int screen_height = vbe_get_height();

    for (int dy = 0; dy < dst_h; dy++) {
        int screen_y = y + dy;
        if (screen_y < 0 || screen_y >= screen_height) continue;

        int sy = (dy * src_h) / dst_h;
        if (sy >= src_h) sy = src_h - 1;
        if (sy < 0) sy = 0;

        for (int dx = 0; dx < dst_w; dx++) {
            int screen_x = x + dx;
            if (screen_x < 0 || screen_x >= screen_width) continue;

            int sx = (dx * src_w) / dst_w;
            if (sx >= src_w) sx = src_w - 1;
            if (sx < 0) sx = 0;

            pixel(screen_x, screen_y, bmp->data[sy * src_w + sx]);
        }
    }
}