#ifndef _GATEWAY_NON_CRITICAL_H_ 
#define _GATEWAY_NON_CRITICAL_H_

#include <stdint.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <arpa/inet.h> 
#include <pthread.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdbool.h>
#include <semaphore.h>
#include <sys/time.h>
#include <time.h>
#include "non_critical_config.h"

//structure definitation for UDS packet
typedef struct  __attribute__((packed)) {
        uint8_t pci;
        uint8_t sid;
        uint8_t sub_fun;
        uint8_t did[2];
        uint8_t data[3];
} uds_t;

//structure definitation for DOIP packet
typedef struct  __attribute__((packed)) {
        uint8_t proto_ver;
        uint8_t inv_proto_ver;
        uint16_t payload_type;
        uint32_t  payload_len;
} doip_t;

//structure definitation for OBD packet
typedef struct  __attribute__((packed)) {
        doip_t doip;
        uds_t uds;
} obd_t;


/* Used by all the threads as their individual copy of context */
typedef struct {
        /* lock used for sharing buffer between threads */
        pthread_mutex_t buffer_lock;
        /* lock used for synchronizing response received from can and being sent to ethernet */
        pthread_mutex_t read_lock;
        /* semaphore used for synchronizing the request received and response sent */
        sem_t sem;
        /* ETH-to-CAN Buffer */
        uint8_t eth_to_can_buff[ETH_TO_CAN_BUFF_SIZE][MAX_ETH_PYLD_SIZE];
        /* CAN-to-ETH buffer */
        uint8_t can_to_eth_buff[CAN_TO_ETH_BUFF_SIZE][8];
        /* buffer start position */
        uint32_t start_pos;
        /* buffer end position */
        uint32_t end_pos;
        /* used to indicate the current position in the Buffer to fill new ETH Packets */
        uint32_t fil_pos;
        /* used to indicate the current position in the Buffer taken for processing */
        uint32_t emty_pos;
        /* Etherent packet count */
        uint32_t count;
        /* Indicates if the Packet in the ETH-to-CAN Buffer is new and to be processed */
        uint32_t is_new_eth[ETH_TO_CAN_BUFF_SIZE];
        /* file descriptor for the CAN dummy driver */
        uint32_t driver_fd;
        /* socket used for udp connection */
        uint32_t udp_fd;
        /* socket used for tcp for listening to clients */
        uint32_t tcp_lisfd;
        /* socket used for transfering data through tcp connection */
        uint32_t tcp_fd;
        /* varibale used for selecting a can interface for a perticular ethernet inteface */
        uint8_t canif_sel;
        /* Indicates if there is a new CAN-to-ETH packet */
        uint32_t is_new_can[CAN_TO_ETH_BUFF_SIZE];
        /* for storing socket addresses */
        struct sockaddr_in serv_addr_tcp;
        struct sockaddr_in serv_addr_udp; 
        struct sockaddr_in client_addr;
        /* used for timing calculation */
        struct timeval tv;
        uint64_t res_tm;
        /* security key buffer */
        unsigned char cmac_key[16];
        /* variable used for storing the received packet in odb format */
        obd_t *obd_data;
        /* used for locking a can interface when a single can inteface is shared between multiple ethernet interfaces */
        uint8_t lock_num;
        /* varibale to indicate which device is in execution */
        uint8_t device_num;
        /* used for reading CAN data */
        uint8_t read_buff[8];
        /* CAN read bytes */
        uint32_t read_bytes;
        /* to store ip address */
        char *host_address;
        /* is connection type tcp or udp */
        bool is_proto_udp;
        bool is_proto_tcp;

        /* to maintain the thread ids of all the threads in execution */
        pthread_t recv_eth_th_id;
        pthread_t send_can_th_id;
        pthread_t send_eth_th_id;
        pthread_t recv_from_can_th_id;
        pthread_t recv_can_req_id;
        pthread_t send_req_recv_res_id;
        pthread_t reconnect_id;

} thread_data_t;

/** @brief : This thread will receive the requests coming from a can ecu and store it in can2eth buffer.

  The requests coming from a  can ecu will be stored in a buffer in this thread.

  @param *args : A pointer pointing to the structure which has all the information regarding the can2eth data.
  @return : void *.
 */
void *recv_request_from_can(void *args);

/** @brief : This thread will forward the requests to eth ecu and receive the responses back.

  The requests from a can ecu, which are stored in the buffer will be forwared to the ethernet ecu and will wait until the response is received back.
  The response received in this thread will be forwarded back to the can ecu

  @param *args : A pointer pointing to the structure which has all the information regarding the can2eth data.
  @return : void *.
 */
void *forward_request_to_eth_respond_back_to_can(void *args);

/** @brief : This thread will receive the requests coming from an eth ecu and store it in eth2can buffer.

  This thread will make use socket apis to receive the packets from an ethernet ecu and store it in eth2can buffer.

  @param *args : A pointer pointing to the structure which has all the information regarding the eth2can data.
  @return : void *.
 */
void *recv_request_from_eth(void *args);

/** @brief : This thread will forward the requests to can_ecu

  Each request stored in eth2can buffer will be processed and only the can payload of every diop/uds packet is forwarded to the can ecu

  @param *args : A pointer pointing to the structure which has all the information regarding the eth2can data.
  @return : void *.
 */
void *forward_request_to_can(void *args);

/** @brief : This thread will receive the reponses coming from a can_ecu

  The responses coming from can ecu will be received in this thread and will be stored in a temperory buffer.

  @param *args : A pointer pointing to the structure which has all the information regarding the eth2can data.
  @return : void *.
 */
void *recv_response_from_can(void *args);

/** @brief : This thread will forwared the reponses of every request, back to the ethernet ecu.

  The responses stored in the temporary buffer will be processed here and the sen socket api is called to send the reponse packet to the ethernet ecu

  @param *args : A pointer pointing to the structure which has all the information regarding the eth2can data.
  @return : void *.
 */
void *forward_response_to_eth(void *args);

/** @brief : This thread will create a new channel for eth2can transfer

  This thread will create tcp/udp sockets and will spwan all the threads necessary for eth2can data transfer

  @param *args : A pointer pointing to the structure that has all the information regarding the eth2can data
  @return : void *.
 */
void *create_eth_to_can_channel(void *args);

/** @brief : This thread will create a new channel for can2eth transfer

  This thread will create the tcp/udp sockets and will spwan all the threads necessary for can2eth data transfer

  @param *args : A pointer pointing to the structure that has all the information regarding the can2eth data
  @return : void *.
 */
void *create_can_to_eth_channel(void *args);

/** @brief : This function will reconnect to the ethernet server in case of failue for tcp connection

  This function will reconnect to the ethernet server(which receives a request and sends the response).

  @param *args : A pointer pointing to the structure that has all the information regarding the can2eth data
  @return : void *.
 */
int reconnect_client(thread_data_t *thread_data);

/** @brief : This function will reconnect to the ethernet client in case of failue for tcp connection

  This function will reconnect to the ethernet client(which sends a request and receives the response).

  @param *args : A pointer pointing to the structure that has all the information regarding the eth2can data
  @return : void *.
 */
int reconnect_server(thread_data_t *thread_data);

#endif
