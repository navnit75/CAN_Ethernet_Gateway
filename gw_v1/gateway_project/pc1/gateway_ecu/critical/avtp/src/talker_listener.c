#include <assert.h>
#include <poll.h>
#include "critical.h"
#include "aes.h"
#include "cmac.h"
#include "encrypt.h"
#include "utils_cmac.h"

static uint64_t seq_num = 0;

static uint8_t expected_seq;

static uint64_t num_acfs;

struct sample_entry {
        STAILQ_ENTRY(sample_entry) entries;
        struct timespec tspec;
        //Saving the whole of the acf message itself
        uint8_t *pdu;
};

static STAILQ_HEAD(sample_queue, sample_entry) samples;

//Talker functions
int init_pdu(talker_listener_params_t *initialize)
{
        int res;

        if(initialize->ntscf_on == 1)
        {
                res = avtp_ntscf_pdu_init((struct avtp_ntscf_stream_pdu *)initialize->ntscf_pdu);
                if (res < 0)
                        return -1;

                res = avtp_ntscf_pdu_set((struct avtp_ntscf_stream_pdu *)initialize->ntscf_pdu, AVTP_NTSCF_FIELD_DATA_LEN,DATA_LEN_NTSCF);
                if (res < 0)
                        return -1;
        }
        if(initialize->tscf_on == 1)
        {
                // pdu intialised with the VERSION, SUBTYPE and SV
                res = avtp_tscf_pdu_init((struct avtp_stream_pdu*)initialize->tscf_pdu);
                if (res < 0)
                        return -1;

                // MR field set
                res = avtp_tscf_pdu_set((struct avtp_stream_pdu*)initialize->tscf_pdu, AVTP_TSCF_FIELD_MR, 0);
                if (res < 0)
                        return -1;

                // TV field set
                res = avtp_tscf_pdu_set((struct avtp_stream_pdu*)initialize->tscf_pdu, AVTP_TSCF_FIELD_TV, 1);
                if (res < 0)
                        return -1;

                // STREAM ID SET
                res = avtp_tscf_pdu_set((struct avtp_stream_pdu*)initialize->tscf_pdu,AVTP_TSCF_FIELD_STREAM_ID,STREAM_ID);
                if (res < 0)
                        return -1;

                // TU field set
                res = avtp_tscf_pdu_set((struct avtp_stream_pdu*)initialize->tscf_pdu, AVTP_TSCF_FIELD_TU,0);
                if (res < 0)
                        return -1;

                // Stream Data len field set
                res = avtp_tscf_pdu_set((struct avtp_stream_pdu*)initialize->tscf_pdu, AVTP_TSCF_FIELD_STREAM_DATA_LEN,RESPONSE_DATA_LEN);
                if (res < 0)
                        return -1;

        }
        return 0;
}

