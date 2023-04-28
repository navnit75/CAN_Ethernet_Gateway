#include "critical.h"

static struct avtp_stream_pdu *tscf_pdu;
static struct avtp_ntscf_stream_pdu *ntscf_pdu;
static struct can_pdu *can_pdu;

static uint8_t mac_address[ETH_ALEN];

static talker_listener_params_t talker_listener;

void *recv_request_from_can(void *args)
{
        uint8_t r_ret= 0;
        thread_data_critical_t *thread_data = (thread_data_critical_t *)args;

        while(1)
        {
                if(thread_data->count > BUFF_SIZE_CRITICAL)
                {
                        continue;
                }
                //else
                // {
                if(thread_data->is_new[thread_data->fil_pos] == EMTY)
                {
                        do{
                                r_ret = read(thread_data->driver_fd,thread_data->can_to_eth_buff[thread_data->fil_pos], CAN_PAYLOAD_LEN);
                                usleep(CAN_BUS_DELAY);
                        }while(r_ret == 0);

                        pthread_mutex_lock(&thread_data->lock);
                        thread_data->is_new[thread_data->fil_pos] = FULL;
                        thread_data->fil_pos++;

                        if(thread_data->fil_pos == thread_data->end_pos)
                        {
                                thread_data->fil_pos = thread_data->start_pos;
                        }

                        thread_data->count++;
                        pthread_mutex_unlock(&thread_data->lock);
                        usleep(CAN_BUS_DELAY);
                }
                //}
        }
}

void *forward_request_to_eth_respond_back_to_can(void *args)
{
        uint8_t can_buff[CAN_PAYLOAD_LEN];
        uint8_t writtten_bytes = 0;
        thread_data_critical_t *thread_data = (thread_data_critical_t *)args;
        uint32_t num = 0;

        while(1)
        {
                pthread_mutex_lock(&thread_data->lock);
                if(thread_data->count > 0)
                {
                        if(thread_data->is_new[thread_data->emty_pos] == FULL)
                        {       
                                memcpy(thread_data->talk_lis_data->can_payload, thread_data->can_to_eth_buff[thread_data->emty_pos], CAN_PAYLOAD_LEN);
                                thread_data->is_new[thread_data->emty_pos] = EMTY;
                                thread_data->count--;
                                thread_data->emty_pos++;
                                if(thread_data->emty_pos == thread_data->end_pos)
                                {
                                        thread_data->emty_pos = thread_data->start_pos;
                                }

                                pthread_mutex_unlock(&thread_data->lock);
                                if(talker(thread_data->talk_lis_data))
                                {
                                        printf("Error while accessing talker\n");
                                        close(thread_data->talk_lis_data->talker_fd);
                                }

                                if(listener(thread_data->talk_lis_data))
                                {
                                        printf("error while accessing listener\n");
                                        close(thread_data->talk_lis_data->listener_fd);
                                }

                                memcpy(can_buff, thread_data->talk_lis_data->can_payload, CAN_PAYLOAD_LEN);

                                do{
                                        writtten_bytes = write(thread_data->driver_fd, can_buff, CAN_PAYLOAD_LEN);
                                        usleep(CAN_BUS_DELAY);
                                }while(writtten_bytes == 0);
                        }
                }
                else
                {
                        pthread_mutex_unlock(&thread_data->lock);
                        usleep(CAN_BUS_DELAY);
                }
        }
}

void* create_can2eth_channel_critical(void *args)
{
        uint32_t send_ret, recv_ret;
        int res;
        pthread_t can_req_id, eth_send_id; 
        thread_data_critical_t *thread_data = (thread_data_critical_t *)args;
        char *increment_key;

        if(thread_data->is_tscf)
                talker_listener.tscf_on = 1;

        if(thread_data->is_ntscf)
        {
                talker_listener.ntscf_on = 1;
        }

        sscanf(DESTINATION_MAC_ADDRESS_CAN2ETH, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                        &mac_address[0], &mac_address[1], &mac_address[2],
                        &mac_address[3], &mac_address[4], &mac_address[5]);

        printf("CAN2ETH channel\n");

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

        printf("interface name for can2eth transfer = %s\n", INTERFACE_NAME_CAN2ETH);
        printf("mac_address = ");
        for(int i =0; i <6; i++)
        {
                printf("%X:",mac_address[i]);
        }
        printf("\n");    

        can_pdu = (struct can_pdu*)alloca(CAN_PDU_SIZE);
        talker_listener.can_pdu = can_pdu;

        if(talker_listener.tscf_on)
        {
                tscf_pdu = (struct avtp_stream_pdu*)alloca(PDU_SIZE);
                talker_listener.tscf_pdu = tscf_pdu;
        }
        if(talker_listener.ntscf_on)
        {
                ntscf_pdu = (struct avtp_ntscf_stream_pdu*)alloca(PDU_SIZE);
                talker_listener.ntscf_pdu = ntscf_pdu;
        }

        talker_listener.talker_fd = create_talker_socket(CLASS_PRIORITY);
        if (talker_listener.talker_fd < 0)
        {
                close(talker_listener.talker_fd);
                printf("talker_listener.talker_fd error while creating\n");
                return NULL;
        }

        talker_listener.listener_fd = create_listener_socket(INTERFACE_NAME_CAN2ETH, mac_address, ETH_P_TSN);
        if (talker_listener.listener_fd < 0)
        {
                close(talker_listener.listener_fd);
                printf("talker_listener.listener_fd error while creating\n");
                return NULL;
        }

        res = setup_socket_address(talker_listener.talker_fd, INTERFACE_NAME_CAN2ETH, mac_address, ETH_P_TSN, &talker_listener.sk_addr);
        if (res < 0){
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
                thread_data->driver_fd = open(GW_DEVICE_TSCF_req, O_RDWR);
        }
        if(talker_listener.ntscf_on)
        {
                talker_listener.pdu_size = SINGLE_NTSCF_PDU_SIZE;
                thread_data->driver_fd = open(GW_DEVICE_NTSCF_req, O_RDWR);
        }

        thread_data->talk_lis_data = &talker_listener;

        thread_data->end_pos = BUFF_SIZE_CRITICAL;
        thread_data->count = 0;
        pthread_mutex_init(&thread_data->lock, NULL);

        if( (recv_ret = pthread_create( &can_req_id, NULL, &recv_request_from_can, (void *)thread_data)))
        {
                printf("gw_ecu :Thread creation failed: %d\n", recv_ret);
        }
        if( (send_ret = pthread_create( &eth_send_id, NULL, &forward_request_to_eth_respond_back_to_can, (void *)thread_data)))
        {
                printf("gw_ecu :Thread creation failed: %d\n", send_ret);
        }

        pthread_join(eth_send_id, NULL);
        pthread_join(can_req_id, NULL);   

        return 0;
}
