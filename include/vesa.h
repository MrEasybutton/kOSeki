#ifndef VESA_H
#define VESA_H

#include "types.h"

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768

//adpated from https://github.com/xing1357/SimpleOS/blob/main/include/vesa.h
// similar to my v2 but i figure the comments here will be helpful

extern uint32 g_width, g_height;
extern uint32 *g_back_buffer;

typedef struct {
    char VbeSignature[4];           /* VBE Signature */
    uint16 VbeVersion;              /* VBE version number */
    uint32 OEMStringPtr;             /* Far pointer to OEM string */
    uint32 Capabilities;            /* Capabilities of video card */
    uint32 VideoModePtr;           /* Far pointer to supported modes */
    uint16 TotalMemory;             /* Number of 64kb memory blocks */
    uint16 OemSoftwareRev;          /* VBE implementation Software revision */
    uint32 OemVendorNamePtr;        /* Far pointer to Vendor Name String */
    uint32 OemProductNamePtr;       /* Far pointer to Product Name String */
    uint32 OemProductRevPtr;        /* Far pointer to Product Revision String */
    char reserved[222];             /* Pad to 256 byte block size */
    char OemData[256];              /* Data Area for OEM Strings */
}__attribute__ ((packed)) VBE20_INFOBLOCK;

typedef struct {
    // Mandatory information for all VBE revisions
    uint16 ModeAttributes;          /* Mode attributes */
    uint8 WinAAttributes;           /* Window A attributes */
    uint8 WinBAttributes;           /* Window B attributes */
    uint16 WinGranularity;          /* Window granularity in k */
    uint16 WinSize;                 /* Window size in k */
    uint16 WinASegment;             /* Window A segment */
    uint16 WinBSegment;             /* Window B segment */
    void (*WinFuncPtr)(void);       /* Pointer to window function */
    uint16 BytesPerScanLine;        /* Bytes per scanline */

    // Mandatory information for VBE 1.2 and above
    uint16 XResolution;             /* Horizontal resolution */
    uint16 YResolution;             /* Vertical resolution */
    uint8 XCharSize;                /* Character cell width */
    uint8 YCharSize;                /* Character cell height */
    uint8 NumberOfPlanes;           /* Number of memory planes */
    uint8 BitsPerPixel;             /* Bits per pixel */
    uint8 NumberOfBanks;            /* Number of CGA style banks */
    uint8 MemoryModel;              /* Memory model type */
    uint8 BankSize;                 /* Size of CGA style banks */
    uint8 NumberOfImagePages;       /* Number of images pages */
    uint8 Reserved;                 /* Reserved */

    // Direct color fields
    uint8 RedMaskSize;              /* Size of direct color red mask */
    uint8 RedFieldPosition;         /* Bit posn of lsb of red mask */
    uint8 GreenMaskSize;            /* Size of direct color green mask */
    uint8 GreenFieldPosition;         /* Bit posn of lsb of green mask */
    uint8 BlueMaskSize;              /* Size of direct color blue mask */
    uint8 BlueFieldPosition;        /* Bit posn of lsb of blue mask */
    uint8 RsvdMaskSize;             /* Size of direct color res mask */
    uint8 RsvdFieldPosition;        /* Bit posn of lsb of res mask */
    uint8 DirectColorModeInfo;      /* Direct color mode attributes */

    // Mandatory information for VBE 2.0 and above
    uint32 PhysBasePtr;             /* physical address for flat frame buffer */
    uint32 OffScreenMemOffset;      /* pointer to start of off screen memory */
    uint16 OffScreenMemSize;        /* amount of off screen memory in 1k units */
    uint8 Reserved2[206];           /* remainder of ModeInfoBlock */
} VBE20_MODEINFOBLOCK;


#define RGBA(r, g, b, a) ((((uint32)(a)) << 24) | (((uint32)(r)) << 16) | (((uint32)(g)) << 8) | ((uint32)(b)))
#define RGB(r, g, b) RGBA(r, g, b, 255)

uint32 vbe_get_width();
uint32 vbe_get_height();
void swapbuf(void);
void swapbuf_region(int x, int y, int w, int h);

void vesa_cleanup(void);
int vesa_init(uint32 width, uint32 height, uint32 bpp);
uint32 blend_pixel(uint32 fg_color, uint32 bg_color);

void pixel(int x, int y, int color);
uint32 getpixel(int x, int y);

#endif