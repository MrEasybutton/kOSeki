#ifndef CC_H
#define CC_H

#include "types.h"
#include "serial.h"
#include "string.h"
#include <stddef.h>

typedef uint8 u8_t;
typedef sint8 s8_t;
typedef uint16 u16_t;
typedef sint16 s16_t;
typedef uint32 u32_t;
typedef sint32 s32_t;

typedef uint32 mem_ptr_t;
typedef uint32 sys_prot_t;

#define U16_F "u"
#define S16_F "d"
#define X16_F "x"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"
#define X8_F "02x"
#define SZT_F "u"

extern uint32 timer_ticks;
#define LWIP_RAND() ((u32_t)timer_ticks * 1103515245 + 12345)

#define BYTE_ORDER LITTLE_ENDIAN

#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_FIELD(x) x

#define LWIP_PLATFORM_DIAG(x) do { kprint x; } while(0)
#define LWIP_PLATFORM_ASSERT(x) do { kprint("assertion \"%s\" failed (line %d in %s)\n", x, __LINE__, __FILE__); } while(0)

#include "kheap.h"
#define mem_clib_malloc kmalloc
#define mem_clib_free kfree

#endif