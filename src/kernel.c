#include "kernel.h"
#include "cmdtree.h"
#include "console.h"
#include "string.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "bae.h"
#include "multiboot.h"
#include "graphics.h"
#include "ide.h"
#include "pmm.h"
#include "kheap.h"
#include "vesa.h"
#include "bitmap.h"
#include "fat32.h"
#include "bmp.h"
#include "video.h"
#include "gui.h"
#include "ac97.h"
#include "apps/CLPlayer.h"
#include "synth.h"
#include "fat32.h"
#include "speaker.h"
#include "cmos.h"
#include "isr.h"
#include "kronii.h"
#include "pci.h"
#include "e1000.h"
#include "net.h"
#include "utils.h"
#include "procsys.h"
#include "timer.h"
#include "cmos.h"
#include "serial.h"
#include "lwip/timeouts.h"
#include "kmath.h"
#include "string.h"
#include "apps/Pebbleshell.h"
#include "apps/Reaper.h"
#include "apps/WAHtercolour.h"
#include "apps/Novella.h"
#include "apps/CLStudio.h"
#include "baux2/baux2.h"

int CLOCK_X = 880;
int CLOCK_Y = 3;

uint32 PM1A_CNT_BLK, PM1B_CNT_BLK = 0;

KERNEL_MEMORY_MAP g_kmap;
uint8 g_video_mode = VIDEO_MODE_TEXT;

extern char* fat_read_file(char* fpath);
extern void cmd(char* command_line, char* pbsh_buf, int* pbsh_len);

int atoi(const char* str) {
    int result = 0;
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] < '0' || str[i] > '9') break;
        result = result * 10 + (str[i] - '0');
    }
    return result;
}

uint32 g_current_dir_cluster = 2;
char g_current_path[256] = "/";

char* g_wallpaper_path = "SYSTEM/bliss-wllp.bmp";
char* g_wallpaper_bmp_base_data = NULL;
char* g_wllp_data = NULL;
int g_wllp_width = 0;
int g_wllp_height = 0;
int g_wallpaper_row_padded = 0;

static uint32* g_wallpaper_cache = NULL;
static int g_wallpaper_cache_width = 0;
static int g_wallpaper_cache_height = 0;
static BOOL g_wallpaper_cache_valid = FALSE;

static void draw_bootfrm(const char* title, const char* message) {
    rect_grad(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, RGB(186, 158, 224), RGB(240, 182, 224));

    rect_grad(0, 0, SCREEN_WIDTH, 36, RGB(118, 90, 168), RGB(168, 132, 198));
    rect_grad(0, 0, SCREEN_WIDTH, 16, RGB(255, 250, 255), RGB(196, 168, 220));
    rect(0, 36, SCREEN_WIDTH, 1, RGB(240, 232, 250));

    int box_x = (SCREEN_WIDTH - 360) / 2;
    int box_y = (SCREEN_HEIGHT - 140) / 2;
    int box_w = 360;
    int box_h = 140;
    int pad_x = 16;
    int text_x = box_x + pad_x;

    rect(box_x + 4, box_y + 6, box_w, box_h, RGB(150, 130, 176));

    rect(box_x, box_y, box_w, box_h, RGB(255, 255, 255));
    rect(box_x + 2, box_y + 2, box_w - 4, box_h - 4, RGB(214, 198, 236));
    rect(box_x + 6, box_y + 6, box_w - 12, box_h - 12, RGB(247, 242, 255));

    rect_grad(box_x + 6, box_y + 6, box_w - 12, (box_h - 12) * 2 / 5, RGB(255, 255, 255), RGB(247, 242, 255));

    rect(box_x + 6, box_y + 6, box_w - 12, 2, RGB(255, 255, 255));

    if (title)
        text_ex(title, text_x, box_y + 12, RGB(74, 62, 104), FONT_DEFAULT, TRUE, 2);

    if (message) {
        char l1[96] = {0}, l2[96] = {0};

        if (strlen(message) > 34) {
            int split = 34;
            while (split > 0 && message[split] != ' ')
                split--;

            if (split > 0) {
                memcpy(l1, message, split);
                l1[split] = 0;
                strcpy(l2, message + split + 1);

                text_ex(l1, text_x, box_y + 50, RGB(94, 80, 120), FONT_KALNIA, FALSE, 1);
                text_ex(l2, text_x, box_y + 64, RGB(94, 80, 120), FONT_KALNIA, FALSE, 1);
            } else {
                text_ex(message, text_x, box_y + 57, RGB(94, 80, 120), FONT_KALNIA, FALSE, 1);
            }
        } else text_ex(message, text_x, box_y + 57, RGB(94, 80, 120), FONT_KALNIA, FALSE, 1);
    }

    text_ex("BIBOO is preparing...", text_x, box_y + 88, RGB(122, 108, 152), FONT_DEFAULT, TRUE, 1);
    text_ex("please wait warmly", text_x, box_y + 106, RGB(122, 108, 152), FONT_DEFAULT, TRUE, 1);

    swapbuf();
}

