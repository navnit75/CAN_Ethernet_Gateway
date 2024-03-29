/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of Intel Corporation nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* AAF Listener example.
 *
 * This example implements a very simple AAF listener application which
 * receives AFF packets from the network, retrieves the PCM samples, and
 * writes them to stdout once the presentation time is reached.
 *
 * For simplicity, the example accepts only AAF packets with the following
 * specification:
 *    - Sample format: 16-bit little endian
 *    - Sample rate: 48 kHz
 *    - Number of channels: 2 (stereo)
 *
 * TSN stream parameters such as destination mac address are passed via
 * command-line arguments. Run 'aaf-listener --help' for more information.
 *
 * This example relies on the system clock to schedule PCM samples for
 * playback. So make sure the system clock is synchronized with the PTP
 * Hardware Clock (PHC) from your NIC and that the PHC is synchronized with
 * the PTP time from the network. For further information on how to synchronize
 * those clocks see ptp4l(8) and phc2sys(8) man pages.
 *
 * The easiest way to use this example is combining it with 'aplay' tool
 * provided by alsa-utils. 'aplay' reads a PCM stream from stdin and sends it
 * to a ALSA playback device (e.g. your speaker). So, to play Audio from a TSN
 * stream, you should do something like this:
 *
 * $ aaf-listener <args> | aplay -f dat -t raw -D <playback-device>
 */

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
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>

#define STREAM_ID 			0x1232454585957584
#define CAN_PDU_SIZE		(sizeof(struct can_pdu) + 8)
#define PAYLOAD_LEN         8
#define NO_OF_ACF_MESSAGE			1
#define DATA_LEN			CAN_PDU_SIZE * NO_OF_ACF_MESSAGE
#define PDU_SIZE			(sizeof(struct avtp_stream_pdu) + DATA_LEN)

FILE *fp;

uint64_t usecs_req;
uint64_t usecs_res;

struct timeval tv;
// use of avtp and creation of avtp_stream_pdu
static struct avtp_stream_pdu *pdu;
static struct can_pdu *can_pdu;
// Seq num temp variable
static uint32_t seq_num = 0;
static int talekr_fd;
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


static int max_transit_time;



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
static int init_pdu(struct avtp_stream_pdu *pdu)
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
        res = avtp_tscf_pdu_set(pdu, AVTP_TSCF_FIELD_STREAM_DATA_LEN,DATA_LEN);
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

int talker(){
        int res = 0;



        int a = CAN_PDU_SIZE;




        //    int i =0;
        //    // Infinite loop of packets to be sent to the , listener
        //    while (i<1) {
        ssize_t n;
        uint32_t avtp_time;

        res = calculate_avtp_time(&avtp_time, max_transit_time);
        if (res < 0) {
                fprintf(stderr, "Failed to calculate avtp time\n");
                goto err;
        }


        res = avtp_tscf_pdu_set(pdu, AVTP_TSCF_FIELD_TIMESTAMP,
                        avtp_time);
        if (res < 0)
                goto err;

        res = avtp_tscf_pdu_set(pdu, AVTP_TSCF_FIELD_SEQ_NUM, seq_num++);
        if (res < 0)
                goto err;

        uint8_t *payload = payload_creation();

        memcpy(can_pdu->can_payload,payload,PAYLOAD_LEN);
        free(payload);
        // res = present_data(can_pdu->can_payload,PAYLOAD_LEN);
        if(res<0)
                goto err;

        memcpy(pdu->avtp_payload,can_pdu,CAN_PDU_SIZE);

        n = sendto(talekr_fd, pdu, PDU_SIZE, 0,
                        (struct sockaddr *) &sk_addr, sizeof(sk_addr));
        if (n < 0) {
                perror("Failed to send data");
                goto err;
        }

        if (n != PDU_SIZE) {
                fprintf(stderr, "wrote %zd bytes, expected %zd\n",
                                n, PDU_SIZE);

        }
        //        i++;
        //    }


        //close(talekr_fd);
        return 0;

err:
        printf("Error...");
        //close(talekr_fd);
        return 1;
}
//---------------LISTENER FUNCTIONS------------------
// This function is fine and not to be touched
static int schedule_sample(int fd, struct timespec *tspec, uint8_t *sample)
{
        struct sample_entry *entry;

        entry = malloc(sizeof(*entry));


        if (!entry) {
                fprintf(stderr, "Failed to allocate memory\n");
                return -1;
        }

        entry->tspec.tv_sec = tspec->tv_sec;
        entry->tspec.tv_nsec = tspec->tv_nsec;
        memcpy(entry->pdu, sample, DATA_LEN);

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
        if (val64 != DATA_LEN) {
                fprintf(stderr, "Data len mismatch: expected %lu, got %" PRIu64 "\n",
                                DATA_LEN, val64);
                return false;
        }

        return true;
}

