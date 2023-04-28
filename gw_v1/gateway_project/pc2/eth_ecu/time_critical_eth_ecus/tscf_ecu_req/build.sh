make
gnome-terminal -t "ETH_ECU_TSCF_req" --command="bash -c 'sudo nice -20 ./tscf_ecu_req_exe -d 8c:ec:4b:8d:85:09 -i enp3s0; $SHELL'"
