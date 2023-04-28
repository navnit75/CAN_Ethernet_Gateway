#include <assert.h>
#include <argp.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <poll.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/timerfd.h>
#include <sys/time.h>
#include <unistd.h>
#include <inttypes.h>
#include "avtp.h"
#include "avtp_ntscf.h"
#include "can.h"
#include "common.h"
#include "aes.h"
#include "cmac.h"
#include "encrypt.h"
#include "utils_cmac.h"
#include "pthread.h"

#define ACCENTCOLOR "\033[1;36m"
#define DEFAULT "\033[0m"

#define CAN_PDU_SIZE		(sizeof(struct can_pdu) + 8)
#define PAYLOAD_LEN         8
#define NO_OF_ACF_MESSAGE			1
#define DATA_LEN			CAN_PDU_SIZE * NO_OF_ACF_MESSAGE
#define PDU_SIZE			(sizeof(struct avtp_ntscf_stream_pdu) + DATA_LEN)
#define S_PDU_SIZE			(sizeof(struct avtp_ntscf_stream_pdu) + CAN_PDU_SIZE)


#define RES_PDU_SIZE 		36
#define AVTP_HDR_SIZE           12
#define SINGLE_PKT_LEN		36
uint8_t exit_recv = 0;

uint64_t usecs_req;
uint64_t usecs_reqs;
uint64_t usecs_res;
uint64_t usecs_ress;

struct timeval tv;
FILE *fp;
FILE *stats_fp;

int stats_count;
int stats_count1;

typedef struct {
        int lis_fd;
        uint64_t single_req;
        uint64_t single_res;
        uint64_t multiple_req;
        uint64_t multiple_res;
} lis_par_t;

// use of avtp and creation of avtp_stream_pdu

struct avtp_ntscf_stream_pdu *pdu;
struct avtp_ntscf_stream_pdu *pdu_s;

static struct can_pdu *can_pdu[60];
static struct can_pdu *can_pdu_s;
/*static struct can_pdu *can_pdu1;
  static struct can_pdu *can_pdu2;
  static struct can_pdu *can_pdu3;*/
// Seq num temp variable
static uint64_t seq_num = 0;
static int talker_fd;
// Socket address
static struct sockaddr_ll sk_addr;

// interface name handling
static char ifname[IFNAMSIZ];

// Mac address handling
static uint8_t macaddr[ETH_ALEN];

// Sequence number handling
static uint8_t expected_seq;

static int priority = -1;

uint16_t send_buffer_idx = 0;

static int max_transit_time;
uint8_t send_once = 0, eth_type_udp = 0, eth_type_tcp = 0;
uint8_t payload_buffer[4096];
uint16_t payload_idx = 0;
uint32_t no_of_acf_msgs;

unsigned char key[] = {
        0x31, 0x50, 0x10, 0x47,
        0x17, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
};

// ARGUMENT PARSER
static struct argp_option options[] = {
        {"dst-addr", 'd', "MACADDR", 0, "Stream Destination MAC address" },
        {"ifname", 'i', "IFNAME", 0, "Network Interface" },
        {"max-transit-time", 'm', "MSEC", 0, "Maximum Transit Time in ms" },
        {"prio", 'p', "NUM", 0, "SO_PRIORITY to be set in socket" },
        { 0 }
};

static error_t parser(int key, char *arg, struct argp_state *state)
{
        int res;

        switch (key) {
                case 'd':
                        res = sscanf(arg, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                                        &macaddr[0], &macaddr[1], &macaddr[2],
                                        &macaddr[3], &macaddr[4], &macaddr[5]);
                        if (res != 6) {
                                fprintf(stderr, "Invalid address\n");
                                exit(EXIT_FAILURE);
                        }

                        break;
                case 'i':
                        strncpy(ifname, arg, sizeof(ifname) - 1);
                        break;
                case 'p':
                        priority = atoi(arg);
                        break;
        }

        return 0;
}

