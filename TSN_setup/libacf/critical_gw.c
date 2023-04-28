#include <argp.h>
#include <arpa/inet.h>
#include <assert.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <poll.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <time.h>

#include "avtp.h"
#include "avtp_tscf.h"
#include "common.h"
#include "can.h"


#define STREAM_ID 			0x1232454585957584
#define CAN_PAYLOAD_SIZE            8
#define PAYLOAD_LEN                 CAN_PAYLOAD_SIZE
#define NO_OF_ACF_MESSAGE			1
#define DATA_LEN			((sizeof(struct can_pdu) + CAN_PAYLOAD_SIZE) * NO_OF_ACF_MESSAGE)
#define PDU_SIZE			(sizeof(struct avtp_stream_pdu) + DATA_LEN)
#define CAN_PDU_SIZE		(sizeof(struct can_pdu) + CAN_PAYLOAD_SIZE)
#define ONE_SEC			    1000000000ULL
#define pr_err(s)           fprintf(stderr, s "\n")
#define DEFAULT_PERIOD		1000000
#define DEFAULT_DELAY		500000

#ifndef SO_TXTIME
#define SO_TXTIME		    61
    #define SCM_TXTIME		    SO_TXTIME
#endif

static char ifname[IFNAMSIZ];
static uint8_t macaddr[ETH_ALEN];
static int priority = -1;
static int max_transit_time;
static struct sockaddr_ll addr = {0};
static int running = 1, use_so_txtime = 1;
static uint64_t base_time = 0;
static int period_nsec = DEFAULT_PERIOD;
static int waketx_delay = DEFAULT_DELAY;
static int iterations = 10000000;
static int can_payload_size = CAN_PAYLOAD_SIZE;
static uint64_t seq_num = 0;
static uint64_t expected_seq;
static uint8_t payload[CAN_PAYLOAD_SIZE]={0};
static uint8_t data_recieved[PAYLOAD_LEN]={0};

static struct argp_option options[] = {
        {"dst-addr", 'd', "MACADDR", 0, "Stream Destination MAC address" },
        {"ifname", 'i', "IFNAME", 0, "Network Interface" },
        {"max-transit-time", 'm', "MSEC", 0, "Maximum Transit Time in ms" },
        {"prio", 'p', "NUM", 0, "SO_PRIORITY to be set in socket" },
        {"iterations",'n',"NUM",0,"Num of packets to be sent"},
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
        case 'm':
            max_transit_time = atoi(arg);
            break;
        case 'p':
            priority = atoi(arg);
            break;
        case 'n':
            iterations = atoi(arg);
            if(iterations < 0 ){
                fprintf(stderr,"Invalid Number of Iterations Provided\n");
                exit(EXIT_FAILURE);
            }
            break;
    }

    return 0;
}


static struct argp argp = { options, parser };

/* Intializing the TSCF payload */
static int init_pdu(struct avtp_stream_pdu *pdu)
{
    int res;
    res = avtp_tscf_pdu_init(pdu);
    if (res < 0)
        return -1;

    res = avtp_tscf_pdu_set(pdu, AVTP_TSCF_FIELD_MR, 0);
    if (res < 0)
        return -1;

    res = avtp_tscf_pdu_set(pdu, AVTP_TSCF_FIELD_TV, 1);
    if (res < 0)
        return -1;

    res = avtp_tscf_pdu_set(pdu,AVTP_TSCF_FIELD_STREAM_ID,STREAM_ID);
    if (res < 0)
        return -1;


    res = avtp_tscf_pdu_set(pdu, AVTP_TSCF_FIELD_TU,0);
    if (res < 0)
        return -1;

    res = avtp_tscf_pdu_set(pdu, AVTP_TSCF_FIELD_STREAM_DATA_LEN,DATA_LEN);
    if (res < 0)
        return -1;


    return 0;
}


/* Intialization of CAN pdu */
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

/* Payload creation */
static uint8_t* payload_creation(){
    static int j = 0;

    for(int i=0;i<CAN_PAYLOAD_SIZE;i++){
        payload[i]=j;
        j++;
    }
    return payload;
}

