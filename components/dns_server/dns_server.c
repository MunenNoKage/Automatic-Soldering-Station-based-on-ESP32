/**
 * @file dns_server.c
 * @brief DNS server implementation for captive portal
 * 
 * Implements a simple DNS server that responds to all queries with
 * the ESP32 AP's IP address, enabling captive portal detection on
 * mobile devices and redirecting all traffic to the web interface.
 * 
 * Based on: wifi-captive-portal-esp-idf-component
 * https://github.com/defcronyke/wifi-captive-portal-esp-idf
 */

#include "dns_server.h"
#include <string.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "lwip/sockets.h"
#include "lwip/err.h"

static const char *TAG = "DNS_SERVER";

static int sock_fd = -1;
static TaskHandle_t dns_task_handle = NULL;

/**
 * @brief Put unaligned 16-bit network value
 */
static void setn16(void *pp, int16_t n) {
    char *p = pp;
    *p++ = (n >> 8);
    *p++ = (n & 0xff);
}

/**
 * @brief Put unaligned 32-bit network value
 */
static void setn32(void *pp, int32_t n) {
    char *p = pp;
    *p++ = (n >> 24) & 0xff;
    *p++ = (n >> 16) & 0xff;
    *p++ = (n >> 8) & 0xff;
    *p++ = (n & 0xff);
}

/**
 * @brief Convert network byte order to host
 */
static uint16_t my_ntohs(uint16_t *in) {
    char *p = (char *)in;
    return ((p[0] << 8) & 0xff00) | (p[1] & 0xff);
}

/**
 * @brief Parse DNS label into dotted string
 * 
 * @param packet Full DNS packet
 * @param labelPtr Pointer to label in packet
 * @param packetSz Total packet size
 * @param res Result buffer
 * @param resMaxLen Result buffer size
 * @return Pointer to next field in packet
 */
static char *label_to_str(char *packet, char *labelPtr, int packetSz, 
                          char *res, int resMaxLen) {
    int i, j, k;
    char *endPtr = NULL;
    i = 0;
    
    do {
        if ((*labelPtr & 0xC0) == 0) {
            // Regular label
            j = *labelPtr++;
            // Add separator period if not first label
            if (i < resMaxLen && i != 0) {
                res[i++] = '.';
            }
            // Copy label to result
            for (k = 0; k < j; k++) {
                if ((labelPtr - packet) > packetSz) {
                    return NULL;
                }
                if (i < resMaxLen) {
                    res[i++] = *labelPtr++;
                }
            }
        } else if ((*labelPtr & 0xC0) == 0xC0) {
            // Compressed label pointer
            endPtr = labelPtr + 2;
            int offset = my_ntohs(((uint16_t *)labelPtr)) & 0x3FFF;
            if (offset > packetSz) {
                return NULL;
            }
            labelPtr = &packet[offset];
        }
        
        if ((labelPtr - packet) > packetSz) {
            return NULL;
        }
    } while (*labelPtr != 0);
    
    res[i] = 0; // Zero-terminate
    if (endPtr == NULL) {
        endPtr = labelPtr + 1;
    }
    return endPtr;
}

/**
 * @brief Convert dotted hostname to DNS label format
 */
static char *str_to_label(char *str, char *label, int maxLen) {
    char *len = label;
    char *p = label + 1;
    
    while (1) {
        if (*str == '.' || *str == 0) {
            *len = ((p - len) - 1);
            len = p;
            p++;
            if (*str == 0) {
                break;
            }
            str++;
        } else {
            *p++ = *str++;
        }
    }
    *len = 0;
    return p;
}

/**
 * @brief Process DNS packet and send response
 */
