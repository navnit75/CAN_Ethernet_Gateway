#include<stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h> 
#include <pthread.h>
#include "can_ecu.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static uint8_t read_buff[8];
static uint32_t read_bytes;

uint8_t buffer[2][100] = {{0x07, 0x50, 0xbb, 0xf1, 0x20, 0x02, 0x03, 0x04},
        {0x10, 0x45, 0x51, 0xaa, 0xbb, 0xff, 0xff, 0xff,

                0x21, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
                0x22, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
                0x23, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,

                0x24, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
                0x25, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
                0x26, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,

                0x27, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
                0x28, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
                0x29, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee}}; 

void *recv_from_gw(void *args)
{
        int driver_fd = *(int *)args;
        uint32_t rd_ret = 0;
        uint8_t buff[8];
        uint64_t read_timeout;
        FILE *out_fp = fopen("/opt/gateway/can3.txt", "a");

        while(1)
        {
                do{
                        rd_ret = read(driver_fd, buff, 8);
                        usleep(CAN_BUS_DELAY/2);
                }while(rd_ret == 0);


                for(int i = 0; i < 3; i++)
                {
                        fprintf(out_fp, "%c",(char)buff[5+i]);
                        fflush(out_fp);
                }
                pthread_mutex_lock(&lock);
                memcpy(read_buff, buff, 8);
                read_bytes = rd_ret;
                pthread_mutex_unlock(&lock);
        }
}

void *send_to_gw(void *args)
{
        uint8_t buff[8];
        int driver_fd = *(int *)args;
        int wr_ret = 0;
        int buffer_idx = 0;

        while(1)
        {
                memset(buff, 0, 8);

                while(1)
                {
                        pthread_mutex_lock(&lock);
                        if(read_bytes)
                        {
                                memcpy(buff, read_buff, read_bytes);
                                read_bytes = 0;
                                pthread_mutex_unlock(&lock);
                                break;
                        }
                        pthread_mutex_unlock(&lock);
                        usleep(CAN_BUS_DELAY/2);
                }

                if(buff[1] == 0x10)
                {
                        usleep(CAN_BUS_DELAY_BEFORE_WRITE);
                        do {
                                wr_ret = write(driver_fd, buffer[0], 8);
                                usleep(CAN_BUS_DELAY);
                        }while(wr_ret == 0);
                }

                else if(buff[1] == 0x11)
                {

                       usleep(CAN_BUS_DELAY_BEFORE_WRITE);
                        buffer_idx = 0;
                        //sending response packets till BC size
                        do {
                                wr_ret = write(driver_fd, (&buffer[1][0] + buffer_idx), 8);
                                usleep(CAN_BUS_DELAY);
                        }while(wr_ret == 0);
                        buffer_idx += 8;
                        //receiving flow control packet after BC size

                     /*   while(1)
                        {
                                pthread_mutex_lock(&lock);
                                if(read_bytes)
                                {
                                        memcpy(buff, read_buff, read_bytes);
                                        read_bytes = 0;
                                        pthread_mutex_unlock(&lock);
                                        break;
                                }
                                pthread_mutex_unlock(&lock);
                                usleep(CAN_BUS_DELAY);
                        }*/

                        for(int j = 0; j < 3; j++)
                        {
                                for(int i = 0; i < 3; i++)
                                {

                                        usleep(CAN_BUS_CONS_WRITE_DELAY);
                                        do {
                                                wr_ret = write(driver_fd, (&buffer[1][0] + buffer_idx), 8);
                                                usleep(CAN_BUS_DELAY);
                                        }while(wr_ret == 0);
                                        buffer_idx += 8;
                                }

                                //receiving flow control packet after BC size

                                /*while(1)
                                {
                                        pthread_mutex_lock(&lock);
                                        if(read_bytes)
                                        {
                                                memcpy(buff, read_buff, read_bytes);
                                                read_bytes = 0;
                                                pthread_mutex_unlock(&lock);
                                                break;
                                        }
                                        pthread_mutex_unlock(&lock);
                                        usleep(CAN_BUS_DELAY);
                                }*/


                        }
                }
        }
}



int main(int argc, const char *argv[])
{
        int driver_fd;
        int send_ret, recv_ret;
        pthread_t send_th_id, recv_th_id, read_th_id;

        pthread_mutex_init(&lock, NULL);

        driver_fd = open(CAN_DEVICE2, O_RDWR);

        if( (recv_ret = pthread_create( &recv_th_id, NULL, &send_to_gw, (void *)&driver_fd)))
        {
                printf("can_ecu : Receive Thread creation failed: %d\n", recv_ret);
        }

        if( (recv_ret = pthread_create( &read_th_id, NULL, &recv_from_gw, (void *)&driver_fd)))
        {
                printf("can_ecu : Read Thread creation failed: %d\n", recv_ret);
        }

        pthread_join(recv_th_id, NULL);
        pthread_join(read_th_id, NULL);

        return 0;
}



