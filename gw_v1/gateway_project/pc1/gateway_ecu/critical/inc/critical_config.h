#ifndef _CRITICAL_CONFIG_H_
#define _CRITICAL_CONFIG_H_

#define ETH_TO_CAN_BUFF_SIZE 5
#define CAN_TO_ETH_BUFF_SIZE 100
#define BUFF_SIZE_CRITICAL 100
#define CAN_BUS_DELAY_BEFORE_WRITE 5000//85000 //can bus inherent delay (64u) + packet processing delay (26u, approximately)
#define CAN_BUS_DELAY 100//85000 //can bus inherent delay (64u) + packet processing delay (26u, approximately)
#define CAN_BUS_CONS_WRITE_DELAY 20000//85000 //can bus inherent delay (64u) + packet processing delay (26u, approximately)
#define ETH_REQ_TIME_DELAY 2000000
#define READ_TIME_OUT 0x165A0BC00


// CAN dummy driver devices
#define GW_DEVICE_TSCF "/dev/gateway_ecu_dummy_driver_TSCF"
#define GW_DEVICE_TSCF_req "/dev/gateway_ecu_dummy_driver_TSCF_req"
#define GW_DEVICE_NTSCF "/dev/gateway_ecu_dummy_driver_NTSCF"
#define GW_DEVICE_NTSCF_req "/dev/gateway_ecu_dummy_driver_NTSCF_req"

#define CAN_DEVICE_TSCF "/dev/can_ecu_dummy_driver_TSCF"
#define CAN_DEVICE_TSCF_req "/dev/can_ecu_dummy_driver_TSCF_req"
#define CAN_DEVICE_NTSCF "/dev/can_ecu_dummy_driver_NTSCF"
#define CAN_DEVICE_NTSCF_req "/dev/can_ecu_dummy_driver_NTSCF_req"

#define ACCENTCOLOR "\033[1;36m"
#define DEFAULT "\033[0m"
#define STREAM_ID 			0x1232454585957584

// ETH ECU mac addressses
#define  INTERFACE_NAME_ETH2CAN               "enp2s0"
#define  DESTINATION_MAC_ADDRESS_ETH2CAN      "1c:fd:08:71:89:dc"
#define  INTERFACE_NAME_CAN2ETH               "enp3s0"
#define  DESTINATION_MAC_ADDRESS_CAN2ETH      "8c:ec:4b:8d:86:af" 
#define  CLASS_PRIORITY                       -1
#define  MAX_TRANSIT_TIME                      0


//////////////////////////////////////////////////////////////////////
#define NUM_OF_INTERFACES_FOR_ETH2CAN_CHANNELS 1
#define NUM_OF_INTERFACES_FOR_CAN2ETH_CHANNELS 1
#define  INTERFACE_NAME_ETH2CAN_CHANNEL1               "enp1s0"
#define  DESTINATION_MAC_ADDRESS_ETH2CAN_CHANNEL1      "60:18:95:40:c8:02"
#define  INTERFACE_NAME_CAN2ETH_CHANNEL1               "wlp2s0"
#define  DESTINATION_MAC_ADDRESS_CAN2ETH_CHANNEL1      "74:12:b3:19:8d:79" 
///////////////////////////////////////////////////////////////////////////

#define SECURITY_KEY "31:50:10:47:17:00:00:00:00:00:00:00:00:00:00:00"

#endif
