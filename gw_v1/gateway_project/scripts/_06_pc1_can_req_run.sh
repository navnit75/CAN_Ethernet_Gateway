cd ../pc1/can_ecu
gcc -I./include -pthread ecus/can_ecu6.c -o output/can_ecu6
gcc -I./include -pthread ecus/can_ecu7.c -o output/can_ecu7
gcc -I./include -pthread ecus/can_ecu8.c -o output/can_ecu8
gcc -I./include -pthread ecus/can_ecu_tscf_req.c -o output/can_ecu_tscf_req
gcc -I./include -pthread ecus/can_ecu_ntscf_req.c -o output/can_ecu_ntscf_req

gnome-terminal -t "CAN_ECU_NON_CRITICAL_6" --command="bash -c './output/can_ecu6 stats/can6_rrt_stats.log; $SHELL'"
gnome-terminal -t "CAN_ECU_NON_CRITICAL_7" --command="bash -c './output/can_ecu7 stats/can7_rrt_stats.log; $SHELL'"
gnome-terminal -t "CAN_ECU_NON_CRITICAL_8" --command="bash -c './output/can_ecu8 stats/can8_rrt_stats.log; $SHELL'"
#gnome-terminal -t "CAN_ECU_CRITICAL_NTSCF_req" --command="bash -c './output/can_ecu_ntscf_req stats/can_ntscf_rrt_stats.log; $SHELL'"
gnome-terminal -t "CAN_ECU_CRITICAL_TSCF_req" --command="bash -c './output/can_ecu_tscf_req stats/can_tscf_rrt_stats.log; $SHELL'"
