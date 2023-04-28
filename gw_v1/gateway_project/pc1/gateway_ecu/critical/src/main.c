#include "critical.h"
#include <json-c/json.h>

static thread_data_critical_t eth2can_channel_info;
static thread_data_critical_t can2eth_channel_info;

/**************READING JSON FILE **********************/
void read_from_conf_json_file(char *fname,char *buffer1, char *buffer2)
{    
        /* buffer for json read */ 
        char buffer_json[1024];

        /* temp struct to store the file and parsed file */ 
        struct json_object *parsed_json; 
        struct json_object *connection_type1; 
        struct json_object *connection_type2; 

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
        json_object_object_get_ex(parsed_json,"interface_1_eth2can",&connection_type1);
        json_object_object_get_ex(parsed_json,"interface_2_can2eth",&connection_type2);

        /* copy into provided arguments */ 
        memcpy(buffer1,json_object_get_string(connection_type1),5);
        memcpy(buffer2,json_object_get_string(connection_type2),5);
}

int main(int argc, char *argv[])
{
        FILE *eth_type_fp;
        char conn_type1[5];
        char conn_type2[5];
        pthread_t eth2can_channel_id;
        pthread_t can2eth_channel_id;

        bool tscf_selected_eth2can  = false;
        bool ntscf_selected_eth2can = false;
        bool tscf_selected_can2eth  = false;
        bool ntscf_selected_can2eth = false;

        eth_type_fp = fopen("/opt/gateway/conf.json", "r");
        if(eth_type_fp == NULL)
        {
                printf("conf.json not found\n");
                exit(0);
        }

        read_from_conf_json_file("/opt/gateway/conf.json",conn_type1, conn_type2);

        if(conn_type1[0] == 'T') 
        {
                tscf_selected_eth2can  = true;
        }
        else if(conn_type1[0] == 'N')
        {
                ntscf_selected_eth2can = true;
        }
        else
        {
                printf("The connection type specified is not supported\n");
                exit(0);
        }
        printf("%s selected for eth2can transfer\n",conn_type1);

        if(conn_type2[0] == 'T') 
        {
                tscf_selected_can2eth  = true;
        }
        else if(conn_type2[0] == 'N')
        {
                ntscf_selected_can2eth = true;
        }
        else
        {
                printf("The connection type specified is not supported\n");
                exit(0);
        }
        printf("%s selected for can2eth transfer\n\n",conn_type2);

        eth2can_channel_info.is_tscf = tscf_selected_eth2can;
        eth2can_channel_info.is_ntscf = ntscf_selected_eth2can;
        can2eth_channel_info.is_tscf = tscf_selected_can2eth;
        can2eth_channel_info.is_ntscf = ntscf_selected_can2eth;

        pthread_create(&eth2can_channel_id, NULL, &create_eth2can_channel_critical, (void *)&eth2can_channel_info);
        sleep(1);
        pthread_create(&can2eth_channel_id, NULL, &create_can2eth_channel_critical, (void *)&can2eth_channel_info);

        pthread_join(eth2can_channel_id, NULL);
        pthread_join(can2eth_channel_id, NULL);
}
