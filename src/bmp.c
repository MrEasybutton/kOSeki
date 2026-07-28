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

// wah3 now with conditional alpha-channel, CRC32 integrity check, row-block-chunks
//               ^ save-time detection      ^ corrupts pass null   ^ reduce scratch mem for decoder
#define WAH3_BLOCK_ROWS 32u
#define WAH3_FLAG_ALPHA 0x01u

typedef struct {
    uint8 magic[4];
    uint8 version;
    uint8 flags; //bit0 to enable alpha
    uint16 rows;
    uint32 width;
    uint32 height;
    uint32 block_count;
    uint32 crc32;
} __attribute__((packed)) WAH3_HEADER;

static const uint8 WAH3_MAGIC[4] = {'W', 'A', 'H', '3'};

#define CRC32_INIT 0xFFFFFFFFu
#define CRC32_FINAL(c) (~(c))

static uint32 crc32_update(uint32 crc, const uint8* data, uint32 len) {
    for (uint32 i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) { crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320u : 0u); }
    }
    return crc;
}

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

static void delta_encode(uint8* buf, uint32 size, uint32 stride) {
    for (uint32 i = size - 1; i >= stride; i--) buf[i] -= buf[i - stride];
}

static void delta_decode(uint8* buf, uint32 size, uint32 stride) {
    for (uint32 i = stride; i < size; i++) buf[i] += buf[i - stride];
}

int save_wah(const char* filename, Bitmap* bmp) {
    if (!bmp || !bmp->data) return -1;

    uint32 pixel_count = bmp->width * bmp->height;

    bool has_alpha = false;
    for (uint32 i = 0; i < pixel_count; i++) {
        if (bmp->data[i] & 0xFF000000u) { has_alpha = true; break; }
    }
    uint8 bpp = has_alpha ? 4 : 3;

    uint32 rows = WAH3_BLOCK_ROWS;
    uint32 block_count = (bmp->height + rows - 1) / rows;
    uint32 row_bytes = bmp->width * bpp;
    uint32 max_block_bytes = rows * row_bytes;

    uint32* block_size = (uint32*)kmalloc(block_count * sizeof(uint32));
    uint8* raw_block = (uint8*)kmalloc(max_block_bytes);
    if (!block_size || !raw_block) {
        kfree(block_size); kfree(raw_block);
        return -1;
    }

    // worst-case will be sum of bounds per-block
    uint32 total_cap = 0;
    for (uint32 b = 0; b < block_count; b++) {
        uint32 rows_this_block = rows;
        if ((b + 1) * rows > bmp->height)
            rows_this_block = bmp->height - b * rows;
        total_cap += (uint32)lzav_compress_bound(rows_this_block * row_bytes);
    }

    uint8* comp_all = (uint8*)kmalloc(total_cap);
    if (!comp_all) {
        kfree(block_size); kfree(raw_block);
        return -1;
    }

    uint32 offset = 0;
    for (uint32 b = 0; b < block_count; b++) {
        uint32 row0 = b * rows;
        uint32 rows_this_block = rows;
        if (row0 + rows_this_block > bmp->height)
            rows_this_block = bmp->height - row0;
        uint32 block_bytes = rows_this_block * row_bytes;

        for (uint32 r = 0; r < rows_this_block; r++) {
            uint32 pix_row = row0 + r;
            for (uint32 x = 0; x < bmp->width; x++) {
                uint32 color = bmp->data[pix_row * bmp->width + x];
                uint8* dst = raw_block + r * row_bytes + x * bpp;
                dst[0] = (color >> 0) & 0xFF;
                dst[1] = (color >> 8) & 0xFF;
                dst[2] = (color >> 16) & 0xFF;
                if (has_alpha) dst[3] = (color >> 24) & 0xFF;
            }
        }

        delta_encode(raw_block, block_bytes, bpp);

        int cap = (int)(total_cap - offset);
        int csize = lzav_compress_default(raw_block, comp_all + offset, block_bytes, cap);
        if (csize <= 0) {
            kfree(block_size); kfree(raw_block); kfree(comp_all);
            return -1;
        }
        block_size[b] = (uint32)csize;
        offset += (uint32)csize;
    }

    kfree(raw_block);
    uint32 total_compressed = offset;

    WAH3_HEADER hdr;
    memcpy(hdr.magic, WAH3_MAGIC, 4);
    hdr.version = 1;
    hdr.flags = has_alpha ? WAH3_FLAG_ALPHA : 0;
    hdr.rows = (uint16)rows;
    hdr.width = bmp->width;
    hdr.height = bmp->height;
    hdr.block_count = block_count;

    uint32 table_bytes = block_count * sizeof(uint32);
    uint32 crc = CRC32_INIT;
    crc = crc32_update(crc, (uint8*)block_size, table_bytes);
    crc = crc32_update(crc, comp_all, total_compressed);
    hdr.crc32 = CRC32_FINAL(crc);

    uint32 header_blob_size = sizeof(WAH3_HEADER) + table_bytes;
    uint8* header_blob = (uint8*)kmalloc(header_blob_size);
    if (!header_blob) {
        kfree(block_size); kfree(comp_all);
        return -1;
    }
    memcpy(header_blob, &hdr, sizeof(WAH3_HEADER));
    memcpy(header_blob + sizeof(WAH3_HEADER), block_size, table_bytes);
    kfree(block_size);

    FlatWriteCtx w = {
        .header = header_blob,
        .header_size = header_blob_size,
        .data = comp_all,
        .data_size = total_compressed
    };
    int result = fat_write_file_call(filename, header_blob_size + total_compressed, &w, flat_fill_chunk);
    kfree(header_blob);
    kfree(comp_all);
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
    int result = lzav_decompress(comp_data, rgb_data, hdr->compressed_size, rgb_size);
    if (result != (int)rgb_size) {
        kfree(rgb_data);
        return NULL;
    }

    delta_decode(rgb_data, rgb_size, 3);

    Bitmap* bmp = bmp_alloc(hdr->width, hdr->height);
    if (!bmp) { kfree(rgb_data); return NULL; }

    uint32 pixel_count = hdr->width * hdr->height;
    for (uint32 i = 0; i < pixel_count; i++) {
        bmp->data[i] = RGB(rgb_data[i*3 + 2], rgb_data[i*3 + 1], rgb_data[i*3 + 0]);
    }

    kfree(rgb_data);
    return bmp;
}

