#include "non_critical.h"
#include "aes.h"
#include "cmac.h"
#include "encrypt.h"
#include "utils_cmac.h"

static obd_t obd_data_fill;

/* sample doip header */
void get_doip_hdr()
{
        obd_data_fill.doip.proto_ver        = 0x12;
        obd_data_fill.doip.inv_proto_ver    = 0xaa;
        obd_data_fill.doip.payload_type     = 0x1234;
        obd_data_fill.doip.payload_len      = 8;      
}

int reconnect_client(thread_data_t *thread_data)
{
        int retval;
        int num_times_retry = RETRY_TIMES;

        if ((thread_data->tcp_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
                perror("socket failed");
                exit(EXIT_FAILURE);
        }
        printf("ethernet server %d down\n", thread_data->device_num);
        printf("waiting for re-connection...\n");

        do {
                retval = connect(thread_data->tcp_fd, (struct sockaddr *)&thread_data->serv_addr_tcp, sizeof(struct sockaddr_in));
                num_times_retry--;
                sleep(DELAY_BETWEEN_EACH_RETRY);
                if(num_times_retry == 0)
                {
                        printf("re-connection failed due to timeout\n");
                        exit(0);    
                }
        }while(retval != 0);

        printf("connection up\n");
        printf("re-connection accepted for CAN request %d \n", thread_data->device_num);
        return 0;
}

/* thread receiving request from CAN */
void *recv_request_from_can(void *args)
{
        uint8_t r_ret= 0;
        thread_data_t *thread_data = (thread_data_t *)args;

        get_doip_hdr();

        while(1)
        {
                if(thread_data->count > CAN_TO_ETH_BUFF_SIZE)
                {
                        // no more space for packets
                        continue;
                }
                // else
                //{
                if(thread_data->is_new_can[thread_data->fil_pos] == EMPTY)
                {
                        do{
                                r_ret = read(thread_data->driver_fd, thread_data->can_to_eth_buff[thread_data->fil_pos], 8);
                                usleep(CAN_BUS_DELAY);
                        } while(r_ret == 0);

                        pthread_mutex_lock(&thread_data->buffer_lock);
                        thread_data->is_new_can[thread_data->fil_pos] = FULL;
                        thread_data->fil_pos++;

                        if(thread_data->fil_pos == thread_data->end_pos)
                        {
                                thread_data->fil_pos = thread_data->start_pos;
                        }

                        thread_data->count++;
                        pthread_mutex_unlock(&thread_data->buffer_lock);
                }
                //}
        }
}

/* thread to send CAN request to ETHERNET and send reponse back to CAN */
void *forward_request_to_eth_respond_back_to_can(void *args)
{
        thread_data_t *thread_data = (thread_data_t *)args;
        uint8_t can_buff[CAN_PAYLOAD_LEN];
        unsigned int no_of_bytes_recvd, no_of_bytes_sent;
        uint8_t writtten_bytes, remaining_bytes;
        uint8_t send_buff[MAX_ETH_PYLD_SIZE], recv_buff[MAX_ETH_PYLD_SIZE];
        uint16_t buf_idx;
        uint8_t *can_send;
        unsigned char out[AES_CMAC_SIZE];
        unsigned int n = 0;
        unsigned char *encrypted_databuff;
        unsigned char *decrypted_databuff;
        uint32_t serv_addr_len = sizeof(thread_data->serv_addr_udp);

        while(1)
        {
                pthread_mutex_lock(&thread_data->buffer_lock);
                if(thread_data->count > 0)
                {
                        if(thread_data->is_new_can[thread_data->emty_pos] == FULL)
                        {
                                memcpy(send_buff, (void *)&obd_data_fill.doip, sizeof(obd_data_fill.doip));

                                memcpy((void *)&send_buff[sizeof(obd_data_fill.doip)], thread_data->can_to_eth_buff[thread_data->emty_pos], CAN_PAYLOAD_LEN);

                                thread_data->is_new_can[thread_data->emty_pos] = EMPTY;
                                thread_data->count--;
                                thread_data->emty_pos++;

                                pthread_mutex_unlock(&thread_data->buffer_lock);

                                if(thread_data->emty_pos == thread_data->end_pos)
                                {
                                        thread_data->emty_pos = thread_data->start_pos;
                                }

                                /* printf("sendbuff\n");
                                   for(int i = 0; i < pkt_len; i++)
                                   printf("%x  ",sendbuff[i]);
                                   printf("\n\n");*/

                                aes_cmac(send_buff, (CAN_PAYLOAD_LEN + sizeof(obd_data_fill.doip)), (unsigned char*)out, thread_data->cmac_key);
                                //printf("%sAES-128-CMAC Result%s\n", ACCENTCOLOR, DEFAULT);
                                //print_bytes(out, AES_CMAC_SIZE);

                                encrypted_databuff = ecb_encrypt(send_buff, thread_data->cmac_key, aes_128_encrypt, &n, (CAN_PAYLOAD_LEN + sizeof(obd_data_fill.doip)));
                                //printf("%sAES-128-ECB Encrypt Result%s\n", ACCENTCOLOR, DEFAULT);
                                //for (auto i = 0; i < n; i++) {
                                //print_bytes(encrypted_databuff + i * AES_CMAC_SIZE, AES_CMAC_SIZE);
                                //}
                                //printf("n = %d\n",n);

                                if(thread_data->is_proto_udp)
                                {
                                        no_of_bytes_sent = sendto(thread_data->udp_fd, (unsigned char*)encrypted_databuff, (CAN_PAYLOAD_LEN + sizeof(obd_data_fill.doip)), 0, (struct sockaddr*)&thread_data->serv_addr_udp, sizeof(thread_data->client_addr));
                                        memset(recv_buff, 0, MAX_ETH_PYLD_SIZE);
                                        no_of_bytes_recvd = recvfrom(thread_data->udp_fd, recv_buff, sizeof(recv_buff), 0, (struct sockaddr*)&thread_data->serv_addr_udp, &serv_addr_len);
                                }
                                else if(thread_data->is_proto_tcp)
                                {
                                        no_of_bytes_sent = send(thread_data->tcp_fd, (unsigned char*)encrypted_databuff, (CAN_PAYLOAD_LEN + sizeof(obd_data_fill.doip)), 0);
                                        if(no_of_bytes_sent == 0)
                                        {
                                                close(thread_data->tcp_fd);
                                                reconnect_client(thread_data);
                                        }

                                        memset(recv_buff, 0, MAX_ETH_PYLD_SIZE);

                                        no_of_bytes_recvd = recv(thread_data->tcp_fd, recv_buff, sizeof(recv_buff), 0);

                                        if(no_of_bytes_recvd == 0)
                                        {
                                                close(thread_data->tcp_fd);
                                                reconnect_client(thread_data);
                                        }
                                }
                                no_of_bytes_recvd = no_of_bytes_recvd/AES_CMAC_SIZE;
                                decrypted_databuff = ecb_decrypt(recv_buff, thread_data->cmac_key, aes_128_decrypt, &no_of_bytes_recvd);
                                //printf("%sAES-128-ECB Decrypt Result%s\n", ACCENTCOLOR, DEFAULT);
                                //for(int i = 0; i < no_of_bytes_recvd*AES_CMAC_SIZE; i++)
                                //{
                                //printf("%x ",decrypted_databuff[i]);
                                //}
                                //printf("\n\n");

                                can_send = decrypted_databuff;
                                can_send = can_send + CAN_PAYLOAD_LEN;
                                no_of_bytes_recvd = (no_of_bytes_recvd*AES_CMAC_SIZE) - sizeof(obd_data_fill.doip);

                                if(no_of_bytes_recvd > 0)
                                {
                                        buf_idx = 0;	
                                        for(int j = 0; j < (no_of_bytes_recvd / CAN_PAYLOAD_LEN); j++)
                                        {
                                                memset(can_buff, 0, CAN_PAYLOAD_LEN);
                                                memcpy(can_buff, can_send + buf_idx, CAN_PAYLOAD_LEN);
                                                buf_idx += CAN_PAYLOAD_LEN;
                                                do{
                                                        writtten_bytes = write(thread_data->driver_fd, can_buff, CAN_PAYLOAD_LEN);
                                                        usleep(CAN_BUS_DELAY);
                                                }while(writtten_bytes == 0);
                                        }

                                        remaining_bytes = no_of_bytes_recvd % CAN_PAYLOAD_LEN;
                                        if(remaining_bytes > 0)
                                        {
                                                memset(can_buff, 0, CAN_PAYLOAD_LEN);
                                                memcpy(can_buff, can_send + buf_idx, remaining_bytes);
                                                do{
                                                        writtten_bytes = write(thread_data->driver_fd, can_buff, remaining_bytes);
                                                        usleep(CAN_BUS_DELAY);
                                                }while(writtten_bytes == 0);
                                        }
                                }
                        }
                }
                else
                {
                        pthread_mutex_unlock(&thread_data->buffer_lock);
                        usleep(CAN_BUS_DELAY);
                }
        }
}

void *create_can_to_eth_channel(void *args)
{
        uint32_t send_ret, recv_ret;
        int retval;
        thread_data_t *thread_data = (thread_data_t *)args;

        pthread_mutex_init(&thread_data->buffer_lock, NULL);
        sem_init(&thread_data->sem, 0, 0);
        thread_data->read_bytes = 0;
        thread_data->end_pos = CAN_TO_ETH_BUFF_SIZE;

        if(thread_data->is_proto_udp)
        {
                if((thread_data->udp_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 1)
                {
                        printf("gw_ecu :\n Error : Could not create socket \n");
                        return NULL;
                } 

                memset(&thread_data->serv_addr_udp, '0', sizeof(thread_data->serv_addr_udp)); 
                thread_data->serv_addr_udp.sin_family = AF_INET;
                thread_data->serv_addr_udp.sin_port = htons(UDP_PORT_CAN2ETH);
                inet_aton(thread_data->host_address, &thread_data->serv_addr_udp.sin_addr);
                printf("can to ethernet channel %d is ready to communicate through udp\n", thread_data->device_num);             
        }
        else if(thread_data->is_proto_tcp)
        {
                if ((thread_data->tcp_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                {
                        perror("socket failed");
                        exit(EXIT_FAILURE);
                }

                thread_data->serv_addr_tcp.sin_addr.s_addr = inet_addr(thread_data->host_address);
                thread_data->serv_addr_tcp.sin_family = AF_INET;
                thread_data->serv_addr_tcp.sin_port = htons(TCP_PORT_CAN2ETH);

                do {
                        retval = connect(thread_data->tcp_fd,(struct sockaddr *)&thread_data->serv_addr_tcp,sizeof(struct sockaddr_in));
                        usleep(2000);
                }while(retval != 0);

                printf("can to ethernet channel %d is ready to communicate through tcp\n", thread_data->device_num);             
        }

        //  printf("Select the can interface for sending request \n\t6.CAN_IF_6\n\t7.CAN_IF_7\n\t8.CAN_IF_8\n\n");
        //  scanf("%hhd",&thread_data->canif_sel);

        if(thread_data->canif_sel == 6)
        {
                thread_data->driver_fd = open(GW_DEVICE5, O_RDWR);
                thread_data->lock_num = 0;//thread_data->canif_sel;
        }
        else if(thread_data->canif_sel == 7)
        {
                thread_data->driver_fd = open(GW_DEVICE6, O_RDWR);
                thread_data->lock_num = 1;//thread_data->canif_sel;
        }
        else if(thread_data->canif_sel == 8)
        {
                thread_data->driver_fd = open(GW_DEVICE7, O_RDWR);
                thread_data->lock_num = 2;//thread_data->canif_sel;
        }
        else
        {
                printf("NO Interface available for selected interface number\n");
                exit(0);
        }

        if((recv_ret = pthread_create(&thread_data->recv_can_req_id, NULL, &recv_request_from_can, (void *)thread_data)))
        {
                printf("gw_ecu :Thread creation failed: %d\n", recv_ret);
        }
        if((send_ret = pthread_create(&thread_data->send_req_recv_res_id, NULL, &forward_request_to_eth_respond_back_to_can, (void *)thread_data)))
        {
                printf("gw_ecu :Thread creation failed: %d\n", send_ret);
        }

        pthread_join(thread_data->recv_can_req_id, NULL);
        pthread_join(thread_data->send_req_recv_res_id, NULL);
}
