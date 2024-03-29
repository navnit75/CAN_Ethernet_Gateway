

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


#define STREAM_ID 			0x1232454585957584
#define CAN_PDU_SIZE		(sizeof(struct can_pdu) + 8)
#define PAYLOAD_LEN         8
#define NO_OF_ACF_MESSAGE			1
#define DATA_LEN			CAN_PDU_SIZE * NO_OF_ACF_MESSAGE
#define PDU_SIZE			(sizeof(struct avtp_ntscf_stream_pdu) + DATA_LEN)


// interface name handling
static char ifname[IFNAMSIZ];

// Mac address handling
static uint8_t macaddr[ETH_ALEN];

// Sequence number handling
static uint8_t expected_seq;

// ARGUMENT PARSER
static struct argp_option options[] = {
        {"dst-addr", 'd', "MACADDR", 0, "Stream Destination MAC address" },
        {"ifname", 'i', "IFNAME", 0, "Network Interface" },
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
    }

    return 0;
}

static struct argp argp = { options, parser };


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
    struct avtp_ntscf_stream_pdu *pdu = alloca(PDU_SIZE);

    memset(pdu, 0, PDU_SIZE);

    n = recv(sk_fd, pdu, PDU_SIZE, 0);
    if (n < 0 || n != PDU_SIZE) {
        perror("Failed to receive data");
        return -1;
    }

    // 1.
    if (!is_valid_packet(pdu)) {
        fprintf(stderr, "Dropping packet\n");
        return 0;
    }

    
    uint8_t data_recieved[PAYLOAD_LEN];
    struct can_pdu *acf_msg = (struct can_pdu*)pdu->avtp_payload;
    memcpy(data_recieved,acf_msg->can_payload,PAYLOAD_LEN);

    res = present_data(data_recieved, PAYLOAD_LEN);

    if (res < 0)
        return -1;

    return 0;
}

int main(int argc, char *argv[])
{
    int sk_fd, res;
    struct pollfd fds[1];

    argp_parse(&argp, argc, argv, 0, NULL, NULL);

    sk_fd = create_listener_socket(ifname, macaddr, ETH_P_TSN);
    if (sk_fd < 0)
        return 1;

    fds[0].fd = sk_fd;
    fds[0].events = POLLIN;
  
    while (1) {
        res = poll(fds, 2, -1);
        if (res < 0) {
            perror("Failed to poll() fds");
            goto err;
        }

        if (fds[0].revents & POLLIN) {
            res = new_packet(sk_fd);
            if (res < 0)
                goto err;
        }
    }

    return 0;

    err:
    close(sk_fd);
    return 1;
}

