/**
 * @file dns_server.h
 * @brief DNS server for captive portal functionality
 * 
 * Implements a simple DNS server that responds to all DNS queries
 * with the ESP32 AP's IP address, enabling captive portal detection.
 * 
 * Based on: wifi-captive-portal-esp-idf-component
 * https://github.com/defcronyke/wifi-captive-portal-esp-idf
 */

#ifndef DNS_SERVER_H
#define DNS_SERVER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** DNS packet buffer size */
#define DNS_SERVER_MAX_LEN 512

/** DNS flags */
#define DNS_FLAG_QR (1 << 15)
#define DNS_FLAG_TC (1 << 9)

/** DNS query types */
#define DNS_QTYPE_A 1
#define DNS_QTYPE_NS 2
#define DNS_QTYPE_URI 256

/** DNS query classes */
#define DNS_QCLASS_IN 1
#define DNS_QCLASS_URI 256

/**
 * @brief DNS header structure
 */
typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_header_t;

/**
 * @brief DNS question footer structure
 */
typedef struct __attribute__((packed)) {
    uint16_t type;
    uint16_t cl;
} dns_question_footer_t;

/**
 * @brief DNS resource footer structure
 */
typedef struct __attribute__((packed)) {
    uint16_t type;
    uint16_t cl;
    uint32_t ttl;
    uint16_t rdlength;
} dns_resource_footer_t;

/**
 * @brief DNS URI header structure
 */
typedef struct __attribute__((packed)) {
    uint16_t prio;
    uint16_t weight;
} dns_uri_header_t;

/**
 * @brief Initialize and start DNS server
 * 
 * Creates a task that listens on port 53 and responds to all DNS
 * queries with the AP's IP address to enable captive portal detection.
 */
void dns_server_init(void);

/**
 * @brief Stop DNS server
 */
void dns_server_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // DNS_SERVER_H
