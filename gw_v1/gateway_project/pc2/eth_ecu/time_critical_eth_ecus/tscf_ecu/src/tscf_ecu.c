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
#include <unistd.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#include "avtp.h"
#include "avtp_tscf.h"
#include "can.h"
#include "common.h"
#include <pthread.h>
#include "aes.h"
#include "cmac.h"
#include "encrypt.h"
#include "utils_cmac.h"
#define ACCENTCOLOR "\033[1;36m"
#define DEFAULT "\033[0m"

#define STREAM_ID 			0x1232454585957584
#define CAN_PDU_SIZE		(sizeof(struct can_pdu) + 8)
#define PAYLOAD_LEN         8
#define NO_OF_ACF_MESSAGE			1
#define DATA_LEN			CAN_PDU_SIZE * NO_OF_ACF_MESSAGE
#define PDU_SIZE			(sizeof(struct avtp_stream_pdu) + DATA_LEN)
#define S_PDU_SIZE			(sizeof(struct avtp_stream_pdu) + CAN_PDU_SIZE)
#define RES_PDU_SIZE 		48
#define AVTP_HDR_SIZE           24
#define SINGLE_PKT_LEN		48
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
        int timer_fd;
        uint64_t single_req;
        uint64_t single_res;
        uint64_t multiple_req;
        uint64_t multiple_res;
} lis_par_t;

// use of avtp and creation of avtp_stream_pdu

struct avtp_stream_pdu *pdu;
struct avtp_stream_pdu *pdu_s;

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

// SAMPLE Queue entry--> Linked list
// But the data here is PCM
struct sample_entry {
        STAILQ_ENTRY(sample_entry) entries;

        struct timespec tspec;

        //Saving the whole of the acf message itself
        uint8_t pdu[DATA_LEN];
};

// Intialization of Head
static STAILQ_HEAD(sample_queue, sample_entry) samples;

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

// ERROR in argument providing
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
                case 'm':
                        max_transit_time = atoi(arg);
                        break;
                case 'p':
                        priority = atoi(arg);
                        break;
        }

        return 0;
}

static struct argp argp = { options, parser };

//Talker functions
static int init_pdu(struct avtp_stream_pdu *pdu, uint16_t datalen)
{
        int res;

        // pdu intialised with the VERSION, SUBTYPE and SV
        res = avtp_tscf_pdu_init(pdu);
        if (res < 0)
                return -1;

        // MR field set
        res = avtp_tscf_pdu_set(pdu, AVTP_TSCF_FIELD_MR, 0);
        if (res < 0)
                return -1;

        // TV field set
        res = avtp_tscf_pdu_set(pdu, AVTP_TSCF_FIELD_TV, 1);
        if (res < 0)
                return -1;

        // STREAM ID SET
        res = avtp_tscf_pdu_set(pdu,AVTP_TSCF_FIELD_STREAM_ID,STREAM_ID);
        if (res < 0)
                return -1;


        // TU field set
        res = avtp_tscf_pdu_set(pdu, AVTP_TSCF_FIELD_TU,0);
        if (res < 0)
                return -1;

        // Stream Data len field set
        res = avtp_tscf_pdu_set(pdu, AVTP_TSCF_FIELD_STREAM_DATA_LEN,(datalen - AVTP_HDR_SIZE));
        if (res < 0)
                return -1;


        return 0;
}

// Can payload creation
uint8_t* payload_creation(){
        static int j = 0;
        uint8_t *payload;
        payload = (uint8_t*)malloc(8*sizeof(uint8_t));
        for(int i=0;i<8;i++){
                payload[i]=j++;
        }
        return payload;
}

// Initialization of the CAN pdu
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

