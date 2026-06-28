#include "lwip/netif.h"
#include "lwip/etharp.h"
#include "lwip/init.h"
#include "lwip/dns.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#include "e1000.h"
#include "string.h"
#include "kheap.h"
#include "serial.h"
#include "utils.h"

extern uint32 timer_ticks;

u32_t sys_now(void) {
    // 1000/67
    return timer_ticks * 15; 
}

sys_prot_t sys_arch_protect(void) {
    cli();
    return 0;
}

void sys_arch_unprotect(sys_prot_t pval) {
    (void)pval;
    sti();
}

void free(void *ptr) {
    kfree(ptr);
}

void *calloc(size_t count, size_t size) {
    void *ptr = kmalloc(count * size);
    if (ptr) memset(ptr, 0, count * size);
    return ptr;
}

int atoi(const char *nptr) {
    return strtoi((char*)nptr);
}

void k_lwip_input(struct netif *netif, uint8 *data, uint16 len);

static err_t k_netif_output(struct netif *netif, struct pbuf *p) {
    (void)netif;
    // calld by lwIP to send ethernet frame
    uint8 *buffer = (uint8*)kmalloc(p->tot_len);
    if (!buffer) return ERR_MEM;

    pbuf_copy_partial(p, buffer, p->tot_len, 0);
    e1000_send_packet(buffer, p->tot_len);
    
    return ERR_OK;
}

static err_t k_netif_init(struct netif *netif) {
    netif->name[0] = 'k';
    netif->name[1] = 'o';
    netif->output = etharp_output;
    netif->linkoutput = k_netif_output;
    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

    // MAC address is alr in netif->hwaddr
    return ERR_OK;
}

struct netif k_netif;

void k_lwip_init(uint8 *mac) {
    lwip_init();

    ip4_addr_t ip, mask, gw, dns;
    // hardcoded
    IP4_ADDR(&ip, 10, 0, 2, 15);
    IP4_ADDR(&mask, 255, 255, 255, 0);
    IP4_ADDR(&gw, 10, 0, 2, 2);
    IP4_ADDR(&dns, 10, 0, 2, 3);

    memcpy(k_netif.hwaddr, mac, 6);
    k_netif.hwaddr_len = 6;

    netif_add(&k_netif, &ip, &mask, &gw, NULL, k_netif_init, ethernet_input);
    netif_set_default(&k_netif);
    netif_set_up(&k_netif);
    
    dns_setserver(0, &dns);
    
    kprint("lwIP initialized.\n");
}

void k_lwip_input(struct netif *netif, uint8 *data, uint16 len) {
    struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (p) {
        pbuf_take(p, data, len);
        if (netif->input(p, netif) != ERR_OK) {
            pbuf_free(p);
        }
    }
}