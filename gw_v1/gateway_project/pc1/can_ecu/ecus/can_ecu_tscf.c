#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h> 
#include <pthread.h>
#include "can_ecu.h"
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static uint8_t read_buff[8];
static uint32_t read_bytes;
struct timeval tv;

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
        int fd = *(int *)args;
        uint32_t r_ret = 0;
        uint8_t buff[8];
        uint64_t read_timeout;
        FILE *out_fp = fopen("/opt/gateway/can_tscf.txt", "a");

        while(1)
        {
                do{
                        r_ret = read(fd, buff, 8);
                        usleep(CAN_BUS_DELAY);
                }while((r_ret == 0));

/*                printf("read from gw\n");
                for(int i = 0; i < 8; i++)
                        printf("%X ", buff[i]);

                printf("\n");

*/
                for(int i = 0; i < 8; i++)
                {
                        fprintf(out_fp, "%c",(char)buff[i]);

                        fflush(out_fp);
                }

                pthread_mutex_lock(&lock);
                memcpy(read_buff, buff, 8);
                read_bytes = r_ret;
                pthread_mutex_unlock(&lock);
        }
}

void *send_to_gw(void *args)
{
        uint8_t buff[8];
        int fd = *(int *)args;
        int r_ret = 0, w_ret = 0;
        int idx = 0;
        
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
                        usleep(CAN_BUS_DELAY);
                }

               // usleep(CAN_BUS_DELAY_BEFORE_WRITE);
                
                do {
                        w_ret = write(fd, buffer[1], 8);
                        usleep(CAN_BUS_DELAY);
                }while(w_ret == 0);

        }
}


int main(int argc, const char *argv[])
{
        int fd;
        int send_ret, recv_ret;
        pthread_t send_th_id, recv_th_id;

        pthread_mutex_init(&lock, NULL);

        fd = open(CAN_DEVICE_TSCF, O_RDWR);

        if( (recv_ret = pthread_create( &send_th_id, NULL, &send_to_gw, (void *)&fd)))
        {
                printf("can_ecu : Receive Thread creation failed: %d\n", recv_ret);
        }

        if( (recv_ret = pthread_create( &recv_th_id, NULL, &recv_from_gw, (void *)&fd)))
        {
                printf("can_ecu : Read Thread creation failed: %d\n", recv_ret);
        }

        pthread_join(send_th_id, NULL);
        pthread_join(recv_th_id, NULL);

        return 0;
}

