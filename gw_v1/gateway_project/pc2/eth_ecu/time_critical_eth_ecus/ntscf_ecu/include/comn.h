
#ifndef _COMN_H_
#define _COMN_H_
#include <stdint.h>
#define PORT 8080
#define BUFF_SIZE 5

#define ETH_REQ_TIME_DELAY 2000000
#define READ_TIME_OUT 0x165A0BC00
#define RETRY_TIMES 80 
#define DELAY_BETWEEN_EACH_RETRY 1  // provided in seconds
//structure definitation for OBD packet
typedef struct  __attribute__((packed)) {
    uint8_t pci;
    uint8_t sid;
    uint8_t sub_fun;
    uint8_t did[2];
    uint8_t data[3];
} uds_t;

typedef struct  __attribute__((packed)) {
    uint8_t pci;
    uint8_t sid;
    uint8_t sub_fun;
    uint8_t did[2];
    uint8_t data[4096];
} uds_res_t;

typedef struct  __attribute__((packed)) {
    uint8_t pci[2];
    uint8_t sid;
    uint8_t sub_fun;
    uint8_t did[2];
    uint8_t data[4096];
} uds_mres_t;

typedef struct  __attribute__((packed)) {
    uint8_t proto_ver;
    uint8_t inv_proto_ver;
    uint16_t payload_type;
    uint32_t  payload_len;
} doip_t;

typedef struct  __attribute__((packed)) {
    doip_t doip;
    uds_t uds;
} obd_t;

// ETH ECU threrads
void *send_thread(void *args);
void *recv_thread(void *args);
void timeout_function(uint32_t pkts_len);
void *recv_from_can(void *args);

typedef struct {
    uint32_t tcp_fd;
    uint32_t udp_fd;
    uint64_t single_req;
    uint64_t single_res;
    uint64_t multiple_req;
    uint64_t multiple_res;
    struct sockaddr_in host_address;
    uint32_t argc;
    const uint8_t **argv;
    pthread_t th_id;
    obd_t *obd;
} args_t;

#endif
