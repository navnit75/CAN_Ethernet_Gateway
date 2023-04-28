#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
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

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void create_packet(void);

int stats_count = 0;
int stats_count1 = 0;

unsigned char sendbuff[16]    = {0x12, 0xAA, 0x34, 0x12, 0x08, 0x00, 0x00, 0x00, 0x07, 0x10, 0xBB, 0x12, 0x34, 0x56, 0x78, 0x90,};
unsigned char sendbuff2[1536] = {0x14, 0xAA, 0x34, 0x12, 0x08, 0x10, 0x20, 0x30, 0x07, 0x10, 0xBB, 0xF1, 0x11, 0x22, 0x33, 0x44,\
                                 0x15, 0xAA, 0x34, 0x12, 0x08, 0x10, 0x20, 0x30, 0x07, 0x11, 0xBB, 0x55, 0x66, 0x77, 0x88, 0x99,\
                                 0x16, 0xAA, 0x34, 0x12, 0x08, 0x10, 0x20, 0x30, 0x07, 0x10, 0xBB, 0x20, 0xAA, 0xBB, 0xCC, 0xDD,\
                                 0x17, 0xAA, 0x34, 0x12, 0x08, 0x10, 0x20, 0x30, 0x07, 0x11, 0xBB, 0xEE, 0xFF, 0xEE, 0xFF, 0xEE};

uint8_t send_once = 0, eth_type_udp = 0, eth_type_tcp = 0;
uint8_t payload_buffer[2000];
uint32_t payload_idx = 0;
uint8_t exit_recv = 0;

uint8_t recv_buff[1536];
struct sockaddr_in address_udp, address_tcp, client_addr;
uint32_t client_addr_len;
int wait_for_reconnect = 0;
int in_reconnect = 0;
int connect_failed = 0;
obd_t obd_packets[BUFF_SIZE];

FILE *rrt_fp;
FILE *stats_fp_tcp;
FILE *stats_fp_udp;

unsigned char key[] = {
        0x31, 0x50, 0x10, 0x47,
        0x17, 0x77, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
};

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

