# building the ntscf_listener
cc -I./src -I./include -o ntscf_listener src/avtp.c src/avtp_ntscf.c src/common.c src/avtp_stream.c src/can.c ntscf_listener.c

# sudo ./ntscf_listener -d <source MAC address> -i <inteface name> 

# Example: 
sudo ./ntscf_listener -d 8c:ec:4b:8d:85:09 -i enp3s0

