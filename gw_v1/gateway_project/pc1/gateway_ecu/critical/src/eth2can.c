#include "critical.h"

static struct avtp_stream_pdu *tscf_pdu;
static struct avtp_ntscf_stream_pdu *ntscf_pdu;
static struct can_pdu *can_pdu;

static uint8_t mac_address[ETH_ALEN];

static talker_listener_params_t talker_listener;
static thread_data_critical_t thread_data_1;

void *recv_request_from_eth(void *args)
{
        uint32_t n = 0;
        uint8_t temp[MAX_ETH_PYLD_SIZE];
        uint8_t recv_buff[MAX_ETH_PYLD_SIZE];
        thread_data_critical_t *thread_data = (thread_data_critical_t *)args;

        while(1)
        {
                if(thread_data->count > BUFF_SIZE_CRITICAL)
                {
                        continue;
                }
                // else
                // {
                if(thread_data->is_new[thread_data->fil_pos] == EMTY)
                {
                        if(listener(thread_data->talk_lis_data))
                        {
                                printf("error while accessing listener\n");
                                close(thread_data->talk_lis_data->listener_fd);
                        }

                        memcpy(thread_data->eth_to_can_buff[thread_data->fil_pos], thread_data->talk_lis_data->recv_pdu_buffer, MAX_ETH_PYLD_SIZE);
                        //printf("prining inside %s\n", __func__);
                        // for(int i = 0; i < 50; i ++)
                        // {
                        //         printf("%x ", thread_data->talk_lis_data->recv_pdu_buffer[i]);
                        // }
                        // printf("\n");
                        pthread_mutex_lock(&thread_data->lock);
                        thread_data->is_new[thread_data->fil_pos] = FULL;
                        thread_data->fil_pos++;

                        if(thread_data->fil_pos == thread_data->end_pos)
                        {
                                thread_data->fil_pos = thread_data->start_pos;
                        }

                        thread_data->count++;
                        pthread_mutex_unlock(&thread_data->lock);
                }
                //}
        }
        return NULL;
}

void *forward_request_to_can(void *args)
{
        thread_data_critical_t *thread_data = (thread_data_critical_t *)args;
        uint32_t nbytes, j, buf_idx, writtten_bytes, remaining_bytes;
        uint8_t can_buff[CAN_PAYLOAD_LEN];
        uint8_t temp_buff[MAX_ETH_PYLD_SIZE];
        uint16_t strm_pyld_len;
        uint16_t idx = 0;

        while(1)
        {
                pthread_mutex_lock(&thread_data->lock);
                if(thread_data->count > 0)
                {
                        if(thread_data->is_new[thread_data->emty_pos] == FULL)
                        {
                                memset(temp_buff, 0, sizeof(temp_buff));
                                memcpy(temp_buff, thread_data->eth_to_can_buff[thread_data->emty_pos], sizeof(temp_buff));

                                thread_data->is_new[thread_data->emty_pos] = EMTY;
                                thread_data->count--;
                                thread_data->emty_pos++;
                                pthread_mutex_unlock(&thread_data->lock);
                                if(thread_data->emty_pos == thread_data->end_pos)
                                {
                                        thread_data->emty_pos = thread_data->start_pos;
                                }
                                // writing to the CAN Driver block//
                                if(thread_data->talk_lis_data->tscf_on)
                                {
                                        strm_pyld_len = /*(temp_buff[20] << 8) | (temp_buff[21]);*/ CALCULATE_TSCF_PAYLOAD_LEN(temp_buff);
                                        idx = TSCF_ACF_PAYLOAD_START_IDX;
                                }
                                if(thread_data->talk_lis_data->ntscf_on)
                                {
                                        strm_pyld_len = /*((temp_buff[1] & 0x07) << 8) | (temp_buff[2]);*/ CALCULATE_NTSCF_PAYLOAD_LEN(temp_buff);
                                        idx = NTSCF_ACF_PAYLOAD_START_IDX;
                                }
                                while(strm_pyld_len > 0)
                                {
                                        memset(can_buff, 0, CAN_PAYLOAD_LEN);
                                        memcpy(can_buff, temp_buff+idx, CAN_PAYLOAD_LEN);
                                        do{
                                                writtten_bytes = write(thread_data->driver_fd, can_buff, CAN_PAYLOAD_LEN);
                                                usleep(CAN_BUS_DELAY);
                                        }while(writtten_bytes == 0);
                                        idx+=ACF_PACKET_SIZE;
                                        strm_pyld_len -= ACF_PACKET_SIZE;
                                        sem_wait(&thread_data->sem);
                                }
                        }
                }
                else
                {
                        pthread_mutex_unlock(&thread_data->lock);
                        usleep(100);
                }
        }
        return NULL;	
}