static struct argp argp = { options, parser };

static int init_pdu(struct avtp_ntscf_stream_pdu *pdu, uint16_t datalen)
{
        int res;

        // pdu intialised with the VERSION, SUBTYPE and SV
        res = avtp_ntscf_pdu_init(pdu);
        if (res < 0)
                return -1;

        // DATA LEN field set
        res = avtp_ntscf_pdu_set(pdu, AVTP_NTSCF_FIELD_DATA_LEN,(datalen - AVTP_HDR_SIZE));
        if (res < 0)
                return -1;

        return 0;
}

uint8_t* payload_creation(){
        static int j = 0;
        uint8_t *payload;
        payload = (uint8_t*)malloc(8*sizeof(uint8_t));
        for(int i=0;i<8;i++){
                payload[i]=j++;
        }
        return payload;
}

static int init_can_pdu(struct can_pdu * pdu){
        int res;

        res = can_pdu_init(pdu);
        if (res < 0)
                return -1;

        res = can_pdu_set(pdu, CAN_FIELD_MSG_TYPE,1);
        if( res < 0)
                return -1;

        res = can_pdu_set(pdu, CAN_FIELD_MSG_LEN,6);
        if(res < 0)
                return -1;

        res = can_pdu_set(pdu,CAN_FIELD_RTR,1);
        if(res < 0)
                return -1;

        res = can_pdu_set(pdu,CAN_FIELD_MTV,1);
        if(res < 0)
                return -1;

        res = can_pdu_set(pdu,CAN_FIELD_EFF,0);
        if(res < 0)
                return -1;

        res = can_pdu_set(pdu,CAN_FIELD_BRS,1);
        if(res < 0)
                return -1;


        res = can_pdu_set(pdu,CAN_FIELD_FDF,1);
        if(res < 0)
                return -1;

        res = can_pdu_set(pdu,CAN_FIELD_ESI,0);
        if(res < 0)
                return -1;

        res = can_pdu_set(pdu,CAN_FIELD_CAN_BUS_ID,0);
        if(res < 0)
                return -1;

        res = can_pdu_set(pdu,CAN_FIELD_MESSAGE_TIMESTAMP,0xf234567812345678);
        if(res < 0)
                return -1;

        res = can_pdu_set(pdu,CAN_FIELD_CAN_IDENTIFIER,0x0000000);
        if(res < 0)
                return -1;

        return 0;

}

int talker(struct avtp_ntscf_stream_pdu *pdu, uint16_t pdu_size)
{
        int res;
        unsigned int nb;
        ssize_t n;
        unsigned char out[16];
        unsigned char* encrypted_databuff;
        int idx = 0;
        
        memset(out, 0x00, 16);

        res = avtp_ntscf_pdu_set(pdu, AVTP_NTSCF_FIELD_SEQ_NUM, seq_num);
        seq_num++;
        if (res < 0)
        {
                //goto err;
        }
        for(int i = 0; i < ((pdu_size - 12)/ 24); i++)
        {
                for(int j = 0; j < 8; j++)
                {
                        if(send_buffer_idx <= payload_idx)
                        {
                                can_pdu[i]->can_payload[j] = payload_buffer[send_buffer_idx++];
                        }
                        else
                        {
                                can_pdu[i]->can_payload[j] = ' '; 
                        }
                }
                memcpy(pdu->avtp_payload+idx, can_pdu[i], 24);
                idx += 24;
        }

        res = avtp_ntscf_pdu_set(pdu, AVTP_NTSCF_FIELD_DATA_LEN,(pdu_size - AVTP_HDR_SIZE));
        if (res < 0)
                return -1;
        aes_cmac((unsigned char*)pdu, pdu_size, (unsigned char*)out, key);
//        printf("%sAES-128-CMAC Result%s\n", ACCENTCOLOR, DEFAULT);
//        print_bytes(out, 16);

        encrypted_databuff = ecb_encrypt((unsigned char*)pdu, key, aes_128_encrypt, &nb, pdu_size);
//        printf("%sAES-128-ECB Encrypt Result%s\n", ACCENTCOLOR, DEFAULT);
//        for (auto i = 0; i < nb; i++) {
//                print_bytes(encrypted_databuff + i * 16, 16);
//        }
//        printf("nb = %d\n",nb);

        n = sendto(talker_fd, encrypted_databuff, (nb*16), 0,
                        (struct sockaddr *) &sk_addr, sizeof(sk_addr));
        if (n < 0) {
                perror("Failed to send data");
               // goto err;
        }

        if (n != (nb * 16)) {
                fprintf(stderr, "wrote %zd bytes, expected %zd\n",
                                n, (nb*16));
        }

        return 0;

//err: printf("Error...");

        //return 1;
}

