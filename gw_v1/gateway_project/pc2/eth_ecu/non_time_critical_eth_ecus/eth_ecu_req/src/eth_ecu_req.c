#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include "comn.h"
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>
#include <json-c/json.h> 
#include "aes.h"
#include "cmac.h"
#include "encrypt.h"
#include "utils_cmac.h"
#define ACCENTCOLOR "\033[1;36m"
#define DEFAULT "\033[0m"

typedef struct {
        int udp_fd;
        int tcp_fd;
        int tcp_lisfd;
        const char *file_name;
} params_t;

uint8_t ethbuff[1536] = {0xff, 0xfb, 0xfc, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11};
char buffer[1536];
struct sockaddr_in address_udp, address_tcp, client_addr, client_addr_tcp;
uint32_t client_addr_len;
uint8_t cpy_buf[50];
obd_t *obd_packet;
pthread_mutex_t lock_reconn = PTHREAD_MUTEX_INITIALIZER;
int reconnect_success = 0;

FILE *fp;

int eth_type_udp = 0, eth_type_tcp = 0;

uint64_t usecs_req;
uint64_t usecs_res;
unsigned char key[] = {
        0x31, 0x50, 0x10, 0x47,
        0x17, 0x77, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
};
struct timeval tv;
/**************READING JSON FILE **********************/
void read_from_conf_json_file(char *fname,char *buffer){

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

void reconnect(params_t *sock)
{
        int clientLen,sock_fd;
        listen(sock->tcp_lisfd, 3);
        printf("CAN client down\n");
        printf("Waiting for re-connection...\n");
        clientLen = sizeof(struct sockaddr_in);
        //accept connection from an incoming client
        sock->tcp_fd = accept(sock->tcp_lisfd,(struct sockaddr *)&client_addr_tcp,(socklen_t*)&clientLen);
        if (sock->tcp_fd < 0)
        {
                perror("accept failed");
                return;
        }
        printf("re-connection accepted\n");
}

void *recv_thread(void *new_socket)
{
        uint32_t i = 0, ack_num;
        params_t *sock = (params_t *)new_socket;
        unsigned int nb = 0;
        uint16_t num = 0;
        static bool once = true;
        static bool once1 = true;
        unsigned char* encrypted_databuff;
        unsigned char* decrypted_databuff;
        unsigned char out[16];
        FILE *out_fp = fopen(sock->file_name, "w");
        unsigned int n = 0;
        unsigned int m = 0;

        memset(out, 0x00, 16);	
        while(1)
        {
                //memset(buffer, 0, sizeof(buffer));

                if(eth_type_udp)
                {
                        nb = recvfrom(sock->udp_fd, buffer, 1536, 0, (struct sockaddr*)&client_addr, &client_addr_len);
                }
                else if(eth_type_tcp)
                {
here:                   nb = recv(sock->tcp_fd, buffer, 1536, 0);
                        if(nb < 0)
                                continue;
                        if(nb == 0)
                        {
                                close(sock->tcp_fd);
                                reconnect(sock);
                                goto here;
                        }
                }
                else
                {
                        continue;
                }

                nb = nb/16;
                decrypted_databuff = ecb_decrypt((unsigned char*)buffer, key, aes_128_decrypt, &nb);
                //printf("%sAES-128-ECB Decrypt Result%s\n", ACCENTCOLOR, DEFAULT);

                //for(int i = 0; i < nb*16; i++)
                //{
                //        printf("%x ",decrypted_databuff[i]);
                //}
                //printf("\n\n");


                for(int j = 0; j < 3; j++)
                {
                        fprintf(out_fp, "%c",decrypted_databuff[13+j]);
                        fflush(out_fp);
                }
                obd_packet = (obd_t *)buffer;

                memcpy((void *)&obd_packet->uds, ethbuff, 12);

                aes_cmac((unsigned char*)obd_packet, (sizeof(obd_packet->doip) + sizeof(obd_packet->uds) + 4), (unsigned char*)out, key);
                //     printf("%sAES-128-CMAC Result%s\n", ACCENTCOLOR, DEFAULT);
                //     print_bytes(out, 16);

                encrypted_databuff = ecb_encrypt((unsigned char*)obd_packet, key, aes_128_encrypt, &n, (sizeof(obd_packet->doip) + sizeof(obd_packet->uds) + 4));
                //     printf("%sAES-128-ECB Encrypt Result%s\n", ACCENTCOLOR, DEFAULT);
                //     for (auto i = 0; i < n; i++) {
                //             print_bytes(encrypted_databuff + i * 16, 16);
                //     }
                //     printf("n = %d\n",n);

                if(eth_type_udp)
                {
                        sendto(sock->udp_fd, (unsigned char *)encrypted_databuff, n*16, 0, (struct sockaddr*)&client_addr, client_addr_len);
                }
                else if(eth_type_tcp)
                {

                        int peer_name = getpeername(sock->tcp_fd, (struct sockaddr *)&address_tcp, &client_addr_len);
                        if(peer_name == -1)
                        {
                                close(sock->tcp_fd);
                                reconnect(sock);
                        }
                        send(sock->tcp_fd, (unsigned char*)encrypted_databuff, n*16, 0);
                }
        }
        return NULL;
}

int main(int argc, const char *argv[])
{
        uint32_t opt = 1;
        uint32_t server_fd_udp;
        uint32_t send_ret, recv_ret;
        //args_t send_sock;
        params_t send_params;
        pthread_t recv_th;
        FILE *eth_type_fp;
        char c;        
        int tcp_lisfd;

        if(argc != 3)
        {
                printf("usage : %s <ip_address>\n",argv[0]);
                exit(0);
        }

        while(1)
        {
                eth_type_fp = fopen("/opt/gateway/conf.json", "r");

                if(eth_type_fp == NULL)
                {
                        continue;
                        //return(-1);
                }
                c = fgetc(eth_type_fp);
                break;
        }

        char conn_type[5];
        read_from_conf_json_file("/opt/gateway/conf.json",conn_type);
        c = conn_type[0];

        if(c == 'U') 
        {
                eth_type_udp = 1;
                eth_type_tcp = 0;
                printf("UDP selected\n");
        }
        else
        {
                eth_type_udp = 0;
                eth_type_tcp = 1;
                printf("TCP selected\n");
        }

        if(eth_type_udp)
        {
                if ((server_fd_udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
                {
                        perror("socket failed");
                        exit(EXIT_FAILURE);
                }

                address_udp.sin_family      = AF_INET;
                address_udp.sin_addr.s_addr = inet_addr(argv[1]);
                address_udp.sin_port        = htons(8082);
                client_addr_len = sizeof(client_addr);

                send_params.udp_fd = server_fd_udp;

                if (bind(server_fd_udp, (struct sockaddr *)&address_udp, sizeof(address_udp)) < 0)
                {
                        perror("bind failed");
                        exit(EXIT_FAILURE);
                }
        }
        else if(eth_type_tcp)
        {

                if((send_params.tcp_lisfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                {
                        printf("gw_ecu :\n Error : Could not create TCP socket \n");
                        return 1; 
                }

                int clientLen,sock; 
                address_tcp.sin_family = AF_INET;
                address_tcp.sin_addr.s_addr = inet_addr(argv[1]);
                address_tcp.sin_port = htons(8083);

                if((bind(send_params.tcp_lisfd,(struct sockaddr *)&address_tcp,sizeof(address_tcp))) < 0)
                {
                        printf("bind failed\n");
                }
                //	UDP
                if(eth_type_tcp)
                {
                        listen(send_params.tcp_lisfd, 3);
                        printf("Waiting for incoming connections...\n");
                        clientLen = sizeof(struct sockaddr_in);
                        //accept connection from an incoming client
                        sock = accept(send_params.tcp_lisfd,(struct sockaddr *)&client_addr_tcp,(socklen_t*)&clientLen);
                        if (sock < 0)
                        {
                                perror("accept failed");
                                return 1;
                        }
                        printf("Connection accepted\n");
                        send_params.tcp_fd = sock;
                }
        }
        else
        {
                printf("Socket type not supported\n");
        }

        send_params.file_name = argv[2];
        // Creating socket file descriptor

        if( (recv_ret = pthread_create( &recv_th, NULL, &recv_thread, (void *)&send_params)) )
        {
                printf("eth_ecu : Thread creation failed: %d\n", recv_ret);
        }

        pthread_join(recv_th, NULL);

        close(send_params.tcp_lisfd);
        close(server_fd_udp);
        return 0;
}
