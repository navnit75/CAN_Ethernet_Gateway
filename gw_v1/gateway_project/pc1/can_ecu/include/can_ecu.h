
#ifndef _CAN_ECU_H_ 
#define _CAN_ECU_H_

#include <stdint.h>

#define PORT 8080
#define BUFF_SIZE 5
#define CAN_BUS_DELAY_BEFORE_WRITE 5000//85000 //can bus inherent delay (64u) + packet processing delay (26u, approximately)
#define CAN_BUS_DELAY 100//85000 //can bus inherent delay (64u) + packet processing delay (26u, approximately)
#define CAN_BUS_CONS_WRITE_DELAY 20000//85000 //can bus inherent delay (64u) + packet processing delay (26u, approximately)
#define ETH_REQ_TIME_DELAY 2000000
#define READ_TIME_OUT 0x165A0BC00

#define GW_DEVICE "/dev/gateway_ecu_dummy_driver"
#define GW_DEVICE1 "/dev/gateway_ecu_dummy_driver1"
#define GW_DEVICE2 "/dev/gateway_ecu_dummy_driver2"
//#define GW_DEVICE3 "/dev/gateway_ecu_dummy_driver-3"
//#define GW_DEVICE4 "/dev/gateway_ecu_dummy_driver-4"
#define GW_DEVICE5 "/dev/gateway_ecu_dummy_driver5"
#define GW_DEVICE6 "/dev/gateway_ecu_dummy_driver6"
#define GW_DEVICE7 "/dev/gateway_ecu_dummy_driver7"

//#define GW_DEVICE_TSCF "/dev/gateway_ecu_dummy_driver6"
#define GW_DEVICE_TSCF "/dev/gateway_ecu_dummy_driver_TSCF"
#define GW_DEVICE_TSCF_req "/dev/gateway_ecu_dummy_driver_TSCF_req"
#define GW_DEVICE_NTSCF "/dev/gateway_ecu_dummy_driver_NTSCF"
#define GW_DEVICE_NTSCF_req "/dev/gateway_ecu_dummy_driver_NTSCF_req"

#define CAN_DEVICE "/dev/can_ecu_dummy_driver"
#define CAN_DEVICE1 "/dev/can_ecu_dummy_driver1"
#define CAN_DEVICE2 "/dev/can_ecu_dummy_driver2"
//#define CAN_DEVICE3 "/dev/can_ecu_dummy_driver-3"
//#define CAN_DEVICE4 "/dev/can_ecu_dummy_driver-4"
#define CAN_DEVICE5 "/dev/can_ecu_dummy_driver5"
#define CAN_DEVICE6 "/dev/can_ecu_dummy_driver6"
#define CAN_DEVICE7 "/dev/can_ecu_dummy_driver7"
//#define CAN_DEVICE_TSCF "/dev/can_ecu_dummy_driver-6"
#define CAN_DEVICE_TSCF "/dev/can_ecu_dummy_driver_TSCF"
#define CAN_DEVICE_TSCF_req "/dev/can_ecu_dummy_driver_TSCF_req"
#define CAN_DEVICE_NTSCF "/dev/can_ecu_dummy_driver_NTSCF"
#define CAN_DEVICE_NTSCF_req "/dev/can_ecu_dummy_driver_NTSCF_req"

void *recv_from_gw(void *args);
void *send_to_gw(void *args);
void *recv_res_from_gw(void *args);
void *send_req_to_gw(void *args);

#endif