int talker(struct avtp_stream_pdu *pdu, uint16_t pdu_size)
{      
        int res;       
        ssize_t n;
        unsigned int nb;
        uint32_t avtp_time;
        int idx = 0;
        unsigned char out[16];
        unsigned char* encrypted_databuff;
        memset(out, 0x00, 16);	

        res = calculate_avtp_time(&avtp_time, max_transit_time);
        if (res < 0) {
                fprintf(stderr, "Failed to calculate avtp time\n");
                goto err;
        }

        res = avtp_tscf_pdu_set(pdu, AVTP_TSCF_FIELD_TIMESTAMP, avtp_time);

        if (res < 0)
                goto err;

        res = avtp_tscf_pdu_set(pdu, AVTP_TSCF_FIELD_SEQ_NUM, seq_num++);
        if (res < 0)
                goto err;

        for(int i = 0; i < ((pdu_size - 24)/ 24); i++)
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

        res = avtp_tscf_pdu_set(pdu, AVTP_TSCF_FIELD_STREAM_DATA_LEN,(pdu_size - AVTP_HDR_SIZE));
        if (res < 0)
                return -1;

        /*printf("sendbuff\n");
          for(int i = 0; i < pkt_len; i++)
          printf("%x  ",sendbuff[i]);
          printf("\n\n");*/

        aes_cmac((unsigned char*)pdu, pdu_size, (unsigned char*)out, key);
        //        printf("%sAES-128-CMAC Result%s\n", ACCENTCOLOR, DEFAULT);
        //        print_bytes(out, 16);

        encrypted_databuff = ecb_encrypt((unsigned char*)pdu, key, aes_128_encrypt, &nb, pdu_size);
        //        printf("%sAES-128-ECB Encrypt Result%s\n", ACCENTCOLOR, DEFAULT);
        //        for (auto i = 0; i < nb; i++) {
        //                print_bytes(encrypted_databuff + i * 16, 16);
        //        }
        //        printf("nb = %d\n",nb);

        n = sendto(talker_fd, encrypted_databuff, (nb*16), 0, (struct sockaddr *) &sk_addr, sizeof(sk_addr));

        if (n < 0) {
                perror("Failed to send data");
                goto err;
        }

        if (n != (nb*16)) {
                fprintf(stderr, "wrote %zd bytes, expected %d\n",
                                n, (nb*16));
        }

        return 0;

err:
        printf("Error...");
        return 1;
}
//---------------LISTENER FUNCTIONS------------------
// This function is fine and not to be touched
static int schedule_sample(int fd, struct timespec *tspec, uint8_t *sample)
{
        struct sample_entry *entry;

        entry = (struct sample_entry*)malloc(sizeof(*entry));


        if (!entry) {
                fprintf(stderr, "Failed to allocate memory\n");
                return -1;
        }

        entry->tspec.tv_sec = tspec->tv_sec;
        entry->tspec.tv_nsec = tspec->tv_nsec;
        memcpy(entry->pdu, sample, 24  /* DATA_LEN*/);

        STAILQ_INSERT_TAIL(&samples, entry, entries);

        /* If this was the first entry inserted onto the queue, we need to arm
         * the timer.
         */
        if (STAILQ_FIRST(&samples) == entry) {
                int res;

                res = arm_timer(fd, tspec);
                if (res < 0) {
                        STAILQ_REMOVE(&samples, entry, sample_entry, entries);
                        //free(entry);
                        return -1;
                }
        }

        return 0;
}


