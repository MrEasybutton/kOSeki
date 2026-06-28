#include "net_mqtt.h"
#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"
#include "serial.h"
#include "string.h"
#include "kheap.h"
#include "utils.h"

static mqtt_client_t *static_client = NULL;
static mqtt_status_handler_t g_status_handler = NULL;

typedef struct {
    char topic[128];
    mqtt_message_handler_t handler;
} mqtt_subs;

#define MAX_SUBSCRIPTIONS 16
static mqtt_subs g_subscriptions[MAX_SUBSCRIPTIONS];
static int g_sub_count = 0;

static char g_broker_host[128];
static uint16 g_broker_port = 1883;
static char g_client_id[64];
static char g_current_incoming_topic[128];

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    (void)client; (void)arg;
    if (status == MQTT_CONNECT_ACCEPTED) {
        kprint("[MQTT] Connected successfully to %s\n", g_broker_host);
        if (g_status_handler) g_status_handler(MQTT_CONNECTED);
    } else {
        kprint("[MQTT] Connection failed (status %d)\n", status);
        if (g_status_handler) g_status_handler(MQTT_ERROR);
    }
}

static void mqtt_incom_publish_cb(void *arg, const char *topic, u32_t tot_len) {
    (void)arg;

    strncpy(g_current_incoming_topic, topic, sizeof(g_current_incoming_topic)-1);
    g_current_incoming_topic[sizeof(g_current_incoming_topic)-1] = '\0';
    kprint("[MQTT] Incoming publish (topic %s (%d bytes payload))\n", g_current_incoming_topic, tot_len);
}

static BOOL mqtt_topic_match(const char* sub, const char* incoming) {
    while (*sub && *incoming) {
        if (*sub == '#') return TRUE;
        if (*sub == '+') {
            while (*incoming && *incoming != '/') incoming++;
            sub++;
            continue;
        }
        if (*sub != *incoming) return FALSE;
        sub++; incoming++;
    }
    return (*sub == *incoming) || (*sub == '#' && *(sub-1) == '/');
}

static void mqtt_incom_data_call(void *arg, const u8_t *data, u16_t len, u8_t flags) {
    (void)arg; (void)flags;
    for (int i = 0; i < g_sub_count; i++) {
        if (mqtt_topic_match(g_subscriptions[i].topic, g_current_incoming_topic)) {
            if (g_subscriptions[i].handler) {
                g_subscriptions[i].handler(g_current_incoming_topic, data, len);
            }
        }
    }
}

static void dns_call(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    (void)callback_arg;
    if (ipaddr) {
        kprint("[MQTT/DNS] %s resolved to %u.%u.%u.%u\n", name,
                      ip4_addr1(ipaddr), ip4_addr2(ipaddr), 
                      ip4_addr3(ipaddr), ip4_addr4(ipaddr));
        
        struct mqtt_connect_client_info_t ci;
        memset(&ci, 0, sizeof(ci));
        ci.client_id = g_client_id;
        ci.keep_alive = 60;
        
        kprint("[MQTT] Connecting as client '%s'...\n", g_client_id);
        cli();
        mqtt_client_connect(static_client, ipaddr, g_broker_port, mqtt_connection_cb, NULL, &ci);
        sti();
    } else {
        kprint("[MQTT/DNS] Failed to resolve %s\n", name);
        if (g_status_handler) g_status_handler(MQTT_ERROR);
    }
}

void net_mqtt_init(const char* host, uint16 port, const char* client_id) {
    if (!static_client) {
        static_client = mqtt_client_new();
    }
    
    strncpy(g_broker_host, host, sizeof(g_broker_host)-1);
    g_broker_port = port;
    
    if (client_id) {
        strncpy(g_client_id, client_id, sizeof(g_client_id)-1);
    } else {
        // ID
        #include "cmos.h"
        extern uint32 timer_ticks;
        time_t t;
        get_time(&t);

        strcpy(g_client_id, "koseki_");
        char tmp[16];
        itoa(tmp, 'd', t.hour); strcat(g_client_id, tmp);
        itoa(tmp, 'd', t.minute); strcat(g_client_id, tmp);
        itoa(tmp, 'd', t.second); strcat(g_client_id, tmp);
        itoa(tmp, 'd', timer_ticks % 1000); strcat(g_client_id, tmp);
    }
    
    mqtt_set_inpub_callback(static_client, mqtt_incom_publish_cb, mqtt_incom_data_call, NULL);
    
    ip_addr_t addr;
    cli();
    err_t err = dns_gethostbyname(host, &addr, dns_call, NULL);
    sti();
    if (err == ERR_OK) {
        dns_call(host, &addr, NULL);
    } else if (err != ERR_INPROGRESS) {
        kprint("[MQTT] DNS error %d\n", err);
        if (g_status_handler) g_status_handler(MQTT_ERROR);
    }
}

void net_mqtt_set_handlestat(mqtt_status_handler_t handler) {
    g_status_handler = handler;
}

static void mqtt_sub_request_cb(void *arg, err_t result) {
    (void)arg;
    if (result == ERR_OK) {
        kprint("[MQTT] Subscription successful\n");
    } else {
        kprint("[MQTT] Subscription failed (%d)\n", result);
    }
}

void net_mqtt_subscribe(const char* topic, mqtt_message_handler_t handler) {
    if (g_sub_count >= MAX_SUBSCRIPTIONS) return;
    
    strncpy(g_subscriptions[g_sub_count].topic, topic, sizeof(g_subscriptions[g_sub_count].topic)-1);
    g_subscriptions[g_sub_count].handler = handler;
    g_sub_count++;
    
    if (net_mqtt_is_connected()) {
        cli();
        mqtt_subscribe(static_client, topic, 0, mqtt_sub_request_cb, NULL);
        sti();
    }
}

void net_mqtt_publish(const char* topic, const uint8* payload, uint16 payload_len) {
    if (!net_mqtt_is_connected()) return;
    
    kprint("[MQTT] Publishing to %s: %s\n", topic, (const char*)payload);
    cli();
    mqtt_publish(static_client, topic, payload, payload_len, 0, 0, NULL, NULL);
    sti();
}

void net_mqtt_disconnect() {
    if (static_client && mqtt_client_is_connected(static_client)) {
        cli();
        mqtt_disconnect(static_client);
        sti();
        kprint("[MQTT] Disconnected from broker\n");
    }
}

BOOL net_mqtt_is_connected() {
    return static_client && mqtt_client_is_connected(static_client);
}

void tester_mqtt(const char* topic, const uint8* payload, uint16 payload_len) {
    char msg[128];
    uint16 len = payload_len < 127 ? payload_len : 127;
    memcpy(msg, payload, len);
    msg[len] = '\0';
    kprint("[MQTT] TEST RECV [%s]: %s\n", topic, msg);
}