// Initialization of the CAN pdu
int init_can_pdu(struct can_pdu * pdu)
{
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

void init_queue()
{
        STAILQ_INIT(&samples);                
}

int talker(talker_listener_params_t *talker)
{
        int res;
        ssize_t n;
        uint32_t avtp_time;
        unsigned char out[AES_CMAC_SIZE];
        unsigned char* encrypted_databuff;
        unsigned int nb;
        memset(out, 0x00, AES_CMAC_SIZE);

        if(talker->tscf_on == 1)  
        {
                res = calculate_avtp_time(&avtp_time, MAX_TRANSIT_TIME);
                if (res < 0) 
                {
                        fprintf(stderr, "Failed to calculate avtp time\n");
                        return 1;
                }
                res = avtp_tscf_pdu_set((struct avtp_stream_pdu *)talker->tscf_pdu, AVTP_TSCF_FIELD_TIMESTAMP, avtp_time);
                if (res < 0)
                        return 1;

                res = avtp_tscf_pdu_set((struct avtp_stream_pdu *)talker->tscf_pdu, AVTP_TSCF_FIELD_SEQ_NUM, seq_num);
                if (res < 0)
                        return 1;
                seq_num++;

                memcpy(talker->can_pdu->can_payload,talker->can_payload,PAYLOAD_LEN);
                memcpy((struct avtp_stream_pdu*)talker->tscf_pdu->avtp_payload,talker->can_pdu,CAN_PDU_SIZE);

                aes_cmac((unsigned char*)talker->tscf_pdu, talker->pdu_size, (unsigned char*)out, talker->cmac_key);
                encrypted_databuff = ecb_encrypt((unsigned char*)talker->tscf_pdu, talker->cmac_key, aes_128_encrypt, &nb, talker->pdu_size);

        }
        if(talker->ntscf_on == 1)
        {
                res = avtp_ntscf_pdu_set((struct avtp_ntscf_stream_pdu *)talker->ntscf_pdu, AVTP_NTSCF_FIELD_SEQ_NUM, (seq_num));
                seq_num++;
                if (res < 0)
                        return 1;
                memcpy(talker->can_pdu->can_payload,talker->can_payload,PAYLOAD_LEN);
                memcpy((struct avtp_ntscf_stream_pdu*)talker->ntscf_pdu->avtp_payload,talker->can_pdu,CAN_PDU_SIZE);

                aes_cmac((unsigned char*)talker->ntscf_pdu, talker->pdu_size, (unsigned char*)out, talker->cmac_key);
                encrypted_databuff = ecb_encrypt((unsigned char*)talker->ntscf_pdu, talker->cmac_key, aes_128_encrypt, &nb, talker->pdu_size);
        }
        //        printf("%sAES-128-CMAC Result%s\n", ACCENTCOLOR, DEFAULT);
        //        print_bytes(out, AES_CMAC_SIZE);

        //        printf("%sAES-128-ECB Encrypt Result%s\n", ACCENTCOLOR, DEFAULT);
        //        for (auto i = 0; i < nb; i++) {
        //               print_bytes(encrypted_databuff + i * AES_CMAC_SIZE, AES_CMAC_SIZE);
        //        }
        n = sendto(talker->talker_fd, encrypted_databuff, (nb*AES_CMAC_SIZE), 0, (struct sockaddr *) &talker->sk_addr, sizeof(talker->sk_addr));
        if (n < 0) 
        {
                perror("Failed to send data");
                return 1;
        }

        if (n != (nb*AES_CMAC_SIZE))
        {
                fprintf(stderr, "wrote %zd bytes, expected %zd\n", n, talker->pdu_size);
        }

        return 0;

}
//---------------LISTENER FUNCTIONS------------------
// This function is fine and not to be touched
static int schedule_sample(int fd, struct timespec *tspec, uint8_t *sample)
{
        struct sample_entry *entry;
        entry = (struct sample_entry *)malloc(sizeof(*entry));

        if (!entry) 
        {
                fprintf(stderr, "Failed to allocate memory\n");
                return -1;
        }

        entry->tspec.tv_sec = tspec->tv_sec;
        entry->tspec.tv_nsec = tspec->tv_nsec;
        entry->pdu = (uint8_t*)alloca(num_acfs);
        memcpy(entry->pdu, sample, num_acfs);

        STAILQ_INSERT_TAIL(&samples, entry, entries);
        /* If this was the first entry inserted onto the queue, we need to arm
         * the timer.
         */
        if (STAILQ_FIRST(&samples) == entry) 
        {
                int res;
                res = arm_timer(fd, tspec);
                if (res < 0) 
                {
                        STAILQ_REMOVE(&samples, entry, sample_entry, entries);
                        free(entry);
                        return -1;
                }
        }

        return 0;
}

// Checking validity of the packet
bool is_valid_packet(talker_listener_params_t *valid_packet)
{
        uint64_t val64;
        uint32_t val32;
        int res;
        unsigned char *pdu = (unsigned char *)valid_packet->recv_pdu_buffer;
        // Version check
        res = avtp_pdu_get((struct avtp_common_pdu*)pdu, AVTP_FIELD_VERSION, &val32);
        if (res < 0) 
        {
                fprintf(stderr, "Failed to get version field: %d\n", res);
                return false;
        }
        if (val32 != 0) 
        {
                fprintf(stderr, "Version mismatch: expected %u, got %u\n", 0, val32);
                return false;
        }

        res = avtp_pdu_get((struct avtp_common_pdu*)pdu, AVTP_FIELD_SUBTYPE, &val32);
        if (res < 0) 
        {
                fprintf(stderr, "Failed to get subtype field: %d\n", res);
                return false;
        }

        if(valid_packet->tscf_on == 1)
        {
                if (val32 != AVTP_SUBTYPE_TSCF) 
                {
                        fprintf(stderr, "Subtype mismatch: expected %u, got %u\n", AVTP_SUBTYPE_TSCF, val32);
                        return false;
                }
                // TV bit check
                res = avtp_tscf_pdu_get((struct avtp_stream_pdu*)pdu, AVTP_TSCF_FIELD_TV, &val64);
                if (res < 0)
                {
                        fprintf(stderr, "Failed to get tv field: %d\n", res);
                        return false;
                }

                if (val64 != 1) 
                {
                        fprintf(stderr, "tv mismatch: expected %u, got %" PRIu64 "\n", 1, val64);
                        return false;
                }
                // MR bit check
                res = avtp_tscf_pdu_get((struct avtp_stream_pdu*)pdu, AVTP_TSCF_FIELD_MR, &val64);
                if (res < 0) 
                {
                        fprintf(stderr, "Failed to get mv field: %d\n", res);
                        return false;
                }

                if (val64 != 0) 
                {
                        fprintf(stderr, "mr mismatch: expected %u, got %" PRIu64 "\n", 0, val64);
                        return false;
                }

                // TU bit check
                res = avtp_tscf_pdu_get((struct avtp_stream_pdu*)pdu, AVTP_TSCF_FIELD_TU, &val64);
                if (res < 0) 
                {
                        fprintf(stderr, "Failed to get tu field: %d\n", res);
                        return false;
                }

                if (val64 != 0) 
                {
                        fprintf(stderr, "tu mismatch: expected %u, got %" PRIu64 "\n", 0, val64);
                        return false;
                }
                // Stream id check
                res = avtp_tscf_pdu_get((struct avtp_stream_pdu*)pdu, AVTP_TSCF_FIELD_STREAM_ID, &val64);
                if (res < 0) 
                {
                        fprintf(stderr, "Failed to get stream ID field: %d\n", res);
                        return false;
                }

                if (val64 != STREAM_ID) 
                {
                        fprintf(stderr, "Stream ID mismatch: expected %" PRIu64 ", got %" PRIu64 "\n", STREAM_ID, val64);
                        return false;
                }
                // SEQ NUM CHECK
                res = avtp_tscf_pdu_get((struct avtp_stream_pdu*)pdu, AVTP_TSCF_FIELD_SEQ_NUM, &val64);
                if (res < 0) 
                {
                        fprintf(stderr, "Failed to get sequence num field: %d\n", res);
                        return false;
                }

                if (val64 != expected_seq) 
                {
                        // If there is a sequence number mismatch we are not invalidating the packet
                        fprintf(stderr, "Sequence number mismatch: expected %u, got %" PRIu64 "\n", expected_seq, val64);
                        expected_seq = val64;
                }
                expected_seq++;

                // Stream data len check
                res = avtp_tscf_pdu_get((struct avtp_stream_pdu*)pdu, AVTP_TSCF_FIELD_STREAM_DATA_LEN, &val64);
                if (res < 0) 
                {
                        fprintf(stderr, "Failed to get data_len field: %d\n", res);
                        return false;
                }

                if (val64 != num_acfs) 
                {
                        fprintf(stderr, "Data len mismatch: expected %u, got %" PRIu64 "\n", num_acfs, val64);
                        return false;
                }
        }

        if(valid_packet->ntscf_on == 1)
        {
                if (val32 != AVTP_SUBTYPE_NTSCF) 
                {
                        fprintf(stderr, "Subtype mismatch: expected %u, got %u\n", AVTP_SUBTYPE_NTSCF, val32);
                        return false;
                }
                res = avtp_ntscf_pdu_get((struct avtp_ntscf_stream_pdu*)pdu, AVTP_NTSCF_FIELD_SEQ_NUM, &val64);
                if (res < 0) 
                {
                        fprintf(stderr, "Failed to get sequence num field: %d\n", res);
                        return false;
                }

                if (val64 != expected_seq) 
                {
                        // If there is a sequence number mismatch we are not invalidating the packet
                        fprintf(stderr, "Sequence number mismatch: expected %u, got %" PRIu64 "\n", expected_seq, val64);
                        //expected_seq = val64;
                }
                expected_seq++;

                // Stream data len check
                res = avtp_ntscf_pdu_get((struct avtp_ntscf_stream_pdu*)pdu, AVTP_NTSCF_FIELD_DATA_LEN, &val64);
                if (res < 0)
                {
                        fprintf(stderr, "Failed to get data_len field: %d\n", res);
                        return false;
                }

                /*if (val64 != DATA_LEN_NTSCF) 
                  {
                  fprintf(stderr, "Data len mismatch: expected %lu, got %" PRIu64 "\n", DATA_LEN_NTSCF, val64);
                  return false;
                  }*/
        }
        return true;
}

// Recieve function,
int new_packet(talker_listener_params_t *listener)
{
        int        res;
        uint64_t   avtp_time;
        struct     timespec tspec;
        unsigned char *decrypted_databuff;
        size_t no_of_bytes_recvd;
        uint8_t recv_pdu[MAX_ETH_PYLD_SIZE];
        unsigned int size;

        memset(listener->recv_pdu_buffer, 0, MAX_ETH_PYLD_SIZE);

        if((no_of_bytes_recvd = recv(listener->listener_fd, recv_pdu, MAX_ETH_PYLD_SIZE, 0)) == -1)
        {
                fprintf(stderr, "received : %s (%d)\n", strerror(errno),errno);
        }
        size = no_of_bytes_recvd/AES_CMAC_SIZE;
        decrypted_databuff = ecb_decrypt((unsigned char*)recv_pdu, listener->cmac_key, aes_128_decrypt, &size);
     //   printf("%sAES-128-ECB Decrypt Result%s\n", ACCENTCOLOR, DEFAULT);
     //   printf("bytes received in response = %d\n", no_of_bytes_recvd);
     //   for(int i = 0; i < no_of_bytes_recvd; i++)
     //   {
     //           printf("%x ",decrypted_databuff[i]);
     //   }

        if(listener->tscf_on == 1)
        {
                avtp_tscf_pdu_get((struct avtp_stream_pdu*)decrypted_databuff, AVTP_TSCF_FIELD_STREAM_DATA_LEN, &num_acfs);

                res = avtp_tscf_pdu_get((struct avtp_stream_pdu*)decrypted_databuff, AVTP_TSCF_FIELD_TIMESTAMP, &avtp_time);
                if (res < 0) 
                {
                        fprintf(stderr, "Failed to get AVTP time from PDU\n");
                        return -1;
                }

                // 3.
                res = get_presentation_time(avtp_time, &tspec);
                if (res < 0)
                {
                        printf("Failed to get presentation time-stamp\n");
                        return -1;
                }

                // 4.
                struct avtp_stream_pdu *avtp_pyld = (struct avtp_stream_pdu *)decrypted_databuff;

                res = schedule_sample(listener->timer_fd, &tspec,avtp_pyld->avtp_payload);

                if (res < 0)
                {
                        printf("Failed to schedule sample\n");
                        return -1;
                }
        }
        if(listener->ntscf_on)
        {
                avtp_ntscf_pdu_get((struct avtp_ntscf_stream_pdu*)decrypted_databuff, AVTP_NTSCF_FIELD_DATA_LEN, &num_acfs);
        }

        memcpy(listener->recv_pdu_buffer,(uint8_t *)decrypted_databuff,no_of_bytes_recvd);

        if (!is_valid_packet(listener))
        {
                fprintf(stderr, "Dropping packet\n");
                return 0;
        }
        return 0;
}

int timeout(talker_listener_params_t *listener)
{
        int res;
        ssize_t n;
        uint64_t expirations;
        struct sample_entry *entry;

        n = read(listener->timer_fd, &expirations, sizeof(uint64_t));
        if (n < 0) 
        {
                perror("Failed to read timerfd");
                return -1;
        }
        assert(expirations == 1);
        entry = STAILQ_FIRST(&samples);
        assert(entry != NULL);

        struct can_pdu *pdu = (struct can_pdu*)entry->pdu;
        memcpy(listener->can_payload, pdu->can_payload, PAYLOAD_LEN);

        STAILQ_REMOVE_HEAD(&samples, entries);
        free(entry);

        if (!STAILQ_EMPTY(&samples)) 
        {
                entry = STAILQ_FIRST(&samples);
                res = arm_timer(listener->timer_fd, &entry->tspec);
                if (res < 0)
                        return -1;
        }

        return 0;
}

int listener(talker_listener_params_t *listener)
{
        int  res;
        struct pollfd tscf_fds[2];
        struct pollfd ntscf_fds[2];

        if(listener->ntscf_on == 1)
        {
                ntscf_fds[0].fd = listener->listener_fd;
                ntscf_fds[0].events = POLLIN;
        }

        if(listener->tscf_on == 1)
        {
                tscf_fds[0].fd = listener->listener_fd;
                tscf_fds[0].events = POLLIN;
                tscf_fds[1].fd = listener->timer_fd;
                tscf_fds[1].events = POLLIN;
        }

        while (1) 
        {
                if(listener->ntscf_on == 1)
                {
                        res = poll(ntscf_fds, 1, -1);

                        if (res < 0) 
                        {
                                perror("Failed to poll() fds");
                                return 1;
                        }

                        if (ntscf_fds[0].revents & POLLIN) 
                        {
                                res = new_packet(listener);
                                if (res < 0)
                                        return 1;
                                break;
                        }

                }
                if(listener->tscf_on == 1)
                {
                        res = poll(tscf_fds, 2, -1);
                        if (res < 0) 
                        {
                                perror("Failed to poll() fds");
                                printf("poll error\n");
                                return 1;
                        }

                        if (tscf_fds[0].revents & POLLIN) 
                        {
                                res = new_packet(listener);

                                if (res < 0)
                                {
                                        printf("poll revents error 1 lis_fd \n");
                                        return 1;
                                }
                        }

                        if (tscf_fds[1].revents & POLLIN) 
                        {
                                res = timeout(listener);
                                if (res < 0)
                                {
                                        printf("poll revents error 1 timer_fd\n");
                                        return 1;
                                }
                                break;
                        }
                }
        }
        return 0;
}