// Checking validity of the packet
static bool is_valid_packet(struct avtp_stream_pdu *pdu)
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


        if (val32 != AVTP_SUBTYPE_TSCF) {
                fprintf(stderr, "Subtype mismatch: expected %u, got %u\n",
                                AVTP_SUBTYPE_AAF, val32);
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

        // TV bit check
        res = avtp_tscf_pdu_get(pdu, AVTP_TSCF_FIELD_TV, &val64);
        if (res < 0) {
                fprintf(stderr, "Failed to get tv field: %d\n", res);
                return false;
        }
        if (val64 != 1) {
                fprintf(stderr, "tv mismatch: expected %u, got %" PRIu64 "\n",
                                1, val64);
                return false;
        }

        // MR bit check
        res = avtp_tscf_pdu_get(pdu, AVTP_TSCF_FIELD_MR, &val64);
        if (res < 0) {
                fprintf(stderr, "Failed to get mv field: %d\n", res);
                return false;
        }
        if (val64 != 0) {
                fprintf(stderr, "mr mismatch: expected %u, got %" PRIu64 "\n",
                                0, val64);
                return false;
        }

        // TU bit check
        res = avtp_tscf_pdu_get(pdu, AVTP_TSCF_FIELD_TU, &val64);
        if (res < 0) {
                fprintf(stderr, "Failed to get tu field: %d\n", res);
                return false;
        }
        if (val64 != 0) {
                fprintf(stderr, "tu mismatch: expected %u, got %" PRIu64 "\n",
                                0, val64);
                return false;
        }
        // Stream id check
        res = avtp_tscf_pdu_get(pdu, AVTP_TSCF_FIELD_STREAM_ID, &val64);
        if (res < 0) {
                fprintf(stderr, "Failed to get stream ID field: %d\n", res);
                return false;
        }
        if (val64 != STREAM_ID) {
                fprintf(stderr, "Stream ID mismatch: expected %" PRIu64 ", got %" PRIu64 "\n",
                                STREAM_ID, val64);
                return false;
        }

        // SEQ NUM CHECK
        res = avtp_tscf_pdu_get(pdu, AVTP_TSCF_FIELD_SEQ_NUM, &val64);
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
        res = avtp_tscf_pdu_get(pdu, AVTP_TSCF_FIELD_STREAM_DATA_LEN, &val64);
        if (res < 0) {
                fprintf(stderr, "Failed to get data_len field: %d\n", res);
                return false;
        }
        if (val64 != /*DATA_LEN*/ 24) {
                fprintf(stderr, "Data len mismatch: expected %d, got %" PRIu64 "\n",
                                24, val64);
                return false;
        }

        return true;
}

// Recieve function,
static int new_packet(int sk_fd, int timer_fd)
{
        int res;
        int n;
        uint64_t avtp_time;
        struct timespec tspec;
        struct avtp_stream_pdu *pdu;// = (struct avtp_stream_pdu*)alloca(RES_PDU_SIZE);
        unsigned char *decrypted_databuff;
        unsigned int no_of_bytes_recvd;
        //memset(pdu, 0, RES_PDU_SIZE);
        uint8_t recv_pdu[1536];

        n = recv(sk_fd, recv_pdu, RES_PDU_SIZE, 0);
        //        printf("no_of_bytes_recvd = %d\n",n);
        no_of_bytes_recvd = n/16;
        decrypted_databuff = ecb_decrypt((unsigned char*)recv_pdu, key, aes_128_decrypt, &no_of_bytes_recvd);
        //        printf("%sAES-128-ECB Decrypt Result%s\n", ACCENTCOLOR, DEFAULT);

        //        for(int i = 0; i < no_of_bytes_recvd*16; i++)
        //        {
        //                printf("%x ",decrypted_databuff[i]);
        //        }
        //        printf("\n\n");

        pdu = (struct avtp_stream_pdu*)decrypted_databuff;

        if (n < 0 || n != RES_PDU_SIZE) {
                perror("Failed to receive data");
              //  return -1;
        }

        // 1.
        if (!is_valid_packet(pdu)) {
                fprintf(stderr, "Dropping packet\n");
                return 0;
        }

        // 2.
        res = avtp_tscf_pdu_get(pdu, AVTP_TSCF_FIELD_TIMESTAMP, &avtp_time);
        if (res < 0) {
                fprintf(stderr, "Failed to get AVTP time from PDU\n");
                return -1;
        }

        // 3.
        res = get_presentation_time(avtp_time, &tspec);
        if (res < 0)
                return -1;

        // 4.
        res = schedule_sample(timer_fd, &tspec, pdu->avtp_payload);
        if (res < 0)
                return -1;

        return 0;
}


