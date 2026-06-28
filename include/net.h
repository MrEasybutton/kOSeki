#ifndef NET_H
#define NET_H

#include "types.h"

struct eth_header {
    uint8 dest[6];
    uint8 src[6];
    uint16 type;
} __attribute__((packed));

#define ETH_TYPE_IPV4 0x0800
#define ETH_TYPE_ARP  0x0806

struct arp_packet {
    uint16 hw_type;
    uint16 proto_type;
    uint8 hw_addr_len;
    uint8 proto_addr_len;
    uint16 operation;
    uint8 src_mac[6];
    uint32 src_ip;
    uint8 dest_mac[6];
    uint32 dest_ip;
} __attribute__((packed));

#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2

struct ip_header {
    uint8 version_ihl;
    uint8 tos;
    uint16 len;
    uint16 id;
    uint16 flags_frag;
    uint8 ttl;
    uint8 proto;
    uint16 checksum;
    uint32 src;
    uint32 dest;
} __attribute__((packed));

#define IP_PROTO_ICMP 1
#define IP_PROTO_UDP  17
#define IP_PROTO_TCP  6

struct icmp_header {
    uint8 type;
    uint8 code;
    uint16 checksum;
    uint16 id;
    uint16 seq;
} __attribute__((packed));

#define ICMP_TYPE_ECHO_REPLY   0
#define ICMP_TYPE_ECHO_REQUEST 8

struct udp_header {
    uint16 src_port;
    uint16 dest_port;
    uint16 len;
    uint16 checksum;
} __attribute__((packed));

typedef struct {
    uint8 mac[6];
    uint32 ip;
    uint32 gateway;
    uint32 mask;
} net_interface_t;

extern net_interface_t g_net_if;

typedef void (*udp_handler_t)(uint32 src_ip, uint16 src_port, uint8* payload, uint16 payload_len);

void net_init(uint8* mac);
void net_receive(uint8* data, uint16 length);
void net_send_packet(uint8* dest_mac, uint16 eth_type, uint8* payload, uint16 payload_len);
void net_send_ip(uint32 dest_ip, uint8 proto, uint8* payload, uint16 payload_len);
void net_send_arp_req(uint32 target_ip);
void net_ping(uint32 target_ip);
void net_send_udp(uint32 dest_ip, uint16 src_port, uint16 dest_port, uint8* payload, uint16 payload_len);
void net_register_udp_handler(uint16 port, udp_handler_t handler);
void net_poll();

#endif
