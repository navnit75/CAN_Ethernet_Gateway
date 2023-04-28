#ifndef _NON_CRITICAL_CONFIG_H_ 
#define _NON_CRITICAL_CONFIG_H_

#define BUFF_SIZE 5
#define ETH_TO_CAN_BUFF_SIZE 5
#define CAN_TO_ETH_BUFF_SIZE 100
#define CAN_BUS_DELAY_BEFORE_WRITE 5000//85000 //can bus inherent delay (64u) + packet processing delay (26u, approximately)
#define CAN_BUS_DELAY 100//85000 //can bus inherent delay (64u) + packet processing delay (26u, approximately)


//CAN dummy driver devices
#define GW_DEVICE "/dev/gateway_ecu_dummy_driver"
#define GW_DEVICE1 "/dev/gateway_ecu_dummy_driver1"
#define GW_DEVICE2 "/dev/gateway_ecu_dummy_driver2"
#define GW_DEVICE3 "/dev/gateway_ecu_dummy_driver3"
#define GW_DEVICE4 "/dev/gateway_ecu_dummy_driver4"
#define GW_DEVICE5 "/dev/gateway_ecu_dummy_driver5"
#define GW_DEVICE6 "/dev/gateway_ecu_dummy_driver6"
#define GW_DEVICE7 "/dev/gateway_ecu_dummy_driver7"

#define CAN_DEVICE "/dev/can_ecu_dummy_driver"
#define CAN_DEVICE1 "/dev/can_ecu_dummy_driver1"
#define CAN_DEVICE2 "/dev/can_ecu_dummy_driver2"
#define CAN_DEVICE3 "/dev/can_ecu_dummy_driver3"
#define CAN_DEVICE4 "/dev/can_ecu_dummy_driver4"
#define CAN_DEVICE5 "/dev/can_ecu_dummy_driver5"
#define CAN_DEVICE6 "/dev/can_ecu_dummy_driver6"
#define CAN_DEVICE7 "/dev/can_ecu_dummy_driver7"

#define NUM_OF_ETHERNET_CHANNELS 3
#define NUM_OF_CAN_CHANNELS 3
#define RETRY_TIMES 80
#define DELAY_BETWEEN_EACH_RETRY 1 //provided in seconds

//IP addresses for eth2can transfer
#define IP_ADDR_1  "50.4.2.22"
#define IP_ADDR_2  "60.5.3.42"
#define IP_ADDR_3  "70.6.4.52"
#define IP_ADDR_4  "80.7.6.62"
#define IP_ADDR_5  "90.8.7.72"

// IP addresses for can2eth transfer
#define IP_ADDR_6  "100.9.8.81"
#define IP_ADDR_7  "110.10.9.91"
#define IP_ADDR_8  "120.11.10.101"

#define AES_CMAC_SIZE           16
#define CAN_PAYLOAD_LEN         8
#define MAX_ETH_PYLD_SIZE       1518
#define EMPTY 0
#define FULL  1

#define SINGLE_FRAME_RESPONSE  0
#define FIRST_FRAME            0x10
#define CONSECUTIVE_FRAME      0x20
#define MASK_RESPONSE          0xf0
#define MASK_MULTI_RESPONSE_LEN         0x0f

#define UDP_PORT_ETH2CAN   8080
#define TCP_PORT_ETH2CAN   8081
#define UDP_PORT_CAN2ETH   8082
#define TCP_PORT_CAN2ETH   8083

#define ACCENTCOLOR "\033[1;36m"
#define DEFAULT "\033[0m"

#define SECURITY_KEY "31:50:10:47:17:77:00:00:00:00:00:00:00:00:00:00"

#endif
