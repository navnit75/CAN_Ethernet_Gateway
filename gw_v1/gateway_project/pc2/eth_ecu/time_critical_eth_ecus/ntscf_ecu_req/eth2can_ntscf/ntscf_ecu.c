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
#include "avtp_ntscf.h"
#include "can.h"
#include "common.h"
#include <pthread.h>

#define CAN_PDU_SIZE		(sizeof(struct can_pdu) + 8)
#define PAYLOAD_LEN         8
#define NO_OF_ACF_MESSAGE			4
#define DATA_LEN			CAN_PDU_SIZE * NO_OF_ACF_MESSAGE
#define PDU_SIZE			(sizeof(struct avtp_ntscf_stream_pdu) + DATA_LEN)
#define RES_PDU_SIZE 		36

FILE *fp;

uint64_t usecs_req;
uint64_t usecs_res;

struct timeval tv;

typedef struct {
        int lis_fd;
} lis_par_t;

static struct can_pdu *can_pdu[4];
static char ifname[IFNAMSIZ];
static uint8_t macaddr[ETH_ALEN];
static uint8_t expected_seq;
static int priority = -1;


struct sockaddr_ll sk_addr;
static int talker_fd;
// use of avtp and creation of avtp_stream_pdu
struct avtp_ntscf_stream_pdu *pdu; //= alloca(PDU_SIZE);


//struct can_pdu *can_pdu; //= alloca(CAN_PDU_SIZE);

// Seq num temp variable
uint8_t seq_num = 0;