// Recieve function,
static int new_packet(int sk_fd, int timer_fd)
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

        res = present_data(data_recieved, PAYLOAD_LEN);

        if (res < 0)
                return -1;

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
                        printf("%d\n", __LINE__);
                        goto err;
                }

                if (fds[0].revents & POLLIN) {
                        res = new_packet(sk_fd, timer_fd);
                        if (res < 0)
                        {
                                printf("%d\n", __LINE__);
                                goto err;
                        }
                }

                if (fds[1].revents & POLLIN) {
                        res = timeout(timer_fd);

                        if (res < 0){
                                printf("%d\n", __LINE__);
                                goto err;
                        }
                        break;
                }
        }

        //close(sk_fd);
        //close(timer_fd);
        return 0;


err:
        printf("%d\n", __LINE__);
        //close(sk_fd);
        //close(timer_fd);
        return 1;
}



// Caller
int main(int argc, char *argv[])
{
        uint32_t count =0; 
        uint8_t ret, res;
        int timer_fd;
        static bool once = true;
        static bool once1 = true;
        // Send the data
        // 1. Before sending display the sent payload
        // 2. Then display the received payload

        // calling these will store the respective variables into the
        argp_parse(&argp, argc, argv, 0, NULL, NULL);
        fp= fopen("log_tscf_req.txt", "a");

        int sk_fd;

        //listener	
        STAILQ_INIT(&samples);
        sk_fd = create_listener_socket(ifname, macaddr, ETH_P_TSN);
        if (sk_fd < 0)
                return 1;

        timer_fd = timerfd_create(CLOCK_REALTIME, 0);
        if (timer_fd < 0) {
                //close(sk_fd);
                printf("%d\n", __LINE__);
                return 1;
        }
        //talker
        pdu = alloca(PDU_SIZE);
        can_pdu = alloca(CAN_PDU_SIZE);

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


        while(1){
                //printf("\nTalker sending data..\n");
                if(once)
                {
                        once = false;
                        gettimeofday(&tv,NULL);
                        usecs_req = (1000000*tv.tv_sec) + tv.tv_usec;
                        fprintf(fp,"time at request sent in usecs = %ld\n", usecs_req);
                        fflush(fp);
                }


                ret = listener(sk_fd, timer_fd);

                ret = talker();
                if(once1)
                {
                        once1 = false;
                        gettimeofday(&tv,NULL);
                        usecs_res = (1000000*tv.tv_sec) + tv.tv_usec;
                        fprintf(fp,"time at response received in usecs = %ld\n", usecs_res);
                        fflush(fp);
                        fprintf(fp,"Round trip time in usecs = %ld\n",(usecs_res - usecs_req));
                        fflush(fp);
                }

                // printf("\nListener receiving data...:  %d\n", ret);
                // printf("Count: %d\n", count++);	
                /*if(ret)
                  {
                  while(1);
                  }*/
        }
        // Listener returns 1 it means there is an error
        return 0 ;
}

