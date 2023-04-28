#ifndef _CRITICAL_H_
#define _CRITICAL_H_

#include <stdint.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
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
#include <sys/queue.h>
#include <time.h>
#include <sys/time.h>
#include <inttypes.h>
#include <sys/timerfd.h>
#include "avtp.h"
#include "avtp_tscf.h"
#include "avtp_ntscf.h"
#include "can.h"
#include "common.h"
#include <sys/queue.h>
#include <errno.h>
#include "critical_config.h"

/************************** STATIC CONFIGURATIONS ****************************************************/
#define CAN_PDU_SIZE		        (sizeof(struct can_pdu) + 8)
#define PAYLOAD_LEN                     8
#define NO_OF_ACF_MESSAGE		4
#define DATA_LEN			CAN_PDU_SIZE * NO_OF_ACF_MESSAGE
#define DATA_LEN_NTSCF                  24
#define PDU_SIZE			(sizeof(struct avtp_stream_pdu) + DATA_LEN)
#define RESPONSE_PDU_SIZE                    48
#define RESPONSE_DATA_LEN                    24

#define AES_CMAC_SIZE           16
#define CAN_PAYLOAD_LEN         8
#define MAX_ETH_PYLD_SIZE       1518
#define EMTY 0
#define FULL  1

#define TSCF_ACF_PAYLOAD_START_IDX  40
#define NTSCF_ACF_PAYLOAD_START_IDX 28
#define ACF_PACKET_SIZE         24

#define SINGLE_TSCF_PDU_SIZE    48
#define SINGLE_NTSCF_PDU_SIZE   36

#define CALCULATE_TSCF_PAYLOAD_LEN(BUFF) (BUFF[20] << 8) | (BUFF[21])
#define CALCULATE_NTSCF_PAYLOAD_LEN(BUFF) ((BUFF[1] & 0x07) << 8) | (BUFF[2])
/****************************************************************************************************************/


/* structure that has all the information for proper talker/listener functioning */
typedef struct {
        /* socket descriptor for talker(sender) */
        int talker_fd;
        /* socket descriptor for listener(receiver) */
        int listener_fd;
        /* socket descriptor for arming a timer */
        int timer_fd;
        /* A variable for keeping track tscf/ntscf connection type */
        int tscf_on;
        int ntscf_on;
        /* Buffer for storing the can payload of every acf packet */
        unsigned char can_payload[PAYLOAD_LEN];
        /* Security key buffer*/
        unsigned char cmac_key[AES_CMAC_SIZE];
        /* pointer to a structure that stores tscf pdu */
        struct avtp_stream_pdu *tscf_pdu;
        /* pointer to a structure that stores ntscf pdu */
        struct avtp_ntscf_stream_pdu *ntscf_pdu;
        /* pointer to a structure that stores can pdu */
        struct can_pdu *can_pdu;
        /* A socket address structure that stores the client address */
        struct sockaddr_ll sk_addr;
        /* stores the size of pdu to be sent*/
        uint32_t pdu_size;
        /* buffer to store the received packet from an ethernet ecu */
        unsigned char recv_pdu_buffer[MAX_ETH_PYLD_SIZE];
} talker_listener_params_t;


/* structure that has all the information for proper thread functioning */
typedef struct {
        /* lock used for sharing buffer between threads */
        pthread_mutex_t lock;
        /* semaphore used for synchronizing the request received and response sent */
        sem_t sem;
        /* ETH-to-CAN Buffer */
        uint8_t eth_to_can_buff[BUFF_SIZE_CRITICAL][MAX_ETH_PYLD_SIZE];
        /* CAN-to-ETH buffer */
        uint8_t can_to_eth_buff[BUFF_SIZE_CRITICAL][MAX_ETH_PYLD_SIZE];
        /* file descriptor for the CAN dummy driver */
        uint32_t driver_fd;
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
        uint32_t is_new[BUFF_SIZE_CRITICAL];
        /* used for reading CAN data */
        uint8_t read_buff[CAN_PAYLOAD_LEN];
        /* CAN read bytes */
        uint32_t read_bytes;
        /* is connection type tscf or ntscf */
        bool is_tscf;
        bool is_ntscf;
        /* macaddress buffer */
        uint8_t mac_addr[6];
        char *if_name;
        /* A pointer pointing to the structure which consists all the relevant information for talker/listener functioning */
        talker_listener_params_t *talk_lis_data;
} thread_data_critical_t;

