#include "net.h"
#include "e1000.h"
#include "utils.h"
#include "string.h"
#include "serial.h"
#include "kheap.h"
#include "gui.h"

#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/timeouts.h"
#include "lwip/udp.h"

extern struct netif k_netif;
extern void k_lwip_init(uint8 *mac);
extern void k_lwip_input(struct netif *netif, uint8 *data, uint16 len);

typedef struct udp_binding {
    uint16 port;
    udp_handler_t handler;
    struct udp_binding* next;
} udp_binding_t;

static udp_binding_t* g_udp_bindings = NULL;

static void udp_recv_call(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    (void)arg;
    if (p != NULL) {
        uint16 dest_port = pcb->local_port;
        
        //find handler
        udp_binding_t* curr = g_udp_bindings;
        while (curr) {
            if (curr->port == dest_port) {
                uint8* payload = (uint8*)kmalloc(p->tot_len);
                if (payload) {
                    pbuf_copy_partial(p, payload, p->tot_len, 0);
                    curr->handler(ip4_addr_get_u32(addr), port, payload, p->tot_len);
                    kfree(payload);
                }
                break;
            }
            curr = curr->next;
        }
        
        pbuf_free(p);
    }
}

static void netnotify(uint32 src_ip, uint16 src_port, uint8* payload, uint16 payload_len) {
    (void)src_ip; (void)src_port;
    char* msg = (char*)kmalloc(payload_len + 1);
    if (msg) {
        memcpy(msg, payload, payload_len);
        msg[payload_len] = '\0';
        kprint("UDP NOTIFY: %s\n", msg);
        notif_handler("UDP Message", msg);
        kfree(msg);
    }
}

void net_init(uint8* mac) {
    k_lwip_init(mac);
    
    net_register_udp_handler(1234, netnotify);
}

#define PACKET_QUEUE_SIZE 32

typedef struct {
    uint8 *data;
    uint16 length;
} queued_packet_t;

static queued_packet_t g_packet_queue[PACKET_QUEUE_SIZE];
static int g_queue_head = 0;
static int g_queue_tail = 0;

void net_receive(uint8* data, uint16 length) {
    // copy packet and queue
    int next_head = (g_queue_head + 1) % PACKET_QUEUE_SIZE;
    if (next_head == g_queue_tail) {
        kprint("[NET] queue is full, dropping packet\n");
        return;
    }

    uint8 *buffer = (uint8*)kmalloc(length);
    if (!buffer) return;
    memcpy(buffer, data, length);
    
    g_packet_queue[g_queue_head].data = buffer;
    g_packet_queue[g_queue_head].length = length;
    g_queue_head = next_head;
}

void net_poll() {
    while (g_queue_tail != g_queue_head) {
        queued_packet_t *p = &g_packet_queue[g_queue_tail];
        
        cli(); //shield lwIP entry
        k_lwip_input(&k_netif, p->data, p->length);
        sti();
        
        kfree(p->data);
        g_queue_tail = (g_queue_tail + 1) % PACKET_QUEUE_SIZE;
    }
    
    cli();
    sys_check_timeouts();
    sti();
}

void net_send_packet(uint8* dest_mac, uint16 eth_type, uint8* payload, uint16 payload_len) {
    (void)dest_mac; (void)eth_type; (void)payload; (void)payload_len;
}

void net_send_arp_req(uint32 target_ip) {
    (void)target_ip;
}

void net_ping(uint32 target_ip) {
    (void)target_ip;
}

void net_send_ip(uint32 dest_ip, uint8 proto, uint8* payload, uint16 payload_len) {
    (void)dest_ip; (void)proto; (void)payload; (void)payload_len;
}

void net_send_udp(uint32 dest_ip, uint16 src_port, uint16 dest_port, uint8* payload, uint16 payload_len) {
    struct udp_pcb *pcb = udp_new();
    if (!pcb) return;

    ip_addr_t dest_addr;
    ip4_addr_set_u32(&dest_addr, dest_ip);

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, payload_len, PBUF_RAM);
    if (p) {
        pbuf_take(p, payload, payload_len);
        udp_sendto_if(pcb, p, &dest_addr, dest_port, &k_netif);
        pbuf_free(p);
    }
    udp_remove(pcb);
}

void net_register_udp_handler(uint16 port, udp_handler_t handler) {
    udp_binding_t* curr = g_udp_bindings;
    while (curr) {
        if (curr->port == port) {
            curr->handler = handler;
            return;
        }
        curr = curr->next;
    }

    //new handler
    udp_binding_t* binding = (udp_binding_t*)kmalloc(sizeof(udp_binding_t));
    if (!binding) return;

    binding->port = port;
    binding->handler = handler;
    binding->next = g_udp_bindings;
    g_udp_bindings = binding;

    // create pcb for the port
    struct udp_pcb *pcb = udp_new();
    if (pcb) {
        err_t err = udp_bind(pcb, IP_ADDR_ANY, port);
        if (err == ERR_OK) {
            udp_recv(pcb, udp_recv_call, NULL);
            kprint("[UDP] registered handler for port %d\n", port);
        } else {
            udp_remove(pcb);
            kprint("[UDP] Failed to bind port %d\n", port);
        }
    }
}