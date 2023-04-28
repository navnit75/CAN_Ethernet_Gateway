make
gnome-terminal -t "ETH_ECU_TSCF" --command="bash -c 'sudo nice -20 ./tscf_ecu_exe -d 1c:fd:08:71:6d:66 -i enp2s0; $SHELL'"