static bool is_valid_packet(struct avtp_stream_pdu *pdu)
{

    struct avtp_common_pdu *common = (struct avtp_common_pdu *) pdu;
    uint64_t val64;
    uint32_t val32;
    int res;

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

    res = avtp_tscf_pdu_get(pdu, AVTP_TSCF_FIELD_SEQ_NUM, &val64);
    if (res < 0) {
        fprintf(stderr, "Failed to get sequence num field: %d\n", res);
        return false;
    }

    if (val64 != expected_seq) {

        fprintf(stderr, "Sequence number mismatch: expected %lu, got %" PRIu64 "\n",
                expected_seq, val64);
        expected_seq = val64;
    }

    expected_seq++;

    res = avtp_tscf_pdu_get(pdu, AVTP_TSCF_FIELD_STREAM_DATA_LEN, &val64);
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




/* Normalizing the ts */
/* Need to Normalize = When adding delays to ts->tv_nsec
 * The timestamp may go above the required limit
 * Normalize increments the seconds or decrement the second
 * according to the tv_nsec s
 */
static void normalize(struct timespec *ts)
{
    while (ts->tv_nsec > 999999999) {
        ts->tv_sec += 1;
        ts->tv_nsec -= ONE_SEC;
    }

    while (ts->tv_nsec < 0) {
        ts->tv_sec -= 1;
        ts->tv_nsec += ONE_SEC;
    }
}

/* Recieving the packet, schedule the packet */
static int new_packet(int sk_fd)
{
    int res;
    ssize_t n;
    uint64_t avtp_time;
    struct timespec tspec;
    struct avtp_stream_pdu *pdu = alloca(PDU_SIZE);

    memset(pdu, 0, PDU_SIZE);

    n = recv(sk_fd, pdu, PDU_SIZE, 0);
    if (n < 0 || n != PDU_SIZE) {
        perror("Failed to receive data");
        return -1;
    }

    if (!is_valid_packet(pdu)) {
        fprintf(stderr, "Dropping packet\n");
        return 0;
    }

    res = avtp_tscf_pdu_get(pdu, AVTP_TSCF_FIELD_TIMESTAMP, &avtp_time);
    if (res < 0) {
        fprintf(stderr, "Failed to get AVTP time from PDU\n");
        return -1;
    }

    res = get_presentation_time(avtp_time, &tspec);
    if (res < 0)
        return -1;




    struct can_pdu *can_pdu = (struct can_pdu*)pdu->avtp_payload;
    memcpy(data_recieved,can_pdu->can_payload,PAYLOAD_LEN);
    printf("Data Recieved : ");
    res = present_data(data_recieved, PAYLOAD_LEN);



    if (res < 0)
        return -1;

    return 0;
}


int main(int argc, char *argv[])
{
    int talker_fd,listener_fd, res;
    struct pollfd fds[1];

    clockid_t clkid = CLOCK_TAI;

    struct timespec ts;
    int cnt, err;
    __u64 txtime;

    uint8_t *can_payload;


    if (base_time == 0) {
        clock_gettime(clkid, &ts);
        ts.tv_sec += 1;
        ts.tv_nsec = ONE_SEC - waketx_delay;
    } else {
        ts.tv_sec = base_time / ONE_SEC;
        ts.tv_nsec = (base_time % ONE_SEC) - waketx_delay;
    }

    normalize(&ts);

    txtime = ts.tv_sec * ONE_SEC + ts.tv_nsec;
    txtime += waketx_delay;


    struct avtp_stream_pdu *pdu = alloca(PDU_SIZE);
    struct can_pdu *can_pdu = alloca(CAN_PDU_SIZE);


    argp_parse(&argp, argc, argv, 0, NULL, NULL);

    talker_fd = create_talker_socket(priority,clkid);
    if (talker_fd < 0)
        return 1;

    res = setup_socket_address(talker_fd, ifname, macaddr, ETH_P_TSN, &addr);
    if (res < 0){
        printf("socket error");
        goto err;
    }

    listener_fd = create_listener_socket(ifname, macaddr, ETH_P_TSN);
    if (listener_fd < 0)
        return 1;


    fds[0].fd = listener_fd;
    fds[0].events = POLLIN;

    res = init_pdu(pdu);
    if (res < 0){
        printf("TSCF header intialization error");
        goto err;
    }

    res = init_can_pdu(can_pdu);
    if (res < 0){
        printf("Can pdu intialization error");
        goto err;
    }

    printf("Listener Started...\n");

    /* Talker */
    while (iterations--) {
        while (1) {
            res = poll(fds, 1, -1);
            if (res < 0) {
                perror("Failed to poll() fds");
                goto err;
            }

            if (fds[0].revents & POLLIN) {
                printf("Request Recieved ...\n");
                res = new_packet(listener_fd);
                if (res < 0)
                    goto err;
                else
                    break;
            }


        }

        ssize_t n;
        uint32_t avtp_time;

        res = calculate_avtp_time(&avtp_time, max_transit_time);
        if (res < 0) {
            fprintf(stderr, "Failed to calculate avtp time\n");
            goto err;
        }

        res = avtp_tscf_pdu_set(pdu, AVTP_TSCF_FIELD_TIMESTAMP,avtp_time);
        if (res < 0)
            goto err;

        res = avtp_tscf_pdu_set(pdu, AVTP_TSCF_FIELD_SEQ_NUM, seq_num++);
        printf("SeqNum of response: %ld\n", seq_num);

        if (res < 0)
            goto err;

        can_payload = payload_creation();
        printf("Data Sent: ");
        present_data(can_payload,CAN_PAYLOAD_SIZE);
        memcpy(can_pdu->can_payload,can_payload,can_payload_size);
        memcpy(pdu->avtp_payload,can_pdu,CAN_PDU_SIZE);

        cnt = sendto(talker_fd,pdu, PDU_SIZE, 0,(struct sockaddr*) &addr,sizeof(addr));
        printf("Response Sent...\n\n");

        if (cnt != PDU_SIZE) {
            pr_err("send failed");
        }
    }



    close(talker_fd);
    close(listener_fd);
    return 0;

    err:
    pr_err("ERROR...");
    close(talker_fd);
    close(listener_fd);
    return 1;
}
