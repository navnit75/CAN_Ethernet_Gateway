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
#include "avtp.h"
#include "avtp_ntscf.h"
#include "can.h"
#include "common.h"
#include "aes.h"
#include "cmac.h"
#include "encrypt.h"
#include "utils_cmac.h"

#define ACCENTCOLOR "\033[1;36m"
#define DEFAULT "\033[0m"

#define CAN_PDU_SIZE		(sizeof(struct can_pdu) + 8)
#define PAYLOAD_LEN         8
#define NO_OF_ACF_MESSAGE			1
#define DATA_LEN			CAN_PDU_SIZE * NO_OF_ACF_MESSAGE
#define PDU_SIZE			(sizeof(struct avtp_ntscf_stream_pdu) + DATA_LEN)

static char ifname[IFNAMSIZ];
static uint8_t macaddr[ETH_ALEN];
static uint64_t expected_seq;
static int priority = -1;

FILE *out_fp;
struct sockaddr_ll sk_addr;
static int talekr_fd;
// use of avtp and creation of avtp_stream_pdu
struct avtp_ntscf_stream_pdu *pdu; //= alloca(PDU_SIZE);

unsigned char key[] = {
        0x31, 0x50, 0x10, 0x47,
        0x17, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
};

struct can_pdu *can_pdu; //= alloca(CAN_PDU_SIZE);

// Seq num temp variable
static uint8_t seq_num = 0;

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
        int res;
        unsigned int nb;
        ssize_t n;
        unsigned char out[16];
        unsigned char* encrypted_databuff;
        uint8_t *payload = payload_creation();
        memcpy(can_pdu->can_payload,payload,8);

        memset(out, 0x00, 16);
        free(payload);

        memcpy(pdu->avtp_payload,can_pdu,CAN_PDU_SIZE);
        
        res = avtp_ntscf_pdu_set(pdu, AVTP_NTSCF_FIELD_SEQ_NUM, seq_num);
        seq_num++;
        if (res < 0)
        {
                //goto err;
        }

        aes_cmac((unsigned char*)pdu, PDU_SIZE, (unsigned char*)out, key);
//        printf("%sAES-128-CMAC Result%s\n", ACCENTCOLOR, DEFAULT);
//        print_bytes(out, 16);

        encrypted_databuff = ecb_encrypt((unsigned char*)pdu, key, aes_128_encrypt, &nb, PDU_SIZE);
//        printf("%sAES-128-ECB Encrypt Result%s\n", ACCENTCOLOR, DEFAULT);
//        for (auto i = 0; i < nb; i++) {
//                print_bytes(encrypted_databuff + i * 16, 16);
//        }
//        printf("nb = %d\n",nb);

        n = sendto(talekr_fd, encrypted_databuff, (nb*16), 0,
                        (struct sockaddr *) &sk_addr, sizeof(sk_addr));
        if (n < 0) {
                perror("Failed to send data");
               // goto err;
        }

        if (n != 48) {
                fprintf(stderr, "wrote %zd bytes, expected %zd\n",
                                n, PDU_SIZE);
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
               // expected_seq = val64;
        }

        expected_seq++;

        // Stream data len check
        res = avtp_ntscf_pdu_get(pdu, AVTP_NTSCF_FIELD_DATA_LEN, &val64);
        if (res < 0) {
                fprintf(stderr, "Failed to get data_len field: %d\n", res);
                return false;
        }
        if (val64 != DATA_LEN) {
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

        n = recv(sk_fd, recv_pdu, 1536, 0);

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

        if (n < 0 || n != 48) {
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

        for(int j = 0; j < 8; j++)
        {
                fprintf(out_fp, "%c",data_recieved[j]);
                fflush(out_fp);
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

int main(int argc, char *argv[])
{
        uint8_t ret, res;

        // Send the data
        // 1. Before sending display the sent payload
        // 2. Then display the received payload

        // calling these will store the respective variables into the
        argp_parse(&argp, argc, argv, 0, NULL, NULL);
        out_fp = fopen("/opt/gateway/eth_ntscf.txt", "w");

        int sk_fd;

        //listener	
        sk_fd = create_listener_socket(ifname, macaddr, ETH_P_TSN);
        if (sk_fd < 0)
                return 1;

        //talker
        pdu = (struct avtp_ntscf_stream_pdu *)alloca(PDU_SIZE);
        can_pdu = (struct can_pdu*)alloca(CAN_PDU_SIZE);

        // Create talker socket
        talekr_fd = create_talker_socket(priority);
        if (talekr_fd < 0)
                return 1;

        res = setup_socket_address(talekr_fd, ifname, macaddr, ETH_P_TSN, &sk_addr);
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
        res = init_can_pdu(can_pdu);
        if (res < 0){
                printf("Can pdu intialization error");
                return 1;
        }
        while(1)
        {
                ret = listener(sk_fd);

                ret = talker();

                if(ret)
                {
                        while(1);
                }
                //usleep(100000);
        }
        // Listener returns 1 it means there is an error
        return 0 ;
}