int reconnect(args_t *sock)
{
        int n = 0;
        int num_times_retry = RETRY_TIMES;
        printf("CAN server down\n");
        printf("waiting for re-connection...\n");

        if ((sock->tcp_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
                printf("socket failure\n");
                exit(EXIT_FAILURE);
        }
        do
        {
                n = connect(sock->tcp_fd,(struct sockaddr *)&sock->host_address,sizeof(struct sockaddr_in));
                num_times_retry--;
                sleep(DELAY_BETWEEN_EACH_RETRY);
                if(num_times_retry == 0)
                {
                        break;
                }
        }while(n != 0);

        if (num_times_retry == 0)
        {
                printf("re-connection failed due to timeout\n");
                exit(0);
        }

        printf("connection up\n");
        printf("re-connection successfull\n");
        wait_for_reconnect = 1;
        return 0;
}

void *send_thread(void *new_socket)
{
        uint32_t all_pkts_len = 0, pkt_len = 0, i = 0, j = 0;
        uint32_t resend_len = 0;
        args_t *sock = (args_t*)new_socket;
        uint32_t set_num = 0;
        bool once = true, once1 = true;
        uint16_t p_buf_idx = 0;
        int k = 0;
        unsigned char out[16];
        struct timeval tv;
        unsigned char* encrypted_databuf;
        unsigned int n = 0, m = 0;

        pkt_len = sizeof(sock->obd[k]);
        memset(out, 0x00, 16);	
        
        while(1)
        {
                for(int i = 0; i < 3; i++)
                {
                        if(p_buf_idx <= payload_idx)
                        {
                                sendbuff[13 + i] = payload_buffer[p_buf_idx++];
                        }
                        else
                        {
                                sendbuff[13 + i] = ' ';
                        }
                }

                aes_cmac(sendbuff, 16, (unsigned char*)out, key);
                //printf("%sAES-128-CMAC Result%s\n", ACCENTCOLOR, DEFAULT);
                //print_bytes(out, 16);

                encrypted_databuf = ecb_encrypt(sendbuff, key, aes_128_encrypt, &n, 16);
                //printf("%sAES-128-ECB Encrypt Result%s\n", ACCENTCOLOR, DEFAULT);
                //for (auto i = 0; i < n; i++) {
                //print_bytes(encrypted_databuf + i * 16, 16);
                //}

                if(eth_type_udp)
                {
                        sendto(sock->udp_fd, (unsigned char*)encrypted_databuf, n*16, 0, (struct sockaddr*)&address_udp, client_addr_len);
                }
                else if(eth_type_tcp)
                {
                        send(sock->tcp_fd, (unsigned char*)encrypted_databuf, n*16, 0);
                }
                
                if(once1)
                {
                        once1 = false;
                        gettimeofday(&tv,NULL);
                        sock->single_req = (1000000*tv.tv_sec) + tv.tv_usec;
                }                       

                if(p_buf_idx > payload_idx)
                {
                        if(send_once == 1)
                        {
                                send_once = 0;                                        
                                p_buf_idx = 0;
                                once1 = true;
                                pthread_exit(NULL);
                        }
                        else
                        {
                                p_buf_idx = 0;
                        }
                }

                for(int j = 0; j < 4; j++)
                {
                        for(int i = 0; i < 3; i++)
                        {
                                if(p_buf_idx <= payload_idx)
                                {
                                        sendbuff2[((j + 1) * 13)+ (j * 3) + i] = payload_buffer[p_buf_idx++];
                                }
                                else
                                {
                                        sendbuff2[((j + 1) * 13)+ (j * 3) + i] = ' ';
                                }
                        }
                }

                aes_cmac(sendbuff2, 64, (unsigned char*)out, key);
                // printf("%sAES-128-CMAC Result%s\n", ACCENTCOLOR, DEFAULT);
                // print_bytes(out, 16);

                encrypted_databuf = ecb_encrypt(sendbuff2, key, aes_128_encrypt, &m, 64);
                // printf("%sAES-128-ECB Encrypt Result%s\n", ACCENTCOLOR, DEFAULT);
                // for (auto i = 0; i < m; i++) {
                // print_bytes(encrypted_databuf + i * 16, 16);
                // }
                if(eth_type_udp)
                {
                        sendto(sock->udp_fd, (unsigned char*)encrypted_databuf, m*16, 0, (struct sockaddr*)&address_udp, client_addr_len);
                }
                else if(eth_type_tcp)
                {
                        send(sock->tcp_fd, (unsigned char*)encrypted_databuf, m*16, 0);
                }

                if(once)
                {
                        once = false;
                        gettimeofday(&tv,NULL);
                        sock->multiple_req = (1000000*tv.tv_sec) + tv.tv_usec;
                }

                if(p_buf_idx > payload_idx)
                {
                        if(send_once)
                        {
                                p_buf_idx = 0;
                                send_once = 0;
                                once = true;
                                pthread_exit(NULL);
                        }
                        else
                        {
                                p_buf_idx = 0;
                        }
                }
                usleep(1000000);
        }
        return NULL;
}

void *recv_thread(void *new_socket)
{
        uint32_t i = 0, ack_num;
        args_t *sock = (args_t*)new_socket;
        int n = 0;
        unsigned int nb;
        bool once = true;
        unsigned char *decrypted_databuf;
        struct timeval tv1, tv2;
        
	if(eth_type_udp)
        {
                n = recvfrom(sock->udp_fd, recv_buff, 1536, 0, (struct sockaddr*)&address_udp, &client_addr_len);
                {
                        gettimeofday(&tv1,NULL);
                }

                n = recvfrom(sock->udp_fd, recv_buff, 1536, 0, (struct sockaddr*)&address_udp, &client_addr_len);
                {
                        gettimeofday(&tv2,NULL);

                        sock->single_res = (1000000*tv1.tv_sec) + tv1.tv_usec;
                        sock->multiple_res = (1000000*tv2.tv_sec) + tv2.tv_usec;

                        fprintf(rrt_fp,"Single Frame Round trip time in usecs (non-critical UDP) = %ld\n",(sock->single_res - sock->single_req));
                        fflush(rrt_fp);
			
                        fprintf(stats_fp_udp,"%d Single Frame Round trip time in usecs (non-critical UDP) = %ld\t\t", stats_count++, (sock->single_res - sock->single_req));
                        fflush(stats_fp_udp);

                        fprintf(rrt_fp,"Multiple Frame Round trip time in usecs (non-critical UDP) = %ld\n",(sock->multiple_res - sock->multiple_req));
                        fflush(rrt_fp);

                        fprintf(stats_fp_udp,"%d Multiple Frame Round trip time in usecs (non-critical UDP) = %ld\n", stats_count1++, (sock->multiple_res - sock->multiple_req));
                        fflush(stats_fp_udp);

                        sock->single_res = 0;
                        sock->single_req= 0;
                        sock->multiple_res = 0;
                        sock->multiple_req= 0;
                }

        }
        else if(eth_type_tcp)
        {
                do{
                        n = recv(sock->tcp_fd, recv_buff, 1536, 0);
                }while(n <= 0);

                {
                        gettimeofday(&tv1,NULL);
                }

                do{
                        n = recv(sock->tcp_fd, recv_buff, 1536, 0);
                } while(n <= 0);

                {
                        gettimeofday(&tv2,NULL);

                        sock->multiple_res = (1000000*tv2.tv_sec) + tv2.tv_usec;
                        sock->single_res = (1000000*tv1.tv_sec) + tv1.tv_usec;

                        fprintf(rrt_fp,"Single Frame Round trip time in usecs (non-critical TCP) = %ld\n",(sock->single_res - sock->single_req));
                        fflush(rrt_fp);
                        fprintf(stats_fp_tcp,"%d Single Frame Round trip time in usecs (non-critical TCP) = %ld\t\t", stats_count++, (sock->single_res - sock->single_req));
                        fflush(stats_fp_tcp);

                        fprintf(rrt_fp,"Multiple Frame Round trip time in usecs (non-critical TCP) = %ld\n",(sock->multiple_res - sock->multiple_req));
                        fflush(rrt_fp);
			fprintf(stats_fp_tcp,"%d Multiple Frame Round trip time in usecs (non-critical TCP) = %ld\n", stats_count1++, (sock->multiple_res - sock->multiple_req));
                        fflush(stats_fp_tcp);

                        sock->single_res = 0;
                        sock->single_req= 0;
                        sock->multiple_res = 0;
                        sock->multiple_req= 0;
                }
        }

        // printf("no_of_bytes_recvd = %d\n",nb);
        nb = nb/16;
        decrypted_databuf = ecb_decrypt((unsigned char*)recv_buff, key, aes_128_decrypt, &nb);
        // printf("%sAES-128-ECB Decrypt Result%s\n", ACCENTCOLOR, DEFAULT);

        /*for(int i = 0; i < nb*16; i++)
          {
          printf("%x ",decrypted_databuf[i]);
          }
          printf("\n\n");*/

        while(1)
        {
                if(exit_recv)
                {
                        exit_recv = 0;
                        pthread_exit(NULL);
                }
                if(eth_type_udp)
                {
                        n = recvfrom(sock->udp_fd, recv_buff, 1536, 0, (struct sockaddr*)&address_udp, &client_addr_len);
                }
                else if(eth_type_tcp)
                {
                        n = recv(sock->tcp_fd, recv_buff, 1536, 0);
                        if((n == 0) && (in_reconnect == 0))
                        {
                                close(sock->tcp_fd);
                                connect_failed = 1;
                                wait_for_reconnect = 0;
                                in_reconnect = 1;
                                pthread_cancel(sock->th_id);
                                reconnect(sock);
                                in_reconnect = 0;
                        }
                }
                else
                {
                        continue;
                }
                nb = nb/16;
                decrypted_databuf = ecb_decrypt((unsigned char*)recv_buff, key, aes_128_decrypt, &nb);
        }
        return NULL;
}

int main(int argc, const char *argv[])
{
        uint32_t opt = 1;
        uint32_t server_fd, new_socket;
        uint32_t send_ret, recv_ret;
        args_t send_sock;
        pthread_t send_th, recv_th;
        char c;
        FILE *eth_type_fp;
        FILE *payload_fp;

        if(argc != 5)
        {
                printf("usage : %s <ip_address>\n",argv[0]);
                exit(0);
        }
        
        stats_fp_udp = fopen(argv[3], "w");
        stats_fp_tcp = fopen(argv[4], "w");

        create_packet();	

        while(1)
        {
                eth_type_fp = fopen("/opt/gateway/conf.json", "r");

                if(eth_type_fp == NULL)
                {
                        continue;
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
                printf("UDP Selected\n");
        }

        else
        {
                eth_type_udp = 0;
                eth_type_tcp = 1;
                printf("TCP Selected\n");
        }

        if(eth_type_udp)
        {
                if ((server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
                {
                        perror("socket failed");
                        exit(EXIT_FAILURE);
                }

                address_udp.sin_family      = AF_INET;
                inet_aton(argv[1], &address_udp.sin_addr);
                address_udp.sin_port        = htons(8080);
                client_addr_len = sizeof(client_addr);
                send_sock.udp_fd = server_fd;

        }
        else if(eth_type_tcp)
        {
                if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                {
                        perror("socket failed");
                        exit(EXIT_FAILURE);
                }
                address_tcp.sin_addr.s_addr = inet_addr(argv[1]); //Local Host
                address_tcp.sin_family = AF_INET;
                address_tcp.sin_port = htons(8081);
                connect(server_fd,(struct sockaddr *)&address_tcp,sizeof(struct sockaddr_in));
                send_sock.tcp_fd  = server_fd;
        }
        else
        {
                printf("sock not supported\n");
        }
        send_sock.obd = obd_packets;
        send_sock.host_address = address_tcp;

        while(1)
        {
starting:       payload_idx = 0;
                payload_fp = fopen("/opt/gateway/send.txt", "r+");

                if(payload_fp == NULL)
                {
                        continue;
                }
                c = fgetc(payload_fp);
                if(in_reconnect == 0)
                {
                        printf("Waiting for send signal\n");
                        if(eth_type_tcp)
                        {
                                int clnt_peer_err = getpeername(send_sock.tcp_fd, (struct sockaddr *)&address_tcp, &client_addr_len);
                                if(clnt_peer_err == -1)
                                {
                                        close(send_sock.tcp_fd);
                                        connect_failed = 1;
                                        wait_for_reconnect = 0;
                                        in_reconnect = 1;
                                        reconnect(&send_sock);
                                        in_reconnect = 0;
                                }
                        }
                }

                if(c != 'S') {
                        fclose(payload_fp);
                        sleep(1);
                        continue;
                }

                for(int i = 0; i < 5; i++)
                {
                        c = fgetc(payload_fp);
                }

                rrt_fp= fopen(argv[2], "w");

                if(c == 'O')
                {
                        send_once = 1;
                }

                while(fgetc(payload_fp) != '\n');

                while( (c = fgetc(payload_fp)) != EOF)
                {
                        payload_buffer[payload_idx++] = c;
                }

                printf("Text Read from file\n");

                for(int i = 0; i < payload_idx; i++)
                        printf("%c",(char)payload_buffer[i]);
                printf("\n");

                if(payload_idx == 0)
                        goto starting;

                payload_buffer[payload_idx] = ' '; 

                sleep(1);
                rewind(payload_fp);
                fputc('0',payload_fp);

                if( (recv_ret = pthread_create( &recv_th, NULL, &recv_thread, (void *)&send_sock)) )
                {
                        printf("eth_ecu : Thread creation failed: %d\n", recv_ret);
                }

                sleep(2);

                if( (send_ret = pthread_create( &send_th, NULL, &send_thread, (void *)&send_sock)) )
                {
                        printf("eth_ecu : Send Thread creation failed: %d\n", send_ret);
                }
                send_sock.th_id = send_th;
                pthread_join(send_th, NULL);

                sleep(1);
                if(connect_failed)
                {
                        while(wait_for_reconnect != 1);
                        connect_failed = 0;
                }
                exit_recv = 1;
                fclose(payload_fp);
        }
        return 0;
}

void timeout_function(uint32_t pkts_len)
{	
        uint32_t n = 0;
        n = (pkts_len * 8);
        usleep(n);
}

void create_packet()
{
        obd_packets[0].doip.proto_ver = 0x12;
        obd_packets[0].doip.inv_proto_ver = 0xaa;
        obd_packets[0].doip.payload_type = 0x1234;
        obd_packets[0].doip.payload_len = 8;
        obd_packets[0].uds.pci = 0x07;
        obd_packets[0].uds.sid = 0x10;
        obd_packets[0].uds.sub_fun = 0xbb;
        obd_packets[0].uds.did[0] = 0xf1;
        obd_packets[0].uds.did[1] = 0x11;
        obd_packets[0].uds.data[0] = 0x22;
        obd_packets[0].uds.data[1] = 0x33;
        obd_packets[0].uds.data[2] = 0x44;

        obd_packets[1].doip.proto_ver = 0x13;
        obd_packets[1].doip.inv_proto_ver = 0xaa;
        obd_packets[1].doip.payload_type = 0x1234;
        obd_packets[1].doip.payload_len = 8;
        obd_packets[1].uds.pci = 0x07;
        obd_packets[1].uds.sid = 0x11;
        obd_packets[1].uds.sub_fun = 0xbb;
        obd_packets[1].uds.did[0] = 0x55;
        obd_packets[1].uds.did[1] = 0x66;
        obd_packets[1].uds.data[0] = 0x77;
        obd_packets[1].uds.data[1] = 0x88;
        obd_packets[1].uds.data[2] = 0x99;

        obd_packets[2].doip.proto_ver = 0x14;
        obd_packets[2].doip.inv_proto_ver = 0xaa;
        obd_packets[2].doip.payload_type = 0x1234;
        obd_packets[2].doip.payload_len = 8;
        obd_packets[2].uds.pci = 0x07;
        obd_packets[2].uds.sid = 0x10;
        obd_packets[2].uds.sub_fun = 0xbb;
        obd_packets[2].uds.did[0] = 0x00;
        obd_packets[2].uds.did[1] = 0xaa;
        obd_packets[2].uds.data[0] = 0xbb;
        obd_packets[2].uds.data[1] = 0xcc;
        obd_packets[2].uds.data[2] = 0xdd;

        obd_packets[3].doip.proto_ver = 0x15;
        obd_packets[3].doip.inv_proto_ver = 0xaa;
        obd_packets[3].doip.payload_type = 0x1234;
        obd_packets[3].doip.payload_len = 8;
        obd_packets[3].uds.pci = 0x07;
        obd_packets[3].uds.sid = 0x11;
        obd_packets[3].uds.sub_fun = 0xbb;
        obd_packets[3].uds.did[0] = 0xee;
        obd_packets[3].uds.did[1] = 0xff;
        obd_packets[3].uds.data[0] = 0xee;
        obd_packets[3].uds.data[1] = 0xff;
        obd_packets[3].uds.data[2] = 0xee;

        obd_packets[4].doip.proto_ver = 0x16;
        obd_packets[4].doip.inv_proto_ver = 0xaa;
        obd_packets[4].doip.payload_type = 0x1234;
        obd_packets[4].doip.payload_len = 8;
        obd_packets[4].uds.pci = 0x07;
        obd_packets[4].uds.sid = 0x10;
        obd_packets[4].uds.sub_fun = 0xbb;
        obd_packets[4].uds.did[0] = 0x12;
        obd_packets[4].uds.did[1] = 0x34;
        obd_packets[4].uds.data[0] = 0x56;
        obd_packets[4].uds.data[1] = 0x78;
        obd_packets[4].uds.data[2] = 0x90;
}
