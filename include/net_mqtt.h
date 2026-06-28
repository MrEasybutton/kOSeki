#ifndef NET_MQTT_H
#define NET_MQTT_H

#include "types.h"

typedef void (*mqtt_message_handler_t)(const char* topic, const uint8* payload, uint16 payload_len);

typedef enum {
    MQTT_CONNECTED,
    MQTT_DISCONNECTED,
    MQTT_ERROR
} mqtt_status_t;

typedef void (*mqtt_status_handler_t)(mqtt_status_t status);


void net_mqtt_init(const char* host, uint16 port, const char* client_id);

void net_mqtt_set_handlestat(mqtt_status_handler_t handler);

void net_mqtt_subscribe(const char* topic, mqtt_message_handler_t handler);
void net_mqtt_publish(const char* topic, const uint8* payload, uint16 payload_len);

void net_mqtt_disconnect();
BOOL net_mqtt_is_connected();

#endif