void *recv_res_from_can(void *args)
{
        thread_data_critical_t *thread_data = (thread_data_critical_t *)args;
        uint32_t r_ret = 0;
        uint8_t buff[CAN_PAYLOAD_LEN];

        while(1)
        {
                do{
                        r_ret = read(thread_data->driver_fd, buff, CAN_PAYLOAD_LEN);
                        usleep(CAN_BUS_DELAY);
                }while(r_ret == 0);
                memcpy(&thread_data->read_buff[0], buff, r_ret);
                thread_data->read_bytes = r_ret;
        }
}

void *forward_respose_to_eth(void *args)
{
        thread_data_critical_t *thread_data = (thread_data_critical_t *)args;
        uint8_t can_buff[CAN_PAYLOAD_LEN];
        //uint8_t ethtx_buf[MAX_ETH_PYLD_SIZE];
        //uint8_t flctl[8] ={0x30, 0x03, 0x00};
        //uint8_t doip_hdr_len = 0;
        uint8_t rd_bytes = 0; // ret_b = 0;
        //uint16_t res_len = 0, res_len1 = 0, m_pkt_len = 0;
        //uint16_t idx = 0, flidx = 0;

        while(1)
        {
                while(1)
                {
                        if(thread_data->read_bytes)
                        {
                                memcpy(can_buff, &thread_data->read_buff[0], CAN_PAYLOAD_LEN);
                                rd_bytes = thread_data->read_bytes;
                                thread_data->read_bytes = 0;
                                break;
                        }
                        else
                        {
                                usleep(CAN_BUS_DELAY);
                        }
                }
                memcpy(thread_data->talk_lis_data->can_payload, can_buff, CAN_PAYLOAD_LEN);
                if(talker(thread_data->talk_lis_data))
                {
                        printf("Error while accessing talker\n");
                        close(thread_data->talk_lis_data->talker_fd);
                }
                sem_post(&thread_data->sem);
        }
}


