#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include "serial.h"
#include "string.h"
#include "kheap.h"

typedef struct {
    char* buf;
    int* term_len;
    char* host;
} fetch_context_t;

extern void pbsh_buf_print(char* buf, int* term_len, const char* str);

static err_t fetch_recv_call(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    fetch_context_t* ctx = (fetch_context_t*)arg;
    if (p == NULL) {
        tcp_close(tpcb);
        kfree(ctx->host);
        kfree(ctx);
        return ERR_OK;
    }
    
    char* data = (char*)kmalloc(p->tot_len + 1);
    if (data) {
        pbuf_copy_partial(p, data, p->tot_len, 0);
        data[p->tot_len] = '\0';
        
        kprint("FETCH (%d bytes)\n", p->tot_len);
        
        // print to Pebblshell
        if (ctx && ctx->buf) {
            pbsh_buf_print(ctx->buf, ctx->term_len, data);
        }
        
        kfree(data);
    }
    
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static err_t fetch_conn_call(void *arg, struct tcp_pcb *tpcb, err_t err) {
    fetch_context_t* ctx = (fetch_context_t*)arg;
    kprint("Connected to %s!\n", ctx->host);
    
    char request[256];
    snprintf(request, sizeof(request), 
             "GET / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", 
             ctx->host);
             
    tcp_write(tpcb, request, strlen(request), TCP_WRITE_FLAG_COPY);
    tcp_recv(tpcb, fetch_recv_call);
    return ERR_OK;
}

static void dns_call(const char *name, const ip_addr_t *ipaddr, void *cbarg) {
    fetch_context_t* ctx = (fetch_context_t*)cbarg;
    
    if (ipaddr) {
        kprint("[DNS] %s resolved to %u.%u.%u.%u\n", name,
                      ip4_addr1(ipaddr), ip4_addr2(ipaddr), 
                      ip4_addr3(ipaddr), ip4_addr4(ipaddr));
        
        struct tcp_pcb *pcb = tcp_new();
        tcp_arg(pcb, ctx);
        tcp_connect(pcb, ipaddr, 80, fetch_conn_call);
    } else {
        kprint("[DNS] Failed to resolve %s\n", name);
        if (ctx && ctx->buf) {
            pbsh_buf_print(ctx->buf, ctx->term_len, "\nError: DNS resolution failed.\n");
        }
        kfree(ctx->host);
        kfree(ctx);
    }
}

void net_fetch(const char* host, char* buf, int* term_len) {
    fetch_context_t* ctx = (fetch_context_t*)kmalloc(sizeof(fetch_context_t));
    ctx->buf = buf;
    ctx->term_len = term_len;
    ctx->host = (char*)kmalloc(strlen(host) + 1);
    strcpy(ctx->host, host);

    kprint("FETCH: Resolving %s...\n", host);
    if (buf) {
        pbsh_buf_print(buf, term_len, "\nResolving ");
        pbsh_buf_print(buf, term_len, host);
        pbsh_buf_print(buf, term_len, "...\n");
    }

    ip_addr_t addr;
    err_t err = dns_gethostbyname(host, &addr, dns_call, ctx);
    
    if (err == ERR_OK) {
        dns_call(host, &addr, ctx);
    } else if (err != ERR_INPROGRESS) {
        kprint("FETCH: DNS error %d\n", err);
        pbsh_buf_print(buf, term_len, "DNS Error.\n");
        kfree(ctx->host);
        kfree(ctx);
    }
}

void tester() { net_fetch("10.0.2.2", NULL, NULL); }
