cc -I./src -I./include -o critical_ecu src/avtp.c src/avtp_tscf.c src/common.c src/avtp_stream.c src/can.c critical_ecu.c

# sudo ./tscf_listener -d <source MAC address> -i <inteface name> -m <max-transmission time> 

# Example: 
sudo ./critical_ecu -d 1c:fd:08:71:6d:66  -i enp2s0.5 -p 3 -n 100