static bool is_valid_packet(struct avtp_ntscf_stream_pdu *pdu)
{
        // Recieved avtp_common_pdu
        struct avtp_common_pdu *common = (struct avtp_common_pdu *) pdu;
        uint64_t val64;
        uint32_t val32;
        int res;

        // SUBTYPE CHECK
        res = avtp_pdu_get(common, AVTP_FIELD_SUBTYPE, &val32);


        if (res < 0) {
                fprintf(stderr, "Failed to get subtype field: %d\n", res);
                return false;
        }

        if (val32 != AVTP_SUBTYPE_NTSCF) {
                fprintf(stderr, "Subtype mismatch: expected %u, got %u\n",
                                AVTP_SUBTYPE_NTSCF, val32);
                return false;
        }

        // Version check
        res = avtp_pdu_get(common, AVTP_FIELD_VERSION, &val32);
        
        if (res < 0) {
                fprintf(stderr, "Failed to get version field: %d\n", res);
                return false;
        }

        if (val32 != 0) {
                fprintf(stderr, "Version mismatch: expected %u, got %u\n",
                                0, val32);
                return false;
        }

        // SEQ NUM CHECK
        res = avtp_ntscf_pdu_get(pdu, AVTP_NTSCF_FIELD_SEQ_NUM, &val64);
        if (res < 0) {
                fprintf(stderr, "Failed to get sequence num field: %d\n", res);
                return false;
        }

        if (val64 != expected_seq) {
                // If there is a sequence number mismatch we are not invalidating the packet

                fprintf(stderr, "Sequence number mismatch: expected %u, got %" PRIu64 "\n",
                                expected_seq, val64);
                expected_seq = val64;
        }

        expected_seq++;

        // Stream data len check
        res = avtp_ntscf_pdu_get(pdu, AVTP_NTSCF_FIELD_DATA_LEN, &val64);
        if (res < 0) {
                fprintf(stderr, "Failed to get data_len field: %d\n", res);
                return false;
        }
        if (val64 != 24) {
                fprintf(stderr, "Data len mismatch: expected %lu, got %" PRIu64 "\n",
                                DATA_LEN, val64);
                return false;
        }

        return true;
}

static int new_packet(int sk_fd)
{
        int res;
        ssize_t n;
        struct avtp_ntscf_stream_pdu *pdu;// = (struct avtp_ntscf_stream_pdu*)alloca(PDU_SIZE);
        unsigned char *decrypted_databuff;
        unsigned int no_of_bytes_recvd;
      //  memset(pdu, 0, PDU_SIZE);
        uint8_t recv_pdu[1536];

        n = recv(sk_fd, recv_pdu, RES_PDU_SIZE, 0);

        //printf("no_of_bytes_recvd = %d\n",n);
        no_of_bytes_recvd = (n/16) ;
        decrypted_databuff = ecb_decrypt((unsigned char*)recv_pdu, key, aes_128_decrypt, &no_of_bytes_recvd);
        //printf("%sAES-128-ECB Decrypt Result%s\n", ACCENTCOLOR, DEFAULT);

        //for(int i = 0; i < n; i++)
        //{
        //       printf("%x ",decrypted_databuff[i]);
        //}
        //printf("\n\n");
        
        pdu = (struct avtp_ntscf_stream_pdu*)decrypted_databuff;

        if (n < 0 || n != RES_PDU_SIZE) {
                perror("Failed to receive data");
                return -1;
        }

        if (!is_valid_packet(pdu)) {
                fprintf(stderr, "Dropping packet\n");
                return 0;
        }

        
        if (res < 0)
                return -1;

        return 0;
}

