#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h> 
#include <pthread.h>
#include "can_ecu.h"
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>

FILE *rrt_fp; 
FILE *stats_fp; 
int stats_count= 0;

static uint64_t request_time;
uint64_t response_time;
static uint8_t exit_recv = 0;

static struct timeval tv1;
static struct timeval tv2;
static uint8_t send_once = 0, eth_type_udp = 0, eth_type_tcp = 0;
static uint8_t payload_buffer[2000];
static uint16_t file_payld_idx = 0;

static uint8_t send_buffer[2][8] = {{0x07, 0x10, 0xbb, 0xf1, 0x20, 0x01, 0x02, 0x03},
        {0x07, 0x11, 0xbb, 0xf1, 0x20, 0x01, 0x02, 0x03}};

void *recv_res_from_gw(void *args)
{
        int driver_fd = *(int *)args;
        uint32_t rd_ret = 0;
        uint8_t recv_buffer[8];

        do{
                rd_ret = read(driver_fd, recv_buffer, 8);
                usleep(CAN_BUS_DELAY);
        }while(rd_ret == 0);

        gettimeofday(&tv2,NULL);
        request_time = (1000000*tv1.tv_sec) + tv1.tv_usec;
        response_time = (1000000*tv2.tv_sec) + tv2.tv_usec;
        fprintf(rrt_fp,"Round trip time in usecs (critical(ntscf)) = %ld\n",(response_time - request_time));
        fflush(rrt_fp);
        fprintf(stats_fp,"%d Round trip time in usecs(critical(ntscf)) = %ld\n", stats_count++, (response_time - request_time));
        fflush(stats_fp);
        request_time = 0;
        response_time = 0;

        while(1)
        {
                if(exit_recv)
                {
                        exit_recv = 0;
                        pthread_exit(NULL);
                }
                do{
                        rd_ret = read(driver_fd, recv_buffer, 8);
                        usleep(CAN_BUS_DELAY);
                }while(rd_ret == 0);
        }
}

void *send_req_to_gw(void *args)
{
        int driver_fd = *(int *)args;
        int wr_ret = 0;
        uint16_t send_payld_idx = 0;
        bool once1 = true;

        while(1)
        {
                for(int i = 0; i < 8; i++)
                {
                        if(send_payld_idx <= file_payld_idx)
                        {
                                send_buffer[0][i] = payload_buffer[send_payld_idx++];
                        }
                        else
                        {
                                send_buffer[0][i] = ' ';
                        }
                }

                do {
                        wr_ret = write(driver_fd, send_buffer[0], 8);
                        usleep(CAN_BUS_DELAY);
                }while(wr_ret == 0);

                if(once1)
                {
                        once1 = false;
                        gettimeofday(&tv1,NULL);
                }   

                if(send_payld_idx > file_payld_idx)
                {
                        if(send_once == 1)
                        {
                                send_payld_idx = 0;
                                send_once = 0;
                                pthread_exit(NULL);
                        }
                        else
                        {
                                send_payld_idx = 0;
                        }
                }
                usleep(200);
        }
}

int main(int argc, const char *argv[])
{
        int driver_fd;
        int send_ret, recv_ret;
        pthread_t recv_th_id, read_th_id;
        FILE *payload_fp;     
        char c;
        if(argc != 2)
        {
                printf("Usage ./%s stats/filename.log",argv[0]);
        }
        driver_fd = open(CAN_DEVICE_NTSCF_req, O_RDWR);
        stats_fp = fopen(argv[1],"w");

        while(1)
        {
starting:       file_payld_idx = 0;

                do {
                        payload_fp = fopen("/opt/gateway/send.txt", "r+");

                        if(payload_fp == NULL){
                                continue;
                        }

                        c = fgetc(payload_fp);
                        if(c != 'S') {
                                fclose(payload_fp);
                                sleep(1);
                                continue;
                        }

                        for(int i = 0; i < 5; i++)
                        {
                                c = fgetc(payload_fp);
                        }
                        break;

                } while(1);

                if(c == 'O')
                {
                        send_once = 1;
                }

                rrt_fp = fopen("/opt/gateway/can_c_rt.txt", "w");
               
                while(fgetc(payload_fp) != '\n');

                while( (c = fgetc(payload_fp)) != EOF)
                {
                        payload_buffer[file_payld_idx++] = c;
                }

                if(file_payld_idx == 0)
                {
                        goto starting;
                }

                payload_buffer[file_payld_idx] = ' '; 
                sleep(1);
                rewind(payload_fp);
                fputc('0',payload_fp);

                if( (recv_ret = pthread_create( &read_th_id, NULL, &recv_res_from_gw, (void *)&driver_fd)))
                {
                        printf("can_ecu : Read Thread creation failed: %d\n", recv_ret);
                }
                if( (recv_ret = pthread_create( &recv_th_id, NULL, &send_req_to_gw, (void *)&driver_fd)))
                {
                        printf("can_ecu : Receive Thread creation failed: %d\n", recv_ret);
                }


                pthread_join(recv_th_id, NULL);

                sleep(1);
                exit_recv = 1;
                //pthread_join(read_th_id, NULL);
                fclose(payload_fp);
        }
        return 0;
}