/** @brief "listener" function will receive the critical packets being sent by the talker

  If a critical packet is being sent by an eth_ecu, the listener will receive the packet and 
  store it in the buffer, which will then be forwarded to the gateway_ecu which inturn will forward 
  it to the can_ecu, similarly can to eth transfer is carried out. The listener will also de-crypt the packets
  being received

  @param *listener A pointer pointing to the structure which has all the information that a listener needs for its functioning
  @return int.
 */
int listener(talker_listener_params_t *listener);

/** @brief "talker" function will send the critical packets to the listener.

  The talker function will send all the critical packets from an eth_ecu to a gateway_ecu, and will also encrypt the 
  packets before being sent. similarly can to eth transfer is carried out.

  @param *talker : A pointer pointing to the structure which has all the information that a talker needs for its functioning
  @return : int.
 */
int talker(talker_listener_params_t *talker);

/** @brief : This thread will receive the requests coming from an eth ecu and store it in eth2can buffer. Before this thread is run, 
  listener will validate the incoming packet

  The talker function will send all the critical packets from an eth_ecu to a gateway_ecu, and will also encrypt the packets before being sent.
  similarly can to eth transfer is carriesd out

  @param *talker : A pointer pointing to the structure which has all the information that a talker needs for its functioning
  @return : int.
 */
void *recv_request_from_eth(void *args);

/** @brief : This thread will forward the requests to can_ecu

  Each request stored in eth2can buffer will be processed and only the can payload of every acf packet if forwarded to the can ecu

  @param *args : A pointer pointing to the structure which has all the information regarding the eth2can data.
  @return : void *.
 */
void *forward_request_to_can(void *args);

/** @brief : This thread will receive the reponses coming from a can_ecu

  The responses coming from can ecu will be received in this thread and will be stored in a temperory buffer.

  @param *args : A pointer pointing to the structure which has all the information regarding the eth2can data.
  @return : void *.
 */
void *recv_res_from_can(void *args);

/** @brief : This thread will forwared the reponses of every request, back to the ethernet ecu.

  The responses stored in the temporary buffer will be processed here and the talker function is called to send it to the ethernet ecu

  @param *args : A pointer pointing to the structure which has all the information regarding the eth2can data.
  @return : void *.
 */
void *forward_respose_to_eth(void *args);

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

/** @brief : This function is used to receive the critical packet coming from an ethernet ecu

  The pakcets coming from an ethernet ecu will be recived here and and sequence number will be validated, if its a tscf packet then avtp_time is validated

  @param *listener : A pointer pointing to the structure which has all the information that a listener needs for its functioning.
  @return : int.
 */
int new_packet(talker_listener_params_t *listener);

/** @brief : This function is used to validate the packet that has been received by an ethernet ecu.

  The packet received from an ethernet ecu will be validated for all the feilds present in that packet

  @param *pdu : A pointer pointing to the received pdu(i.e received packet)
  @return : bool.
 */
bool is_valid_packet(unsigned char *pdu);

/** @brief : This function will set all the fields of a can pdu to the initial values provided

  This function will initialize all the fields of a can pdu to the vlaues provided by the user.

  @param *pdu : A pointer pointing to the a can pdu variable.
  @return : int.
 */
int init_can_pdu(struct can_pdu * pdu);

/** @brief : This function will set all the fields of a tscf/ntscf pdu to the initial values provided

  This function will initialize all the fields of a tscf/ntscf pdu to the vlaues provided by the user.

  @param *pdu : A pointer pointing to the structure that has all the information related to talker/listener initlialization.
  @return : int.
 */
int init_pdu(talker_listener_params_t *);

/** @brief : This function will wait for timeout for a packet.

  This function will wait until the timeout happens which is set by the user & then the packet will be dequeued from the queue and stored in the buffer
  in case of a tscf packet

  @param *pdu : A pointer pointing to the structure that has all the information related to the listener & its functioning
  @return : int.
 */
int timeout(talker_listener_params_t *listener);

/** @brief : This thread will create a new channel for eth2can transfer

  This thread will create the talker/listener sockets and will spwan all the threads necessary for eth2can data transfer

  @param *args : A pointer pointing to the structure that has all the information regarding the eth2can data
  @return : void *.
 */
void *create_eth2can_channel_critical(void *args);

/** @brief : This thread will create a new channel for can2eth transfer

  This thread will create the talker/listener sockets and will spwan all the threads necessary for can2eth data transfer

  @param *args : A pointer pointing to the structure that has all the information regarding the can2eth data
  @return : void *.
 */
void *create_can2eth_channel_critical(void *args);

/** @brief : This function will initilaize the queue for enqueing and dequeing the data received from ethernet.

  This function will make use of stailq libarary to create a queue

  @param void 
  @return : void
 */
void init_queue(void);

#endif