static struct argp_option options[] = {
        {"dst-addr", 'd', "MACADDR", 0, "Stream Destination MAC address" },
        {"ifname", 'i', "IFNAME", 0, "Network Interface" },
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

static int init_pdu(struct avtp_ntscf_stream_pdu *pdu)
{
        int res;

        // pdu intialised with the VERSION, SUBTYPE and SV
        res = avtp_ntscf_pdu_init(pdu);
        if (res < 0)
                return -1;

        // DATA LEN field set
        res = avtp_ntscf_pdu_set(pdu, AVTP_NTSCF_FIELD_DATA_LEN,DATA_LEN);
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

int talker()
{
        int res,idx;
        int a = CAN_PDU_SIZE;
        uint8_t payloads[4][8] = {{0x7f, 0xbb, 0x10, 0x11, 0x01, 0x02, 0x03, 0x04},
                {0x8f, 0xbb, 0x10, 0x12, 0x11, 0x22, 0x33, 0x44},
                {0x9f, 0xbb, 0x10, 0x13, 0x21, 0x32, 0x43, 0x54},
                {0xaf, 0xbb, 0x10, 0x14, 0x31, 0x42, 0x53, 0x64}};

        // int i = 0;
        // Infinite loop of packets to be sent to the , listener
        //    while (1) {
        ssize_t n;



        res = avtp_ntscf_pdu_set(pdu, AVTP_NTSCF_FIELD_SEQ_NUM, seq_num++);
        /*if (res < 0)
          goto err;

          uint8_t *payload = payload_creation();
          memcpy(can_pdu->can_payload,payload,8);

          free(payload);

          memcpy(pdu->avtp_payload,can_pdu,CAN_PDU_SIZE);
          res = present_data(can_pdu->can_payload, PAYLOAD_LEN);

          if (res < 0)
          return -1;*/

        // uint8_t *payload = payload_creation();
        idx = 28;
        printf("\nTalker sending data..\n");
        for(int i = 0; i < (DATA_LEN/ 24); i++)
        {
                memcpy(can_pdu[i]->can_payload, payloads[i], PAYLOAD_LEN);
                res = present_data(can_pdu[i]->can_payload,PAYLOAD_LEN);
                printf("\n");
                // free(payloads[i]);
                memcpy(pdu->avtp_payload+idx, can_pdu[i], 24);
                idx += 24;
        }

        n = sendto(talker_fd, pdu, PDU_SIZE, 0,
                        (struct sockaddr *) &sk_addr, sizeof(sk_addr));
        if (n < 0) {
                perror("Failed to send data");
                goto err;
        }

        if (n != PDU_SIZE) {
                fprintf(stderr, "wrote %zd bytes, expected %zd\n",
                                n, PDU_SIZE);
        }
        // i++;
        //    }


        // close(talker_fd);
        return 0;

err:
        printf("Error...");
        //close(talker_fd);
        return 1;
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
        if (val64 != /*DATA_LEN*/ 12) {
                fprintf(stderr, "Data len mismatch: expected %d, got %" PRIu64 "\n",
                                12, val64);
                return false;
        }

        return true;
}

static int new_packet(int sk_fd)
{
        int res;
        ssize_t n;
        struct avtp_ntscf_stream_pdu *pdu = alloca(RES_PDU_SIZE);
        uint8_t *recv_buff;

        memset(pdu, 0, RES_PDU_SIZE);

        printf("listener receiving data\n");
        n = recv(sk_fd, pdu, RES_PDU_SIZE, 0);
        //n = recv(sk_fd, recv_buff, PDU_SIZE, 0);
        if (n < 0 || n != RES_PDU_SIZE) {
                perror("Failed to receive data");
                return -1;
        }


        if (!is_valid_packet(pdu)) {
                fprintf(stderr, "Dropping packet\n");
                return 0;
        }


        uint8_t *data_recieved;
        struct can_pdu *acf_msg = (struct can_pdu*)pdu->avtp_payload;
        data_recieved = acf_msg->can_payload;

        res = present_data(data_recieved, PAYLOAD_LEN);

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

        //close(sk_fd);
        return 0;

err:
        //close(sk_fd);
        return 1;
}

void *send_to_gateway(void *args)
{
        static bool once = true;

        //      while(1)
        {
                talker();
                if(once)
                {
                        once = false;
                        gettimeofday(&tv,NULL);
                        usecs_req = (1000000*tv.tv_sec) + tv.tv_usec;
                        fprintf(fp,"time at request sent in usecs = %ld\n", usecs_req);
                        fflush(fp);
                }
                usleep(10000);
        }
}

void *recv_from_gateway(void *args)
{
        lis_par_t *lis_pars = (lis_par_t *)args;
        static bool once = true;
        uint32_t count = 0;

        while(1)
        {
                listener(lis_pars->lis_fd);
                if(once)
                {
                        once = false;
                        gettimeofday(&tv,NULL);
                        usecs_res = (1000000*tv.tv_sec) + tv.tv_usec;
                        fprintf(fp,"time at response received in usecs = %ld\n", usecs_res);
                        fflush(fp);
                        fprintf(fp,"Round trip time in usecs = %ld\n",(usecs_res - usecs_req));
                        fflush(fp);
                }
                printf("Count: %d\n", count++);
                usleep(10000);
        }
}


int main(int argc, char *argv[])
{
        uint32_t count =0; 
        uint8_t ret, res;
        //static bool once = true;
        //static bool once1 = true;
        // Send the data
        // 1. Before sending display the sent payload
        // 2. Then display the received payload

        // calling these will store the respective variables into the
        argp_parse(&argp, argc, argv, 0, NULL, NULL);

        uint32_t send_ret, recv_ret;
        pthread_t send_th, recv_th;

        int sk_fd;
        fp= fopen("log_ntscf.txt", "a");

        //listener	
        sk_fd = create_listener_socket(ifname, macaddr, ETH_P_TSN);
        if (sk_fd < 0)
                return 1;


        //talker
        pdu = alloca(PDU_SIZE);
        can_pdu[0] = alloca(CAN_PDU_SIZE);
        can_pdu[1] = alloca(CAN_PDU_SIZE);
        can_pdu[2] = alloca(CAN_PDU_SIZE);
        can_pdu[3] = alloca(CAN_PDU_SIZE);

        // Create talker socket
        talker_fd = create_talker_socket(priority);
        if (talker_fd < 0)
                return 1;

        res = setup_socket_address(talker_fd, ifname, macaddr, ETH_P_TSN, &sk_addr);
        if (res < 0){
                printf("socket error");
                return 1;
        }

        // Init_PDU call , one function above
        res = init_pdu(pdu);
        if (res < 0){
                printf("TSCF header intialization error");
                return 1;
        }
        res = init_can_pdu(can_pdu[0]);
        if (res < 0){
                printf("Can pdu intialization error");
                return 1;
        }
        res = init_can_pdu(can_pdu[1]);
        if (res < 0){
                printf("Can pdu intialization error");
                return 1;
        }

        res = init_can_pdu(can_pdu[2]);
        if (res < 0){
                printf("Can pdu intialization error");
                return 1;
        }

        res = init_can_pdu(can_pdu[3]);
        if (res < 0){
                printf("Can pdu intialization error");
                return 1;

        }  
        lis_par_t lis_pars;

        lis_pars.lis_fd = sk_fd;
        if( (send_ret = pthread_create( &send_th, NULL, &send_to_gateway, NULL)) )
        {
                printf("eth_ecu :  Thread creation failed: %d\n", send_ret);
        }
        if( (recv_ret = pthread_create( &recv_th, NULL, &recv_from_gateway,(void *)&lis_pars )) )
        {
                printf("eth_ecu : Thread creation failed: %d\n", recv_ret);
        }

        pthread_join(send_th, NULL);
        pthread_join(recv_th, NULL);
        // Listener returns 1 it means there is an error
        return 0 ;

}