int listener(int sk_fd){
        int res;
        struct pollfd fds[1];

        fds[0].fd = sk_fd;
        fds[0].events = POLLIN;

        while (1) {
                res = poll(fds, 1, -1);

                if (res < 0) {
                        perror("Failed to poll() fds");
                        goto err;
                }

                if (fds[0].revents & POLLIN) {
                        res = new_packet(sk_fd);
                        if (res < 0)
                                goto err;
                        break;
                }
        }

        return 0;

err:
        return 1;
}

void *send_to_gateway(void *args)
{
        bool once = true;
        bool once1 = true;
        lis_par_t *params = (lis_par_t *)args;
        uint32_t temp_no_of_acf_msgs = 0;

        temp_no_of_acf_msgs = no_of_acf_msgs;

        while(1)
        {
                talker(pdu_s, S_PDU_SIZE);
                if(once)
                {
                        once = false;
                        gettimeofday(&tv,NULL);
                        params->single_req = (1000000*tv.tv_sec) + tv.tv_usec;
                }
                if(send_buffer_idx > payload_idx)
                {
                        if(send_once == 1)
                        {
                                send_once = 0;                                        
                                send_buffer_idx = 0;
                                pthread_exit(NULL);                
                        }
                        else
                        {
                                send_buffer_idx = 0;
                        }
                }
                usleep(1000000);
                temp_no_of_acf_msgs--;

                if(temp_no_of_acf_msgs > 60)
                {

                        talker(pdu, ((60 *24) + AVTP_HDR_SIZE));
                        temp_no_of_acf_msgs -=60;

                }
                else
                {
                        talker(pdu, ((temp_no_of_acf_msgs*24) + AVTP_HDR_SIZE));
                        temp_no_of_acf_msgs = no_of_acf_msgs;
                }
                if(once1)
                {
                        once1 = false;
                        gettimeofday(&tv,NULL);
                        params->multiple_req = (1000000*tv.tv_sec) + tv.tv_usec;
                }
                if(send_buffer_idx > payload_idx)
                {
                        if(send_once == 1)
                        {
                                send_once = 0;                                        
                                send_buffer_idx = 0;
                                pthread_exit(NULL);                
                        }
                        else
                        {
                                send_buffer_idx = 0;
                        }
                }
                usleep(1000000);
        }
}

void *recv_from_gateway(void *args)
{
        lis_par_t *lis_pars = (lis_par_t *)args;
        bool once = true;
        bool once1 = true;

        {
                listener(lis_pars->lis_fd);
                if(once)
                {
                        once = false;
                        gettimeofday(&tv,NULL);
                        lis_pars->single_res = (1000000*tv.tv_sec) + tv.tv_usec;
                        fprintf(fp,"Single request Round trip time in usecs (critical(ntscf)) = %ld\n",(lis_pars->single_res - lis_pars->single_req));
                        fflush(fp);
                        fprintf(stats_fp,"%d Single request Round trip time in usecs (critical(ntscf)) = %ld\t\t", stats_count++, (lis_pars->single_res - lis_pars->single_req));
                        fflush(stats_fp);
                        lis_pars->single_req = 0;
                        lis_pars->single_res = 0;
                }

                listener(lis_pars->lis_fd);
                if(once1)
                {
                        once1 = false;
                        gettimeofday(&tv,NULL);
                        lis_pars->multiple_res= (1000000*tv.tv_sec) + tv.tv_usec;
                        fprintf(fp,"Multiple request Round trip time in usecs (critical(ntscf)) = %ld\n",(lis_pars->multiple_res - lis_pars->multiple_req));
                        fflush(fp);
                        fprintf(stats_fp,"%d Multiple request Round trip time in usecs (critical(ntscf)) = %ld\n", stats_count1++, (lis_pars->multiple_res - lis_pars->multiple_req));
                        fflush(stats_fp);
                        lis_pars->multiple_req = 0;
                        lis_pars->multiple_res = 0;
                }
        }

        while(1)
        {
                listener(lis_pars->lis_fd);		
                if(exit_recv)
                {
                        pthread_exit(NULL);
                }
        }
}

