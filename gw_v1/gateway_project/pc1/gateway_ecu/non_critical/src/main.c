#include <json-c/json.h>
#include "non_critical.h"

thread_data_t eth_to_can_thread_info[NUM_OF_ETHERNET_CHANNELS];
thread_data_t can_to_eth_thread_info[NUM_OF_CAN_CHANNELS];


/**************READING JSON FILE **********************/
void read_from_conf_json_file(char *fname,char *buffer)
{    
        /* buffer for json read */ 
        char buffer_json[1024];

        /* temp struct to store the file and parsed file */ 
        struct json_object *parsed_json; 
        struct json_object *connection_type; 

        /* reading the file */ 
        FILE *fptr; 
        fptr = fopen(fname,"r");
        if(fptr == NULL){
                printf("Cannot open file %s \n",fname);
                exit(0); 
        }

        fread(buffer_json,1024,1,fptr);
        fclose(fptr);

        /* parse the data read from file */ 
        parsed_json = json_tokener_parse(buffer_json);
        json_object_object_get_ex(parsed_json,"connection_type",&connection_type);

        /* copy into provided arguments */ 
        memcpy(buffer,json_object_get_string(connection_type),5);
}


int main(int argc, char *argv[])
{
        FILE *eth_type_fp;
        int ret_val = 0;
        char *ip_addresses_server[NUM_OF_ETHERNET_CHANNELS];        
        char *ip_addresses_client[NUM_OF_CAN_CHANNELS];
        char conn_type[5];

        char *increment_key;
        unsigned char cmac_key[16];
        bool eth_type_udp = false;
        bool eth_type_tcp = false;

        pthread_t eth_to_can_channel_id[NUM_OF_ETHERNET_CHANNELS];
        pthread_t can_to_eth_channel_id[NUM_OF_CAN_CHANNELS];

        ip_addresses_server[0] = IP_ADDR_1;
        ip_addresses_server[1] = IP_ADDR_2;
        ip_addresses_server[2] = IP_ADDR_3;
        ip_addresses_client[0] = IP_ADDR_6;
        ip_addresses_client[1] = IP_ADDR_7;
        ip_addresses_client[2] = IP_ADDR_8;

        eth_type_fp = fopen("/opt/gateway/conf.json", "r");

        if(eth_type_fp == NULL)
        {
                printf("conf.json not found\n");
                exit(0);
        }

        read_from_conf_json_file("/opt/gateway/conf.json",conn_type);

        if(conn_type[0] == 'U') 
        {
                eth_type_tcp = false;
                eth_type_udp = true;
                printf("UDP selected\n");
        }
        else if(conn_type[0] == 'T')
        {
                eth_type_udp = false;
                eth_type_tcp = true;
                printf("TCP selected\n");
        }
        else
        {
                printf("The connection type specified is not supported\n");
                exit(0);
        }


        for(int i = 0; i < NUM_OF_ETHERNET_CHANNELS; i++)
        {
                eth_to_can_thread_info[i].canif_sel = i + 1;                // i is selected because no i/p is taken from user at the moment
                eth_to_can_thread_info[i].device_num = i + 1;
                eth_to_can_thread_info[i].host_address = ip_addresses_server[i];  // ip address is static, no user selection option is on at the moment
                eth_to_can_thread_info[i].is_proto_udp = eth_type_udp;
                eth_to_can_thread_info[i].is_proto_tcp = eth_type_tcp;
                increment_key = SECURITY_KEY;
                for(int j = 0; j < 16; j++)
                {
                        sscanf(increment_key,"%hhx",&eth_to_can_thread_info[i].cmac_key[j]);
                        increment_key+=3;
                }

                if((ret_val = pthread_create(&eth_to_can_channel_id[i], NULL, &create_eth_to_can_channel, (void *)&eth_to_can_thread_info[i])))
                {
                        printf("gw_ecu :Thread creation failed: %d\n", ret_val);
                }
                usleep(100000);
        }

        for(int i = 0; i < NUM_OF_CAN_CHANNELS; i++)
        {
                can_to_eth_thread_info[i].canif_sel = i + 6;                // i is selected because no i/p is taken from user at the moment
                can_to_eth_thread_info[i].device_num = i + 6;
                can_to_eth_thread_info[i].host_address = ip_addresses_client[i];  // ip address is static, no user selection option is on at the moment
                can_to_eth_thread_info[i].is_proto_udp = eth_type_udp;
                can_to_eth_thread_info[i].is_proto_tcp = eth_type_tcp;
                increment_key = SECURITY_KEY;

                for(int j = 0; j < 16; j++)
                {
                        sscanf(increment_key,"%hhx",&can_to_eth_thread_info[i].cmac_key[j]);
                        increment_key+=3;
                }

                if((ret_val = pthread_create(&can_to_eth_channel_id[i], NULL, &create_can_to_eth_channel, (void *)&can_to_eth_thread_info[i])))
                {
                        printf("gw_ecu :Thread creation failed: %d\n", ret_val);
                }
                usleep(100000);
        }

        for(int i = 0; i < 3; i++)
        {
                pthread_join(eth_to_can_channel_id[i], NULL);        
                pthread_join(can_to_eth_channel_id[i], NULL);
        }

        return 0;
}

int get_ip_addr(char *ipaddr)
{
        int i = 0;
        int user_selction = 0;
        char user_ip[10][100];

        struct ifaddrs *addresses;

        if (getifaddrs(&addresses) == -1)
        {
                printf("getifaddrs call failed\n");
                return -1;
        }

        struct ifaddrs *address = addresses;

        printf("Please select interface name for gateway socket creation\n");

        while(address)
        {
                int family = address->ifa_addr->sa_family;
                if ((family == AF_INET || family == AF_INET6))
                {
                        if((family == AF_INET) && (strcmp(address->ifa_name, "lo")))
                        {
                                printf("%d : %s\t", ++i, address->ifa_name);
                                printf("%s\t", family == AF_INET ? "IPv4" : "IPv6");

                                char ap[100];
                                const int family_size = family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
                                getnameinfo(address->ifa_addr,family_size, ap, sizeof(ap), 0, 0, NI_NUMERICHOST);
                                strcpy(user_ip[i] , ap);
                                printf("\t%s\n", ap);
                        }
                }
                address = address->ifa_next;
        }

        scanf("%d", &user_selction);
        printf("\nyou have selected %s IP address\n",user_ip[user_selction]);
        printf("Please provide the selected IP address at the ethernet ECU side as an input to connect to the gateway\n");
        strcpy(ipaddr, user_ip[user_selction]);
        freeifaddrs(addresses);

        return 0;
}