void *create_eth2can_channel_critical(void *args)
{
        thread_data_critical_t *thread_data = (thread_data_critical_t *)args;
        uint32_t send_ret, recv_ret, recv_can;
        int res;
        pthread_t send_can_th_id, recv_eth_th_id, send_eth_th_id, recv_can_th_id;
        char *increment_key;

        if(thread_data->is_tscf)
        {
                talker_listener.tscf_on = 1;
        }

        if(thread_data->is_ntscf)
        {
                talker_listener.ntscf_on = 1;
        }

        sscanf(DESTINATION_MAC_ADDRESS_ETH2CAN, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                        &mac_address[0], &mac_address[1], &mac_address[2],
                        &mac_address[3], &mac_address[4], &mac_address[5]);

        printf("ETH2CAN channel\n");
        increment_key = SECURITY_KEY;
        for(int j = 0; j < AES_CMAC_SIZE; j++)
        {
                sscanf(increment_key,"%hhx",&talker_listener.cmac_key[j]);
                increment_key+=3;
        }
        printf("Security key :\n");
        for(int i = 0; i < AES_CMAC_SIZE; i++)
        {
                printf("%X:",talker_listener.cmac_key[i]);
        }
        printf("\n");

        printf("interface name for eth2can transfer= %s\n", INTERFACE_NAME_ETH2CAN);
        printf("mac_address = ");
        for(int i = 0; i < 6; i++)
        {
                printf("%X:",mac_address[i]);
        }
        printf("\n\n");    

        can_pdu = (struct can_pdu*)alloca(CAN_PDU_SIZE);
        talker_listener.can_pdu = can_pdu;

        if(talker_listener.tscf_on)
        {
                tscf_pdu = (struct avtp_stream_pdu*)alloca(MAX_ETH_PYLD_SIZE);
                talker_listener.tscf_pdu = tscf_pdu;
        }
        if(talker_listener.ntscf_on)
        {
                ntscf_pdu = (struct avtp_ntscf_stream_pdu*)alloca(MAX_ETH_PYLD_SIZE);
                talker_listener.ntscf_pdu = ntscf_pdu;
        }

        talker_listener.talker_fd = create_talker_socket(CLASS_PRIORITY);
        if (talker_listener.talker_fd < 0)
        {
                close(talker_listener.talker_fd);
                printf("talker_fd error while creating\n");
                return NULL;
        }

        talker_listener.listener_fd = create_listener_socket(INTERFACE_NAME_ETH2CAN, mac_address, ETH_P_TSN);
        if (talker_listener.listener_fd < 0)
        {
                close(talker_listener.listener_fd);
                printf("listener_fd error while creating\n");
                return NULL;
        }

        res = setup_socket_address(talker_listener.talker_fd, INTERFACE_NAME_ETH2CAN, mac_address, ETH_P_TSN, &talker_listener.sk_addr);
        if (res < 0)
        {
                printf("socket error while setup\n");
                return NULL;
        }

        // Init_PDU call , one function above
        res = init_pdu(&talker_listener);
        if (res < 0){
                printf("TSCF header intialization error\n");
                return NULL;
        }

        res = init_can_pdu(can_pdu);
        if (res < 0){
                printf("Can pdu intialization error\n");
                return NULL;
        }

        if(talker_listener.tscf_on)
        {
                talker_listener.timer_fd = timerfd_create(CLOCK_REALTIME, 0);
                if (talker_listener.timer_fd < 0) {
                        close(talker_listener.timer_fd);
                        printf("timer_fd error while creating\n");
                        return NULL;
                }
                talker_listener.pdu_size = SINGLE_TSCF_PDU_SIZE;
                init_queue();
                thread_data->driver_fd = open(GW_DEVICE_TSCF, O_RDWR);
        }
        if(talker_listener.ntscf_on)
        {
                talker_listener.pdu_size = SINGLE_NTSCF_PDU_SIZE;
                thread_data->driver_fd = open(GW_DEVICE_NTSCF, O_RDWR);
        }

        thread_data->talk_lis_data = &talker_listener;

        thread_data->read_bytes = 0;
        thread_data->emty_pos = 0;
        thread_data->end_pos = BUFF_SIZE_CRITICAL;

        pthread_mutex_init(&thread_data->lock, NULL);
        sem_init(&thread_data->sem, 0, 0);

        if( (recv_ret = pthread_create(&recv_eth_th_id, NULL, &recv_request_from_eth, (void *)thread_data)))
        {
                printf("gw_ecu :Thread creation failed: %d\n", recv_ret);
        }
        if( (send_ret = pthread_create(&send_can_th_id, NULL, &forward_request_to_can, (void *)thread_data)))
        {
                printf("gw_ecu :Thread creation failed: %d\n", send_ret);
        }
        if( (recv_can = pthread_create(&send_eth_th_id, NULL, &forward_respose_to_eth, (void *)thread_data)))
        {
                printf("gw_ecu :Thread creation failed: %d\n", recv_can);
        }
        if( (recv_can = pthread_create(&recv_can_th_id, NULL, &recv_res_from_can, (void *)thread_data)))
        {
                printf("gw_ecu :Thread creation failed: %d\n", recv_can);
        }

        pthread_join(recv_eth_th_id, NULL);
        pthread_join(send_can_th_id, NULL);
        pthread_join(send_can_th_id, NULL);   
        pthread_join(recv_can_th_id, NULL);   

        return 0;
}