static void dns_recv(struct sockaddr_in *premote_addr, char *pusrdata, 
                     unsigned short length) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"

    char buff[DNS_SERVER_MAX_LEN];
    char reply[DNS_SERVER_MAX_LEN];
    int i;
    char *rend = &reply[length];
    char *p = pusrdata;
    dns_header_t *hdr = (dns_header_t *)p;
    dns_header_t *rhdr = (dns_header_t *)&reply[0];
    p += sizeof(dns_header_t);

    // Sanity checks
    if (length > DNS_SERVER_MAX_LEN) {
        return;
    }
    if (length < sizeof(dns_header_t)) {
        return;
    }
    if (hdr->ancount || hdr->nscount || hdr->arcount) {
        return; // This is a reply, not a query
    }
    if (hdr->flags & DNS_FLAG_TC) {
        return; // Truncated
    }

    // Copy request to reply and set response flag
    memcpy(reply, pusrdata, length);
    rhdr->flags |= DNS_FLAG_QR;

    // Process each question
    for (i = 0; i < my_ntohs(&hdr->qdcount); i++) {
        // Parse question domain name
        p = label_to_str(pusrdata, p, length, buff, sizeof(buff));
        if (p == NULL) {
            return;
        }
        
        dns_question_footer_t *qf = (dns_question_footer_t *)p;
        p += sizeof(dns_question_footer_t);

        ESP_LOGI(TAG, "DNS query (type 0x%X class 0x%X) for %s", 
                 my_ntohs(&qf->type), my_ntohs(&qf->cl), buff);

        if (my_ntohs(&qf->type) == DNS_QTYPE_A) {
            // They want IPv4 address - respond with our AP IP
            rend = str_to_label(buff, rend, sizeof(reply) - (rend - reply));
            if (rend == NULL) {
                return;
            }
            
            dns_resource_footer_t *rf = (dns_resource_footer_t *)rend;
            rend += sizeof(dns_resource_footer_t);
            setn16(&rf->type, DNS_QTYPE_A);
            setn16(&rf->cl, DNS_QCLASS_IN);
            setn32(&rf->ttl, 0);
            setn16(&rf->rdlength, 4); // IPv4 is 4 bytes

            // Get AP IP address
            esp_netif_ip_info_t info;
            esp_netif_t *netif = esp_netif_get_default_netif();
            if (netif != NULL) {
                esp_netif_get_ip_info(netif, &info);
            }
            
            *rend++ = ip4_addr1(&info.ip);
            *rend++ = ip4_addr2(&info.ip);
            *rend++ = ip4_addr3(&info.ip);
            *rend++ = ip4_addr4(&info.ip);
            setn16(&rhdr->ancount, my_ntohs(&rhdr->ancount) + 1);
            
            ESP_LOGI(TAG, "Responding with IP: " IPSTR, IP2STR(&info.ip));
        } 
        else if (my_ntohs(&qf->type) == DNS_QTYPE_NS) {
            // Name server query
            rend = str_to_label(buff, rend, sizeof(reply) - (rend - reply));
            dns_resource_footer_t *rf = (dns_resource_footer_t *)rend;
            rend += sizeof(dns_resource_footer_t);
            setn16(&rf->type, DNS_QTYPE_NS);
            setn16(&rf->cl, DNS_QCLASS_IN);
            setn16(&rf->ttl, 0);
            setn16(&rf->rdlength, 4);
            *rend++ = 2;
            *rend++ = 'n';
            *rend++ = 's';
            *rend++ = 0;
            setn16(&rhdr->ancount, my_ntohs(&rhdr->ancount) + 1);
        }
        else if (my_ntohs(&qf->type) == DNS_QTYPE_URI) {
            // URI query
            rend = str_to_label(buff, rend, sizeof(reply) - (rend - reply));
            dns_resource_footer_t *rf = (dns_resource_footer_t *)rend;
            rend += sizeof(dns_resource_footer_t);
            dns_uri_header_t *uh = (dns_uri_header_t *)rend;
            rend += sizeof(dns_uri_header_t);
            setn16(&rf->type, DNS_QTYPE_URI);
            setn16(&rf->cl, DNS_QCLASS_URI);
            setn16(&rf->ttl, 0);
            setn16(&rf->rdlength, 4 + 16);
            setn16(&uh->prio, 10);
            setn16(&uh->weight, 1);
            memcpy(rend, "http://esp.local", 16);
            rend += 16;
            setn16(&rhdr->ancount, my_ntohs(&rhdr->ancount) + 1);
        }
    }

    // Send response
    sendto(sock_fd, (uint8_t *)reply, rend - reply, 0, 
           (struct sockaddr *)premote_addr, sizeof(struct sockaddr_in));

#pragma GCC diagnostic pop
}

/**
 * @brief DNS server task
 */
static void dns_task(void *pvParameters) {
    struct sockaddr_in server_addr;
    uint32_t ret;
    struct sockaddr_in from;
    socklen_t fromlen;
    char udp_msg[DNS_SERVER_MAX_LEN];

    ESP_LOGI(TAG, "DNS server task starting...");

    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(53);
    server_addr.sin_len = sizeof(server_addr);

    // Create UDP socket
    do {
        sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_fd == -1) {
            ESP_LOGE(TAG, "Failed to create socket!");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    } while (sock_fd == -1);

    ESP_LOGI(TAG, "DNS socket created");

    // Bind to port 53
    do {
        ret = bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (ret != 0) {
            ESP_LOGE(TAG, "Failed to bind socket!");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    } while (ret != 0);

    ESP_LOGI(TAG, "DNS server initialized and listening on port 53");

    // Main receive loop
    while (1) {
        memset(&from, 0, sizeof(from));
        fromlen = sizeof(struct sockaddr_in);
        ret = recvfrom(sock_fd, (uint8_t *)udp_msg, DNS_SERVER_MAX_LEN, 
                      0, (struct sockaddr *)&from, (socklen_t *)&fromlen);
        if (ret > 0) {
            dns_recv(&from, udp_msg, ret);
        }
    }

    close(sock_fd);
    vTaskDelete(NULL);
}

void dns_server_init(void) {
    ESP_LOGI(TAG, "Initializing DNS server for captive portal...");
    xTaskCreate(dns_task, "dns_server_task", 10000, NULL, 3, &dns_task_handle);
}

void dns_server_deinit(void) {
    if (dns_task_handle != NULL) {
        vTaskDelete(dns_task_handle);
        dns_task_handle = NULL;
    }
    if (sock_fd >= 0) {
        close(sock_fd);
        sock_fd = -1;
    }
    ESP_LOGI(TAG, "DNS server stopped");
}