void splash(const char* message) {
    if (vesa_init(SCREEN_WIDTH, SCREEN_HEIGHT, 32) < 0) return;
    draw_bootfrm("kOSeki", message ? message : "BOOTING...");
}

void set_text_mode() {
    g_video_mode = VIDEO_MODE_TEXT;
    console_init(COLOR_BRIGHT_MAGENTA, COLOR_BLACK);
    printf("text. \n");
}

void placeholdact() {
    printf("icon clicked\n");
}

void delay(uint32 value) {
    for (volatile uint32 i = 0; i < value;) {
        __asm__ __volatile__ (
            "nop\n"
        );
        i++;
    }
}

extern BOOL _kb_combo(BOOL* was_pressed, uint8 key1, uint8 key2);
static BOOL _kb_reboot(void) {
    static BOOL was_pressed = FALSE;
    return _kb_combo(&was_pressed, SCAN_CODE_KEY_LEFT_CTRL, SCAN_CODE_KEY_L);
}
static BOOL _kb_shutdown(void) {
    static BOOL was_pressed = FALSE;
    return _kb_combo(&was_pressed, SCAN_CODE_KEY_LEFT_CTRL, SCAN_CODE_KEY_K);
}

static inline void outb(uint16 port, uint8 val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outw(uint16 port, uint16 val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

void reboot(void) {
    asm volatile("cli");
    outb(0x64, 0xFE);
    while (1) asm volatile("hlt");
}

void shutdown(uint32 pm1a_cnt, uint32 pm1b_cnt) {
    asm volatile("cli");

    // no fadt parsed? fallback to kb reset
    if (pm1a_cnt == 0) {
        outb(0x64, 0xFE);
        while (1) asm volatile("hlt");
    }

    uint16 slp_en = (1 << 13);
    uint16 slp_typ[] = { 5, 0, 7, 1, 2 }; //commons

    for (int i = 0; i < 5; i++) {
        uint16 payload = (slp_typ[i] << 10) | slp_en;
        outw((uint16)pm1a_cnt, payload);
        if (pm1b_cnt) outw((uint16)pm1b_cnt, payload);
        for (volatile int d = 0; d < 200000; d++);
    }

    while (1) asm volatile("hlt");
}

const int icon_count = sizeof(icons) / sizeof(icons[0]);

int get_mm(KERNEL_MEMORY_MAP *kmap, MULTIBOOT_INFO *mboot_info) {
    uint32 i;

    if (kmap == NULL) return -1;
    kmap->kernel.k_start_addr = (uint32)&__kernel_section_start;
    kmap->kernel.k_end_addr = (uint32)&__kernel_section_end;
    kmap->kernel.k_len = ((uint32)&__kernel_section_end - (uint32)&__kernel_section_start);

    kmap->kernel.text_start_addr = (uint32)&__kernel_text_section_start;
    kmap->kernel.text_end_addr = (uint32)&__kernel_text_section_end;
    kmap->kernel.text_len = ((uint32)&__kernel_text_section_end - (uint32)&__kernel_text_section_start);

    kmap->kernel.data_start_addr = (uint32)&__kernel_data_section_start;
    kmap->kernel.data_end_addr = (uint32)&__kernel_data_section_end;
    kmap->kernel.data_len = ((uint32)&__kernel_data_section_end - (uint32)&__kernel_data_section_start);

    kmap->kernel.rodata_start_addr = (uint32)&__kernel_rodata_section_start;
    kmap->kernel.rodata_end_addr = (uint32)&__kernel_rodata_section_end;
    kmap->kernel.rodata_len = ((uint32)&__kernel_rodata_section_end - (uint32)&__kernel_rodata_section_start);

    kmap->kernel.bss_start_addr = (uint32)&__kernel_bss_section_start;
    kmap->kernel.bss_end_addr = (uint32)&__kernel_bss_section_end;
    kmap->kernel.bss_len = ((uint32)&__kernel_bss_section_end - (uint32)&__kernel_bss_section_start);

    kmap->system.total_memory = mboot_info->mem_low + mboot_info->mem_high;

    for (i = 0; i < mboot_info->mmap_length; i += sizeof(MULTIBOOT_MEMORY_MAP)) {
        MULTIBOOT_MEMORY_MAP *mmap = (MULTIBOOT_MEMORY_MAP *)(mboot_info->mmap_addr + i);
        if (mmap->type != MULTIBOOT_MEMORY_AVAILABLE) continue;
        if (mmap->addr_low == kmap->kernel.text_start_addr) {
            kmap->available.start_addr = kmap->kernel.k_end_addr + 1024 * 1024;
            kmap->available.end_addr = mmap->addr_low + mmap->len_low;
            kmap->available.size = kmap->available.end_addr - kmap->available.start_addr;
            return 0;
        }
    }

    return -1;
}

void report_mm(KERNEL_MEMORY_MAP *kmap) {
    printf("kernel:\n");
    printf("kernel-start: 0x%x, kernel-end: 0x%x, TOTAL: %d bytes\n",
           kmap->kernel.k_start_addr, kmap->kernel.k_end_addr, kmap->kernel.k_len);
    printf("text-start: 0x%x, text-end: 0x%x, TOTAL: %d bytes\n",
           kmap->kernel.text_start_addr, kmap->kernel.text_end_addr, kmap->kernel.text_len);
    printf("data-start: 0x%x, data-end: 0x%x, TOTAL: %d bytes\n",
           kmap->kernel.data_start_addr, kmap->kernel.data_end_addr, kmap->kernel.data_len);
    printf("rodata-start: 0x%x, rodata-end: 0x%x, TOTAL: %d\n",
           kmap->kernel.rodata_start_addr, kmap->kernel.rodata_end_addr, kmap->kernel.rodata_len);
    printf("bss-start: 0x%x, bss-end: 0x%x, TOTAL: %d\n",
           kmap->kernel.bss_start_addr, kmap->kernel.bss_end_addr, kmap->kernel.bss_len);

    printf("total_memory: %d KB\n", kmap->system.total_memory);
    printf("available:\n");
    printf("start_adddr: 0x%x\n end_addr: 0x%x\n size: %d\n",
           kmap->available.start_addr, kmap->available.end_addr, kmap->available.size);
}

BOOL is_cd(char *b) {
    if((b[0]=='c')&&(b[1]=='d'))
        if(b[2]==' '||b[2]=='\0')
            return TRUE;
    return FALSE;
}

BOOL is_cat(char *b) {
    if((b[0]=='c')&&(b[1]=='a')&&(b[2]=='t'))
        if(b[3]==' '||b[3]=='\0')
            return TRUE;
    return FALSE;
}

void wllp_rendcache(void) {
    int screen_width = vbe_get_width();
    int screen_height = vbe_get_height();
    
    if (g_wallpaper_cache_valid && g_wallpaper_cache) {
        uint32* dst = g_back_buffer;
        uint32* src = g_wallpaper_cache;
        
        int pixels = screen_width * screen_height;
        memcpy(dst, src, pixels * sizeof(uint32));
        return;
    }
    
    if (!g_wllp_data) {
        rect_grad(0, 0, screen_width, screen_height, 
                  RGB(222, 200, 219), RGB(227, 161, 218));
        return;
    }
    
    // this should not happen
    rect_grad(0, 0, screen_width, screen_height, RGB(222, 200, 219), RGB(227, 161, 218));
}

void wllp_asmcache(void) {
    int screen_width = vbe_get_width();
    int screen_height = vbe_get_height();
    
    if (g_wallpaper_cache) {
        kfree(g_wallpaper_cache);
        g_wallpaper_cache = NULL;
    }
    
    g_wallpaper_cache_valid = FALSE;
    
    int cache_size = screen_width * screen_height * sizeof(uint32);
    g_wallpaper_cache = (uint32*)kmalloc(cache_size);
    
    if (!g_wallpaper_cache) {
        kprint("ERROR: Failed to allocate wallpaper cache!\n");
        return;
    }
    
    g_wallpaper_cache_width = screen_width;
    g_wallpaper_cache_height = screen_height;
    
    if (g_wllp_data) {
        int dst_w = 1024;
        int dst_h = 788;
        int offset_y = -20;
        
        bool bottom_up = g_wllp_height > 0;
        int abs_src_h = bottom_up ? g_wllp_height : -g_wllp_height;
        
        int scale_x = (g_wllp_width << 16) / dst_w;
        int scale_y = (abs_src_h << 16) / dst_h;
        
        for (int dy = 0; dy < dst_h; dy++) {
            int screen_y = offset_y + dy;
            if (screen_y < 0 || screen_y >= screen_height) continue;
            
            int sy = (dy * scale_y) >> 16;
            if (sy >= abs_src_h) sy = abs_src_h - 1;
            
            int row_index = bottom_up ? (abs_src_h - 1 - sy) : sy;
            char* src_row = g_wllp_data + row_index * g_wallpaper_row_padded;
            
            uint32* cache_row = g_wallpaper_cache + screen_y * screen_width;
            
            for (int dx = 0; dx < dst_w && dx < screen_width; dx++) {
                int sx = (dx * scale_x) >> 16;
                if (sx >= g_wllp_width) sx = g_wllp_width - 1;
                
                uint8 b = src_row[sx * 3 + 0];
                uint8 g = src_row[sx * 3 + 1];
                uint8 r = src_row[sx * 3 + 2];
                
                cache_row[dx] = RGB(r, g, b);
            }
        }
    } else {
        uint32 color1 = RGB(222, 200, 219);
        uint32 color2 = RGB(227, 161, 218);
        
        uint8 r1 = (color1 >> 16) & 0xFF;
        uint8 g1 = (color1 >> 8) & 0xFF;
        uint8 b1 = color1 & 0xFF;
        
        uint8 r2 = (color2 >> 16) & 0xFF;
        uint8 g2 = (color2 >> 8) & 0xFF;
        uint8 b2 = color2 & 0xFF;
        
        for (int y = 0; y < screen_height; y++) {
            int blend = (y * 255) / screen_height;
            
            uint8 r = r1 + ((r2 - r1) * blend) / 255;
            uint8 g = g1 + ((g2 - g1) * blend) / 255;
            uint8 b = b1 + ((b2 - b1) * blend) / 255;
            
            uint32 color = RGB(r, g, b);
            uint32* row = g_wallpaper_cache + y * screen_width;
            
            for (int x = 0; x < screen_width; x++) {
                row[x] = color;
            }
        }
    }
    
    g_wallpaper_cache_valid = TRUE;
    kprint("built wallpaper cache: %dx%d\n", screen_width, screen_height);
}

typedef struct __attribute__((packed)) {
    char sig[8];
    uint8 checksum;
    char oem_id[6];
    uint8 revision;
    uint32 rsdt_addr;
} ACPI_RSDP;

typedef struct __attribute__((packed)) {
    char sig[4];
    uint32 length;
    uint8 rev;
    uint8 checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32 oem_rev;
    uint32 creator_id;
    uint32 creator_rev;
} ACPI_SDT_HEADER;

void acpi_init(void) {
    ACPI_RSDP* rsdp = NULL;
    for (uint32 addr = 0xE0000; addr < 0x100000; addr += 16) {
        if (!strncmp((char*)addr, "RSD PTR ", 8)) { rsdp = (ACPI_RSDP*)addr; break; }
    }
    if (!rsdp) return; // pm1A stay 0 for reset

    ACPI_SDT_HEADER* rsdt = (ACPI_SDT_HEADER*)rsdp->rsdt_addr;
    uint32* entries = (uint32*)((uint8*)rsdt + sizeof(ACPI_SDT_HEADER));
    int count = (rsdt->length - sizeof(ACPI_SDT_HEADER)) / 4;

    for (int i = 0; i < count; i++) {
        ACPI_SDT_HEADER* table = (ACPI_SDT_HEADER*)entries[i];
        if (!strncmp(table->sig, "FACP", 4)) {
            PM1A_CNT_BLK = *(uint32*)((uint8*)table + 64);
            PM1B_CNT_BLK = *(uint32*)((uint8*)table + 68);
            break;
        }
    }
}

void kmain(unsigned long magic __attribute__((unused)), unsigned long addr) {
    g_video_mode = VIDEO_MODE_TEXT;
    console_init(COLOR_BRIGHT_MAGENTA, COLOR_BLUE);
    serial_init();
    MULTIBOOT_INFO *mboot_info;
    gdt_init();
    idt_init();
    //register_interrupt_handler(IRQ_BASE + IRQ0_TIMER, timer_handler);
    printf("Biboo is preparing...\n");
    printf("please wait warmly :D\n");

    printf("*cue the obligatory ASCII art*\n");
    printf("\n\n\n\n");
    printf(
        " /$$           /$$$$$$      /$$$$$$            /$$         /$$\n"
        "| $$          /$$__  $$    /$$__  $$           | $$        |__/\n"
        "| $$   /$$   | $$  \\ $$  | $$  \\__/  /$$$$$$  | $$  /$$   /$$\n"
        "| $$  /$$/   | $$  | $$   |  $$$$$$  /$$__  $$  | $$ /$$/  | $$\n"
        "| $$$$$$/    | $$  | $$   \\____  $$ | $$$$$$$$  | $$$$$$/  | $$\n"
        "| $$_  $$    | $$  | $$   /$$  \\ $$| $$_____ |  | $$_$$|   | $$\n"
        "| $$ \\  $$  |  $$$$$$$/  | $$$$$$/ | $$$$$$$| $ | $ \\$$|  | $$\n"
        "|__/  \\__/   \\______/    \\______/  \\_______/ | $_/ \\__/|__/\n"
    );
    printf("\n\n");
    
    mboot_info = (MULTIBOOT_INFO *)addr;
    
    if ((mboot_info->flags & 0x8) && mboot_info->modules_count > 0) {
        MULTIBOOT_MODULE *mods = (MULTIBOOT_MODULE *)mboot_info->modules_addr;
        g_ramdisk_base = (uint8 *)mods[0].mod_start;
        g_ramdisk_size = mods[0].mod_end - mods[0].mod_start;
        g_using_ramdisk = 1;
    }

    if (get_mm(&g_kmap, mboot_info) < 0) return;
    pmm_init(g_kmap.available.start_addr, g_kmap.available.size);
    pmm_init_region(g_kmap.available.start_addr, g_kmap.available.size);

    acpi_init();

    if (g_using_ramdisk) {
        uint32 base = (uint32)g_ramdisk_base;
        uint32 start = base & ~(PMM_BLOCK_SIZE - 1);
        uint32 end = (base + g_ramdisk_size + PMM_BLOCK_SIZE - 1) & ~(PMM_BLOCK_SIZE - 1);

        pmm_deinit_region(start, end - start);

        printf("[multiboot] 0x%x - 0x%x (%u bytes), reserved 0x%x - 0x%x from PMM\n",
            base, base + g_ramdisk_size, g_ramdisk_size, start, end);
        kprint("[multiboot] 0x%x - 0x%x (%u bytes), reserved 0x%x - 0x%x from PMM\n",
            base, base + g_ramdisk_size, g_ramdisk_size, start, end);
    }

    uint32_t free_blocks = pmm_get_max_blocks() - pmm_get_used_blocks();

    uint32_t heap_blocks = (free_blocks > 512) ? (free_blocks - 512) : (free_blocks / 2);
    void *start = pmm_alloc_blocks(heap_blocks);
    
    if (start == NULL) {
        printf("!CRITICAL! Failed to allocate memory for heap!\n");
        printf("available: %d, free blocks: %d\n", g_kmap.available.size, free_blocks);
        kprint("!CRITICAL! Failed to allocate memory for heap!\n");
        kprint("available: %d, free blocks: %d\n", g_kmap.available.size, free_blocks);
        for(;;);
    }

    void *end = (void*)((uint32)start + (heap_blocks * PMM_BLOCK_SIZE));
    k_init(start, end);
    init_procsys();
    
    timer_init(67);
    sti();

    splash("preparing ur storage");

    ata_init();

    // check ATA/ATAPI (i beeped up the img loading, you can use it to check the found drives and their models)
    // char ide_summary[512];
    // ide_get_summary(ide_summary, sizeof(ide_summary));
    // splash(ide_summary);
    // while (kb_getchar_nb() == 0) { press any key }

    // point IDE to module e.g. GRUB cfg arg, fallback is ata_init
    if ((mboot_info->flags & 0x8) && mboot_info->modules_count > 0) {
        MULTIBOOT_MODULE *mods = (MULTIBOOT_MODULE *)mboot_info->modules_addr;
        g_ramdisk_base = (uint8 *)mods[0].mod_start;
        g_ramdisk_size = mods[0].mod_end - mods[0].mod_start;
        g_using_ramdisk = 1;
        kprint("[ATA] module found: 0x%x - 0x%x (%u bytes), using as ramdisk\n", mods[0].mod_start, mods[0].mod_end, g_ramdisk_size);
    }

    printf("\n\n\n\n\n\n\n\n\n");
    fat_init();
    printf("Scanning storage left...\n");
    uint64 free_space = fat_get_free_space();
    printf("REMAINING SPACE: %llu bytes\n", free_space);
    printf("--------------|> %llu MB\n", free_space / (1024 * 1024));
    printf("available blocks: %d\n", free_blocks);
    
    //delay(1000);
    printf("ROCK IN\n");
    
    init_gui();

    pci_init();
    
    extern void tester();
    tester();
    ac97_play("/SYSTEM/startup.wav");

    keyboard_init();
    mouse_init();

    net_send_arp_req(htonl((10 << 24) | (0 << 16) | (2 << 8) | 2));
    net_ping(htonl((10 << 24) | (0 << 16) | (2 << 8) | 2));

    for (;;) {
        unsigned char c = (unsigned char)kb_getchar_nb();
        if (c != 0) {
            //kprint("pressed %c (0x%x)\n", c, c);
            Window* active_window = get_active_win();
            if (active_window) {
                //kprint("%s in focus\n", active_window->title);
                if (active_window->on_key_press) {
                    active_window->on_key_press(active_window, c);
                    is_dirty(TRUE);
                } else {
                    Process* p = get_process(active_window->pid);
                    if (p) kprint(" Process name: %s\n", p->name);
                    if (p && p->name && strcmp(p->name, "pbsh") == 0) {
                        if (c == '`' || c == '|') {
                            pbsh_handle_scroll(active_window, c == '`' ? -5 : 5);
                            is_dirty(TRUE);
                            continue;
                        }
                        char* pbsh_buf = p->buf;
                        int* pbsh_len = &p->curr_buf_len;

                        if (c == '\b') {
                            char* last_prompt = NULL;
                            char* search = pbsh_buf;
                            while (1) {
                                char* found = strstr(search, PROMPT_TEXT);
                                if (!found) break;
                                last_prompt = found;
                                search = found + 1;
                            }
                            
                            int min_len = last_prompt ? (last_prompt - pbsh_buf) + strlen(PROMPT_TEXT) : 0;
                            
                            if (!last_prompt && *pbsh_len > 0) {
                                min_len = *pbsh_len; 
                            }

                            if (*pbsh_len > min_len && *pbsh_len > 0) {
                                (*pbsh_len)--;
                                pbsh_buf[*pbsh_len] = '\0';
                            }
                        } else if (c == '\n') {
                            char* last_prompt = NULL;
                            char* search = pbsh_buf;
                            while (1) {
                                char* found = strstr(search, PROMPT_TEXT);
                                if (!found) break;
                                last_prompt = found;
                                search = found + 1;
                            }
                            
                            int min_len = last_prompt ? (last_prompt - pbsh_buf) + strlen(PROMPT_TEXT) : 0;
                            char *command_start = pbsh_buf + min_len;
                            pbsh_buf_print(pbsh_buf, pbsh_len, "\n");
                            cmd(command_start, pbsh_buf, pbsh_len);

                        } else if (c >= 32 && c < 127) {
                            if (*pbsh_len < MAX_SHBUF_SIZE -1) {
                                pbsh_buf[*pbsh_len] = c;
                                (*pbsh_len)++;
                                pbsh_buf[*pbsh_len] = '\0';
                            }
                        }
                        p->is_scrolled = FALSE;
                        is_dirty(TRUE);
                    }
                }
            } else {
                kprint(" No active window.\n");
            }
        }

        if (_kb_reboot()) reboot();
        if (_kb_shutdown()) shutdown(PM1A_CNT_BLK, PM1B_CNT_BLK);

        if (g_video_mode == VIDEO_MODE_GRAPHICS) {
            m_update();
            redraw_all();
        }

        ac97_update_streams();
        net_poll();
        reaper_check_redirect();
    }
}

void render_icons(void) {
    const int icon_width = 34;
    const int icon_height = 34;
    const int start_x = 2;
    const int start_y = 1;
    const int spacing = 4;

    for (int i = 0; i < icon_count; i++) {
        int x = start_x + i * (icon_width + spacing);
        int y = start_y;
        icon(x, y, icon_width, icon_height, icons[i].bmp, icons[i].action);

        if (i == icon_count / 2) {
            splash("loading system assets..."); //after this everything is done so I am keeping this msg generic for now
        }
    }
}

void set_wallpaper(const char* filename) {
    static char new_path[256];
    if (filename) {
        strcpy(new_path, "SYSTEM/");
        strcat(new_path, filename);
        
        if (g_wallpaper_bmp_base_data) {
            kfree(g_wallpaper_bmp_base_data);
            g_wallpaper_bmp_base_data = NULL;
            g_wllp_data = NULL;
        }

        g_wallpaper_path = new_path;

        g_wallpaper_bmp_base_data = fat_read_file(g_wallpaper_path);
        if (g_wallpaper_bmp_base_data) {
            BMP_FILE_H *fh = (BMP_FILE_H *)g_wallpaper_bmp_base_data;
            BMP_INFO_H *ih = (BMP_INFO_H *)(g_wallpaper_bmp_base_data + sizeof(*fh));

            if (fh->type == 0x4D42 && ih->bpp == 24 && ih->width > 0 && ih->height > 0) {
                g_wllp_width = ih->width;
                g_wllp_height = ih->height;
                g_wllp_data = g_wallpaper_bmp_base_data + fh->offset;
                g_wallpaper_row_padded = (g_wllp_width * 3 + 3) & ~3;
                
                wllp_asmcache();
            } else {
                kfree(g_wallpaper_bmp_base_data);
                g_wallpaper_bmp_base_data = NULL;
                g_wllp_data = NULL;
            }
        }
        
        is_bg_dirty(TRUE);
    }
}

void sys_cleanup() {
    gui_cleanup();
    vesa_cleanup();

    if (g_wallpaper_bmp_base_data) {
        kfree(g_wallpaper_bmp_base_data);
        g_wallpaper_bmp_base_data = NULL;
        g_wllp_data = NULL;
        g_wllp_width = 0;
        g_wllp_height = 0;
        g_wallpaper_row_padded = 0;
    }
    
    if (g_wallpaper_cache) {
        kfree(g_wallpaper_cache);
        g_wallpaper_cache = NULL;
        g_wallpaper_cache_valid = FALSE;
    }

    kprint("CLEANUP DONE\n");
}

void init_gui() {
    g_video_mode = VIDEO_MODE_GRAPHICS;
    vesa_init(SCREEN_WIDTH, SCREEN_HEIGHT, 32);
    
    splash("preparing desktop...");

    g_init();

    if (g_wallpaper_bmp_base_data == NULL && g_wallpaper_path != NULL) {
        g_wallpaper_bmp_base_data = fat_read_file(g_wallpaper_path);
        if (g_wallpaper_bmp_base_data) {
            BMP_FILE_H *fh = (BMP_FILE_H *)g_wallpaper_bmp_base_data;
            BMP_INFO_H *ih = (BMP_INFO_H *)(g_wallpaper_bmp_base_data + sizeof(*fh));

            if (fh->type == 0x4D42 && ih->bpp == 24 && ih->width > 0 && ih->height > 0) {
                g_wllp_width = ih->width;
                g_wllp_height = ih->height;
                g_wllp_data = g_wallpaper_bmp_base_data + fh->offset;
                g_wallpaper_row_padded = (g_wllp_width * 3 + 3) & ~3;
            } else {
                kfree(g_wallpaper_bmp_base_data);
                g_wallpaper_bmp_base_data = NULL;
                g_wllp_data = NULL;
            }
        }
    }

    wllp_asmcache();

    desktop();
    render_icons();
    swapbuf();
}