static int timeout(int fd)
{
        int res;
        ssize_t n;
        uint64_t expirations;
        struct sample_entry *entry;

        n = read(fd, &expirations, sizeof(uint64_t));
        if (n < 0) {
                perror("Failed to read timerfd");
                return -1;
        }

        assert(expirations == 1);

        entry = STAILQ_FIRST(&samples);
        assert(entry != NULL);

        uint8_t data_recieved[PAYLOAD_LEN];
        struct can_pdu *pdu = (struct can_pdu*)entry->pdu;
        memcpy(data_recieved,pdu->can_payload,PAYLOAD_LEN);

        STAILQ_REMOVE_HEAD(&samples, entries);
        //free(entry);

        if (!STAILQ_EMPTY(&samples)) {
                entry = STAILQ_FIRST(&samples);

                res = arm_timer(fd, &entry->tspec);
                if (res < 0)
                        return -1;
        }

        return 0;
}

int listener(int sk_fd, int timer_fd){
        int res;
        struct pollfd fds[2];

        fds[0].fd = sk_fd;
        fds[0].events = POLLIN;
        fds[1].fd = timer_fd;
        fds[1].events = POLLIN;

        while (1) {
                res = poll(fds, 2, -1);
                if (res < 0) {
                        perror("Failed to poll() fds");
                        goto err;
                }

                if (fds[0].revents & POLLIN) {
                        res = new_packet(sk_fd, timer_fd);
                        if (res < 0)
                        {
                                goto err;
                        }
                }

                if (fds[1].revents & POLLIN) {
                        res = timeout(timer_fd);

                        if (res < 0){
                                goto err;
                        }
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
                listener(lis_pars->lis_fd, lis_pars->timer_fd);
                if(once)
                {
                        once = false;
                        gettimeofday(&tv,NULL);
                        lis_pars->single_res = (1000000*tv.tv_sec) + tv.tv_usec;
                        fprintf(fp,"Single request Round trip time in usecs (critical(tscf)) = %ld\n",(lis_pars->single_res - lis_pars->single_req));
                        fflush(fp);
                        fprintf(stats_fp,"%d Single request Round trip time in usecs (critical(tscf)) = %ld\t\t", stats_count++, (lis_pars->single_res - lis_pars->single_req));
                        fflush(stats_fp);
                        lis_pars->single_req = 0;
                        lis_pars->single_res = 0;
                }

                listener(lis_pars->lis_fd, lis_pars->timer_fd);
                if(once1)
                {
                        once1 = false;
                        gettimeofday(&tv,NULL);
                        lis_pars->multiple_res= (1000000*tv.tv_sec) + tv.tv_usec;
                        fprintf(fp,"Multiple request Round trip time in usecs (critical(tscf)) = %ld\n",(lis_pars->multiple_res - lis_pars->multiple_req));
                        fflush(fp);
                        fprintf(stats_fp,"%d Multiple request Round trip time in usecs (critical(tscf)) = %ld\n", stats_count1++, (lis_pars->multiple_res - lis_pars->multiple_req));
                        fflush(stats_fp);
                        lis_pars->multiple_req = 0;
                        lis_pars->multiple_res = 0;
                }
        }

        while(1)
        {
                listener(lis_pars->lis_fd, lis_pars->timer_fd);		
                if(exit_recv)
                {
                        pthread_exit(NULL);
                }
        }
}

// Caller
int main(int argc, char *argv[])
{
        uint8_t res;
        int timer_fd;
        uint32_t send_ret, recv_ret;
        pthread_t send_th, recv_th;
        FILE *payload_fp;  
        char c;
        int sk_fd;

        argp_parse(&argp, argc, argv, 0, NULL, NULL);
        
        stats_fp = fopen("stats/tscf_rrt_stats.log", "w");

        pdu = (struct avtp_stream_pdu*)alloca(1500);

        pdu_s = (struct avtp_stream_pdu*)alloca(S_PDU_SIZE);

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

                  STAILQ_INIT(&samples);

                  sk_fd = create_listener_socket(ifname, macaddr, ETH_P_TSN);
                  if (sk_fd < 0)
                          return 1;

                  timer_fd = timerfd_create(CLOCK_REALTIME, 0);
                  if (timer_fd < 0) {
                          return 1;
                  }

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
                  lis_pars.timer_fd = timer_fd;

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

