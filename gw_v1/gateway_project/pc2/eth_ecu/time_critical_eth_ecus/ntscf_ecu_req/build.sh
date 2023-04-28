# Can to ethernet --> GW side to Eth ecu side 
# Recieving req then replying 
make
gnome-terminal -t "ETH_ECU_NTSCF_req" --command="bash -c 'sudo ./ntscf_ecu_req_exe -d 8c:ec:4b:8d:85:09 -i enp3s0; $SHELL'"