int main(int argc, char *argv[])
{
        uint8_t res;
        uint32_t send_ret, recv_ret;
        pthread_t send_th, recv_th;
        FILE *payload_fp;  
        char c;
        int sk_fd;

        argp_parse(&argp, argc, argv, 0, NULL, NULL);
        
        stats_fp = fopen("stats/ntscf_rrt_stats.log", "w");

        pdu = (struct avtp_ntscf_stream_pdu*)alloca(1500);

        pdu_s = (struct avtp_ntscf_stream_pdu*)alloca(S_PDU_SIZE);

        can_pdu_s = (struct can_pdu*)alloca(CAN_PDU_SIZE);

        for(int i = 0; i < 60; i++)
        {
                can_pdu[i]  = (struct can_pdu*)alloca(CAN_PDU_SIZE);
                init_can_pdu(can_pdu[i]);
        }

        init_pdu(pdu_s, SINGLE_PKT_LEN);
        init_pdu(pdu, 60*24);

        while(1)
        {
starting :        payload_idx = 0;

                  do {
                          payload_fp = fopen("/opt/gateway/send.txt", "r+");

                          if(payload_fp == NULL){
                                  continue;
                          }

                          c = fgetc(payload_fp);
                          if(c != 'S') {
                                  fclose(payload_fp);
                                  sleep(1);
                                  continue;
                          }

                          for(int i = 0; i < 5; i++)
                          {
                                  c = fgetc(payload_fp);
                          }
                          break;

                  } while(1);

                  if(c == 'O')
                  {
                          send_once = 1;
                  }

                  while(fgetc(payload_fp) != '\n');

                  while( (c = fgetc(payload_fp)) != EOF)
                  {
                          payload_buffer[payload_idx++] = c;
                  }

                  if(payload_idx == 0)
                          goto starting;

                  payload_buffer[payload_idx] = ' ';
                  no_of_acf_msgs = ((payload_idx)/8);

                  if(!((payload_idx % 8) == 0))
                          no_of_acf_msgs++;

                  sleep(1);
                  rewind(payload_fp);
                  fputc('0',payload_fp);

                  fp= fopen("/opt/gateway/eth_tscf_rt.txt", "w");

                  sk_fd = create_listener_socket(ifname, macaddr, ETH_P_TSN);
                  if (sk_fd < 0)
                          return 1;


                  talker_fd = create_talker_socket(priority);
                  if (talker_fd < 0)
                          return 1;

                  res = setup_socket_address(talker_fd, ifname, macaddr, ETH_P_TSN, &sk_addr);
                  if (res < 0){
                          printf("socket error");
                          return 1;
                  }

                  lis_par_t lis_pars;
                  lis_pars.lis_fd = sk_fd;

                  if( (send_ret = pthread_create( &send_th, NULL, &send_to_gateway, (void *)&lis_pars)))
                  {
                          printf("eth_ecu :  Thread creation failed: %d\n", send_ret);
                  }
                  if( (recv_ret = pthread_create( &recv_th, NULL, &recv_from_gateway,(void *)&lis_pars)))
                  {
                          printf("eth_ecu : Thread creation failed: %d\n", recv_ret);
                  }

                  pthread_join(send_th, NULL);
                  exit_recv = 1;

                  fclose(payload_fp);
        }
        return 0 ;
}