static Bitmap* decode_wah3(const char* raw_data, uint32 file_size) {
    if (file_size < sizeof(WAH3_HEADER)) return NULL;
    const WAH3_HEADER* hdr = (const WAH3_HEADER*)raw_data;

    if (hdr->width == 0 || hdr->height == 0 || hdr->width > 2048 || hdr->height > 2048 || (uint64)hdr->width * hdr->height > 262144) {
        return NULL;
    }

    if (hdr->rows == 0 || hdr->block_count == 0) return NULL;

    uint32 expected_blocks = (hdr->height + hdr->rows - 1) / hdr->rows;
    if (hdr->block_count != expected_blocks) return NULL;

    uint8 bpp = (hdr->flags & WAH3_FLAG_ALPHA) ? 4 : 3;

    uint32 table_bytes = hdr->block_count * sizeof(uint32);
    uint32 header_and_table = sizeof(WAH3_HEADER) + table_bytes;
    if (file_size < header_and_table) return NULL;

    const uint32* block_size = (const uint32*)(raw_data + sizeof(WAH3_HEADER));
    const uint8* payload = (const uint8*)raw_data + header_and_table;
    uint32 payload_size = file_size - header_and_table;

    // table sum must be equal to the remaining bytes
    uint64 total_compressed = 0;
    for (uint32 b = 0; b < hdr->block_count; b++) total_compressed += block_size[b];
    if (total_compressed != payload_size) return NULL;

    uint32 crc = CRC32_INIT;
    crc = crc32_update(crc, (const uint8*)block_size, table_bytes);
    crc = crc32_update(crc, payload, payload_size);
    if (CRC32_FINAL(crc) != hdr->crc32) return NULL;

    Bitmap* bmp = bmp_alloc(hdr->width, hdr->height);
    if (!bmp) return NULL;

    uint32 row_bytes = hdr->width * bpp;
    uint32 first_block_rows = (hdr->rows < hdr->height) ? hdr->rows : hdr->height;
    uint32 max_block_bytes = first_block_rows * row_bytes;
    uint8* raw_block = (uint8*)kmalloc(max_block_bytes);
    if (!raw_block) { free_bmp(bmp); return NULL; }

    uint32 offset = 0;
    for (uint32 b = 0; b < hdr->block_count; b++) {
        uint32 row0 = b * hdr->rows;
        uint32 rows_this_block = hdr->rows;
        if (row0 + rows_this_block > hdr->height)
            rows_this_block = hdr->height - row0;
        uint32 block_bytes = rows_this_block * row_bytes;

        int result = lzav_decompress(payload + offset, raw_block, block_size[b], block_bytes);
        if (result != (int)block_bytes) {
            kfree(raw_block); free_bmp(bmp); return NULL;
        }
        offset += block_size[b];

        delta_decode(raw_block, block_bytes, bpp);

        for (uint32 r = 0; r < rows_this_block; r++) {
            uint32 pix_row = row0 + r;
            for (uint32 x = 0; x < hdr->width; x++) {
                uint8* src = raw_block + r * row_bytes + x * bpp;
                uint32 color = RGB(src[2], src[1], src[0]);
                if (bpp == 4) color |= ((uint32)src[3] << 24);
                bmp->data[pix_row * hdr->width + x] = color;
            }
        }
    }

    kfree(raw_block);
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
    if (memcmp(raw_data, WAH3_MAGIC, 4) == 0)
        bmp = decode_wah3(raw_data, file_size);
    else if (memcmp(raw_data, WAH2_MAGIC, 4) == 0) // im keeping these here for legacy
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