#include "non_critical.h"
#include "aes.h"
#include "cmac.h"
#include "encrypt.h"
#include "utils_cmac.h"

static doip_t current_doip_header;

int reconnect_server(thread_data_t *thread_data)
{
        int client_len; 
        listen(thread_data->tcp_lisfd, 3);
        printf("ethernet client %d down\n", thread_data->device_num);
        printf("waiting for re-connection...\n");
        client_len = sizeof(struct sockaddr_in);
        //accept connection from an incoming client
        thread_data->tcp_fd = accept(thread_data->tcp_lisfd,(struct sockaddr *)&thread_data->client_addr,(socklen_t*)&client_len);
        if (thread_data->tcp_fd < 0)
        {
                perror("accept failed");
                return -1; 
        }
        printf("re-connection accepted for ethernet request %d\n", thread_data->device_num);
        return 0;
}

/*Threads receiving requests from ETHERNET and sending responses back*/
/* Receive request packets from Ethernet ECU and put them in the ETH-to-CAN Buffer */
void *recv_request_from_eth(void *args)
{
        thread_data_t *thread_data = (thread_data_t *)args;
        unsigned int no_of_bytes_recvd = 0;
        unsigned char temp_buf[MAX_ETH_PYLD_SIZE];
        unsigned char *encrypted_databuff;
        uint32_t serv_addr_len = sizeof(thread_data->serv_addr_udp);

        while(1)
        {
                if(thread_data->count > ETH_TO_CAN_BUFF_SIZE)
                {
                        // no more space for packets
                        continue;
                }
                //else
                //{
                        if(thread_data->is_new_eth[thread_data->fil_pos] == EMPTY)
                        {
                                memset(thread_data->eth_to_can_buff[thread_data->fil_pos], 0, sizeof(thread_data->eth_to_can_buff[thread_data->fil_pos]));

                                if(thread_data->is_proto_udp)
                                {
                                        no_of_bytes_recvd = recvfrom(thread_data->udp_fd, temp_buf, sizeof(thread_data->eth_to_can_buff[thread_data->fil_pos]), 0, (struct sockaddr*)&thread_data->client_addr, &serv_addr_len);

                                }
                                else if(thread_data->is_proto_tcp)
                                {
                                        no_of_bytes_recvd = recv(thread_data->tcp_fd, temp_buf, sizeof(thread_data->eth_to_can_buff[thread_data->fil_pos]), 0);
                                        if(no_of_bytes_recvd == 0)
                                        {
                                                close(thread_data->tcp_fd);
                                                reconnect_server(thread_data);
                                        }
                                }
                                else
                                {
                                        continue;
                                }

                                no_of_bytes_recvd = no_of_bytes_recvd/AES_CMAC_SIZE;
                                encrypted_databuff = ecb_decrypt(temp_buf, thread_data->cmac_key, aes_128_decrypt, &no_of_bytes_recvd);
                                //printf("%sAES-128-ECB Decrypt Result%s\n", ACCENTCOLOR, DEFAULT);
                                //for(int i = 0; i < no_of_bytes_recvd*AES_CMAC_SIZE; i++)
                                //{
                                //printf("%x ",encrypted_databuff[i]);
                                //}
                                //printf("\n\n");

                                memcpy((thread_data->eth_to_can_buff[thread_data->fil_pos]), encrypted_databuff, (no_of_bytes_recvd*AES_CMAC_SIZE));

                                pthread_mutex_lock(&thread_data->buffer_lock);
                                thread_data->is_new_eth[thread_data->fil_pos] = FULL;
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
        return NULL;
}

/* Process the Ethernet requests in the ETH-to-CAN Buffer and send them to CAN ECU */
void *forward_request_to_can(void *args)
{
        thread_data_t *thread_data = (thread_data_t *)args;
        uint32_t nbytes, writtten_bytes;
        uint8_t can_buff[CAN_PAYLOAD_LEN];
        uint8_t temp_buff[MAX_ETH_PYLD_SIZE];
        uint8_t doip_hdr_len = sizeof(thread_data->obd_data->doip);
        // bool once = true;
        // bool once1 = true;

        while(1)
        {
                pthread_mutex_lock(&thread_data->buffer_lock);
                if(thread_data->count > 0)
                {
                        if(thread_data->is_new_eth[thread_data->emty_pos] == FULL)
                        {
                                memset(temp_buff, 0, sizeof(temp_buff));
                                memcpy(temp_buff, thread_data->eth_to_can_buff[thread_data->emty_pos], sizeof(temp_buff));
                                thread_data->obd_data = (obd_t *)temp_buff;

                                thread_data->is_new_eth[thread_data->emty_pos] = EMPTY;
                                thread_data->count--;
                                thread_data->emty_pos++;
                                pthread_mutex_unlock(&thread_data->buffer_lock);
                                nbytes = thread_data->obd_data->doip.payload_len;
                                if(thread_data->emty_pos == thread_data->end_pos)
                                {
                                        thread_data->emty_pos = thread_data->start_pos;
                                }
                                // writing to the CAN Driver block// 

                                if(nbytes > 0)
                                {
                                        while(thread_data->obd_data->doip.proto_ver != 0)
                                        {
                                                memcpy((void *)&current_doip_header, (void *)&thread_data->obd_data->doip, doip_hdr_len);

                                                memset(can_buff, 0, CAN_PAYLOAD_LEN);
                                                memcpy(can_buff, &thread_data->obd_data->uds, CAN_PAYLOAD_LEN);

                                                usleep(CAN_BUS_DELAY_BEFORE_WRITE);
                                                do{
                                                        writtten_bytes = write(thread_data->driver_fd, can_buff, CAN_PAYLOAD_LEN);
                                                        usleep(CAN_BUS_DELAY);
                                                }while(writtten_bytes == 0);

                                                /*if(once)
                                                  {
                                                  once = false;
                                                  gettimeofday(&thread_data->tv,NULL);
                                                  thread_data->res_tm = (1000000*thread_data->tv.tv_sec) + thread_data->tv.tv_usec;
                                                  printf("time while writing to CAN after write = %ld\n",thread_data->res_tm);
                                                  }*/

                                                thread_data->obd_data++;
                                                sem_wait(&thread_data->sem);
                                        }
                                }
                        }
                }
                else
                {
                        pthread_mutex_unlock(&thread_data->buffer_lock);
                        usleep(CAN_BUS_DELAY);
                        //usleep(CAN_BUS_DELAY_BEFORE_WRITE);
                }
        }
        return NULL;	
}

/*read the packets arriving from the CAN ECU */
void *recv_response_from_can(void *args)
{
        thread_data_t *thread_data = (thread_data_t *)args;
        uint32_t r_ret = 0;
        uint8_t buff[CAN_PAYLOAD_LEN];

        while(1)
        {
                do{
                        r_ret = read(thread_data->driver_fd, buff, CAN_PAYLOAD_LEN);
                        usleep(CAN_BUS_DELAY/2);
                }while(r_ret == 0);
                pthread_mutex_lock(&thread_data->read_lock);
                memcpy(&thread_data->read_buff[0], buff, r_ret);
                thread_data->read_bytes = CAN_PAYLOAD_LEN;
                pthread_mutex_unlock(&thread_data->read_lock);
        }
}

/* process the responses from CAN ECU and send them to the Etherent ECU */
void *forward_response_to_eth(void *args)
{
        thread_data_t *thread_data = (thread_data_t *)args;
        uint8_t can_buff[CAN_PAYLOAD_LEN];
        uint8_t ethtx_buf[MAX_ETH_PYLD_SIZE];
        //uint8_t flow_ctl[8] ={0x30, 0x03, 0x00};
        uint8_t doip_hdr_len = 0;
        uint8_t bytes_read = 0; // ret_b = 0;
        uint16_t response_len_uds = 0, multiple_response_pktlen = 0;
        uint16_t ethtx_buff_idx = 0; // flowctl_idx = 0;
        int bytes_sent = 0;
        // bool once = true;
        // bool once1 = true;
        unsigned char out[AES_CMAC_SIZE];
        unsigned int n = 0;
        unsigned char *encrypted_databuff;

        while(1)
        {
                while(1)
                {
                        pthread_mutex_lock(&thread_data->read_lock);
                        if(thread_data->read_bytes)
                        {
                                memcpy(can_buff, &thread_data->read_buff[0], CAN_PAYLOAD_LEN);
                                bytes_read = thread_data->read_bytes;
                                thread_data->read_bytes = 0;
                                pthread_mutex_unlock(&thread_data->read_lock);
                                break;
                        }
                        pthread_mutex_unlock(&thread_data->read_lock);
                        usleep(CAN_BUS_DELAY/2);
                }

                if((can_buff[0] & MASK_RESPONSE) == SINGLE_FRAME_RESPONSE)
                {
                        doip_hdr_len = sizeof(thread_data->obd_data->doip); 
                        memset(ethtx_buf, '\0', sizeof(ethtx_buf));
                        ethtx_buff_idx = 0;
                        memcpy(&ethtx_buf[ethtx_buff_idx], (void *)&current_doip_header , doip_hdr_len);
                        ethtx_buff_idx = doip_hdr_len;
                        memcpy(&ethtx_buf[ethtx_buff_idx], can_buff, bytes_read);

                        aes_cmac(ethtx_buf, (bytes_read + ethtx_buff_idx), (unsigned char*)out, thread_data->cmac_key);
                        //printf("%sAES-128-CMAC Result%s\n", ACCENTCOLOR, DEFAULT);
                        //print_bytes(out, AES_CMAC_SIZE);

                        encrypted_databuff = ecb_encrypt(ethtx_buf, thread_data->cmac_key, aes_128_encrypt, &n, (bytes_read + ethtx_buff_idx));
                        //printf("%sAES-128-ECB Encrypt Result%s\n", ACCENTCOLOR, DEFAULT);
                        //for (auto i = 0; i < n; i++) {
                        //print_bytes(encrypted_databuff + i * AES_CMAC_SIZE, AES_CMAC_SIZE);
                        //}
                        //printf("n = %d\n",n);

                        if(thread_data->is_proto_udp)
                        {
                                sendto(thread_data->udp_fd, encrypted_databuff, n*AES_CMAC_SIZE, 0, (struct sockaddr*)&thread_data->client_addr, sizeof(thread_data->client_addr));
                                sem_post(&thread_data->sem);
                        }
                        else if(thread_data->is_proto_tcp)
                        {
                                /*    if(once)
                                      {
                                      once = false;
                                      gettimeofday(&thread_data->tv,NULL);
                                      thread_data->res_tm = (1000000*thread_data->tv.tv_sec) + thread_data->tv.tv_usec;
                                      printf("time while sending to eth (single-response) = %ld\n",thread_data->res_tm);
                                      }*/

                                bytes_sent = send(thread_data->tcp_fd, encrypted_databuff, n*AES_CMAC_SIZE, 0);
                                if(bytes_sent <= 0)
                                {
                                        reconnect_server(thread_data);
                                }
                                sem_post(&thread_data->sem);

                        }
                }

                else if((can_buff[0] & MASK_RESPONSE) == FIRST_FRAME)
                {
                        response_len_uds = (can_buff[0] & MASK_MULTI_RESPONSE_LEN);
                        response_len_uds = (response_len_uds << 8) | can_buff[1];
                        doip_hdr_len = sizeof(thread_data->obd_data->doip); // + 1;
                        multiple_response_pktlen = response_len_uds + doip_hdr_len + 2;
                        memset(ethtx_buf, '\0', sizeof(ethtx_buf));
                        ethtx_buff_idx = 0;
                        memcpy(&ethtx_buf[ethtx_buff_idx], (void *)&current_doip_header , doip_hdr_len);
                        ethtx_buff_idx = doip_hdr_len;
                        memcpy(&ethtx_buf[ethtx_buff_idx], can_buff, bytes_read);
                        /* usleep(CAN_BUS_DELAY_BEFORE_WRITE);
                           do{
                           ret_b = write(thread_data->driver_fd, flow_ctl, 8);
                           usleep(CAN_BUS_DELAY);
                           }while(ret_b == 0);*/

                        response_len_uds -= 6;
                        ethtx_buff_idx += CAN_PAYLOAD_LEN;
                }

                else if((can_buff[0] & MASK_RESPONSE) == CONSECUTIVE_FRAME)
                {
                        if(response_len_uds > 0)
                        {
                                memcpy(&ethtx_buf[ethtx_buff_idx], (can_buff + 1), (bytes_read - 1));
                                ethtx_buff_idx = ((can_buff[0] & MASK_MULTI_RESPONSE_LEN)*7) + doip_hdr_len + bytes_read;
                                //flowctl_idx++;

                                // send Flow control after every BS size
                                /*if(flowctl_idx == 3)
                                  {

                                  usleep(CAN_BUS_DELAY_BEFORE_WRITE);
                                  do{

                                  ret_b = write(thread_data->driver_fd, flow_ctl, CAN_PAYLOAD_LEN);
                                  usleep(CAN_BUS_DELAY);
                                  }while(ret_b == 0);
                                  flowctl_idx = 0;
                                  }*/
                                response_len_uds -= (bytes_read - 1);
                        }

                        if(response_len_uds == 0)
                        {
                                aes_cmac(ethtx_buf, multiple_response_pktlen, (unsigned char*)out, thread_data->cmac_key);
                                //printf("%sAES-128-CMAC Result%s\n", ACCENTCOLOR, DEFAULT);
                                //print_bytes(out, AES_CMAC_SIZE);

                                encrypted_databuff = ecb_encrypt(ethtx_buf, thread_data->cmac_key, aes_128_encrypt, &n, multiple_response_pktlen);
                                //printf("%sAES-128-ECB Encrypt Result%s\n", ACCENTCOLOR, DEFAULT);
                                //for (auto i = 0; i < n; i++) {
                                //print_bytes(encrypted_databuff + i * AES_CMAC_SIZE, AES_CMAC_SIZE);
                                //}
                                //printf("n = %d\n",n);

                                if(thread_data->is_proto_udp)
                                {
                                        sendto(thread_data->udp_fd, encrypted_databuff, n*AES_CMAC_SIZE, 0, (struct sockaddr*)&thread_data->client_addr, sizeof(thread_data->client_addr));
                                        sem_post(&thread_data->sem);
                                }
                                else if(thread_data->is_proto_tcp)
                                {
                                        /*    if(once1)
                                              {
                                              once1 = false;
                                              gettimeofday(&thread_data->tv,NULL);
                                              thread_data->res_tm = (1000000*thread_data->tv.tv_sec) + thread_data->tv.tv_usec;
                                              printf("time while sending to eth (multi-response) = %ld\n",thread_data->res_tm);
                                              }*/
                                        bytes_sent = send(thread_data->tcp_fd, encrypted_databuff, n*AES_CMAC_SIZE, 0);
                                        if(bytes_sent <= 0)
                                        {
                                                close(thread_data->tcp_fd);
                                                reconnect_server(thread_data);
                                        }
                                        sem_post(&thread_data->sem);
                                }
                        }
                }
                else
                {
                        //dicard
                }
        }
}

void *create_eth_to_can_channel(void *args)
{
        uint32_t send_ret, recv_ret, recv_can;
        int client_len; 
        thread_data_t *thread_data = (thread_data_t *)args;

        pthread_mutex_init(&thread_data->buffer_lock, NULL);
        pthread_mutex_init(&thread_data->read_lock, NULL);
        sem_init(&thread_data->sem, 0, 0);
        thread_data->end_pos = ETH_TO_CAN_BUFF_SIZE;
        thread_data->read_bytes = 0;

        if(thread_data->is_proto_udp)
        {
                if((thread_data->udp_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 1)
                {
                        printf("gw_ecu :\n Error : Could not create UDP socket \n");
                        return NULL;
                }
                memset(&thread_data->serv_addr_udp, '0', sizeof(thread_data->serv_addr_udp)); 
                //get_ip_addr(thread_data->host_address);
                //printf("%s\n",thread_data->host_address);
                thread_data->serv_addr_udp.sin_family = AF_INET;
                thread_data->serv_addr_udp.sin_port = htons(UDP_PORT_ETH2CAN);
                thread_data->serv_addr_udp.sin_addr.s_addr = inet_addr(thread_data->host_address);

                if (bind(thread_data->udp_fd, (struct sockaddr *)&thread_data->serv_addr_udp, sizeof(thread_data->serv_addr_udp)) < 0)
                {
                        perror("bind failed");
                        exit(EXIT_FAILURE);
                }
                printf("ethernet to can channel %d is ready to communicate though udp\n", thread_data->device_num);

        }
        else if(thread_data->is_proto_tcp)
        {
                if((thread_data->tcp_lisfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                {
                        printf("gw_ecu :\n Error : Could not create TCP socket \n");
                        return NULL; 
                }
                memset(&thread_data->serv_addr_tcp, '0', sizeof(thread_data->serv_addr_tcp)); 
                // get_ip_addr(thread_data->host_address);
                // printf("%s\n",thread_data->host_address);
                thread_data->serv_addr_tcp.sin_family = AF_INET;
                thread_data->serv_addr_tcp.sin_addr.s_addr = inet_addr(thread_data->host_address);
                thread_data->serv_addr_tcp.sin_port = htons(TCP_PORT_ETH2CAN);

                if((bind(thread_data->tcp_lisfd, (struct sockaddr *)&thread_data->serv_addr_tcp, sizeof(thread_data->serv_addr_tcp))) < 0)
                {
                        printf("bind failed\n");
                }

                listen(thread_data->tcp_lisfd, 3);
                printf("Waiting for incoming connections...\n");
                client_len = sizeof(struct sockaddr_in);
                //accept connection from an incoming client
                thread_data->tcp_fd = accept(thread_data->tcp_lisfd, (struct sockaddr *)&thread_data->client_addr, (socklen_t*)&client_len);
                if (thread_data->tcp_fd < 0)
                {
                        perror("accept failed");
                        return NULL;
                }
                printf("ethernet to can channel %d is ready to communicate though tcp\n", thread_data->device_num);
        }

        // printf("Select the can interface\n\t1.CAN_IF_1\n\t2.CAN_IF_2\n\t3.CAN_IF_3\n\n");
        // scanf("%hhd",&thread_data->canif_sel);

        if(thread_data->canif_sel == 1)
        {
                thread_data->driver_fd = open(GW_DEVICE, O_RDWR);
                thread_data->lock_num = 0;//thread_data->canif_sel;
        }
        else if(thread_data->canif_sel == 2)
        {
                thread_data->driver_fd = open(GW_DEVICE1, O_RDWR);
                thread_data->lock_num = 1;//thread_data->canif_sel;
        }
        else if(thread_data->canif_sel == 3)
        {
                thread_data->driver_fd = open(GW_DEVICE2, O_RDWR);
                thread_data->lock_num = 2;//;thread_data->canif_sel;
        }
        else
        {
                printf("NO Interface available for selected interface number\n");
                exit(0);
        }

        if( (recv_ret = pthread_create(&thread_data->recv_eth_th_id, NULL, &recv_request_from_eth, (void *)thread_data)))
        {
                printf("gw_ecu :Thread creation failed: %d\n", recv_ret);
        }
        if( (send_ret = pthread_create(&thread_data->send_can_th_id, NULL, &forward_request_to_can, (void *)thread_data)))
        {
                printf("gw_ecu :Thread creation failed: %d\n", send_ret);
        }
        if( (recv_can = pthread_create(&thread_data->recv_from_can_th_id, NULL, &recv_response_from_can, (void *)thread_data)))
        {
                printf("gw_ecu :Thread creation failed: %d\n", recv_can);
        }
        if( (recv_can = pthread_create(&thread_data->send_eth_th_id, NULL, &forward_response_to_eth, (void *)thread_data)))
        {
                printf("gw_ecu :Thread creation failed: %d\n", recv_can);
        }

        pthread_join(thread_data->recv_eth_th_id, NULL);
        pthread_join(thread_data->send_can_th_id, NULL);
        pthread_join(thread_data->send_eth_th_id, NULL);   
        pthread_join(thread_data->recv_from_can_th_id, NULL);
}
