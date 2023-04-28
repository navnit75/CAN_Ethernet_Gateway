cd ../pc1/can_ecu
gcc -I./include -pthread ecus/can_ecu1.c -o output/can_ecu1
gcc -I./include -pthread ecus/can_ecu2.c -o output/can_ecu2
gcc -I./include -pthread ecus/can_ecu3.c -o output/can_ecu3
#gcc -I./include -pthread ecus/can_ecu4.c -o output/can_ecu4
#gcc -I./include -pthread ecus/can_ecu5.c -o output/can_ecu5
gcc -I./include -pthread ecus/can_ecu_tscf.c -o output/can_ecu_tscf
gcc -I./include -pthread ecus/can_ecu_tscf_req.c -o output/can_ecu_tscf_req
gcc -I./include -pthread ecus/can_ecu_ntscf.c -o output/can_ecu_ntscf
gcc -I./include -pthread ecus/can_ecu_ntscf_req.c -o output/can_ecu_ntscf_req

gnome-terminal -t "CAN_ECU_NON_CRITICAL_1" --command="bash -c './output/can_ecu1; $SHELL'"
gnome-terminal -t "CAN_ECU_NON_CRITICAL_2" --command="bash -c './output/can_ecu2; $SHELL'"
gnome-terminal -t "CAN_ECU_NON_CRITICAL_3" --command="bash -c './output/can_ecu3; $SHELL'"
#gnome-terminal -t "CAN_ECU_NON_CRITICAL_4" --command="bash -c './output/can_ecu4; $SHELL'"
#gnome-terminal -t "CAN_ECU_NON_CRITICAL_5" --command="bash -c './output/can_ecu5; $SHELL'"

gnome-terminal -t "CAN_ECU_CRITICAL_TSCF" --command="bash -c './output/can_ecu_tscf; $SHELL'"
#gnome-terminal -t "CAN_ECU_CRITICAL_NTSCF" --command="bash -c 'sudo nice -20 ./output/can_ecu_ntscf; $SHELL'"
#gnome-terminal -t "CAN_ECU_CRITICAL_TSCF_req" --command="bash -c './output/can_ecu_tscf_req; $SHELL'"
#gnome-terminal -t "CAN_ECU_CRITICAL_NTSCF_req" --command="bash -c 'sudo nice -20 ./output/can_ecu_ntscf_req; $SHELL'"
