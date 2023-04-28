# Compiling the ntscf talker 
cc -I./src -I./include -o ntscf_talker src/avtp.c src/avtp_ntscf.c src/common.c src/avtp_stream.c src/can.c ntscf_talker.c


# sudo ./ntscf_talker -d <destination MAC address> -i <inteface name> 

# Example: 
sudo ./ntscf_talker -d 8c:ec:4b:8d:86:af -i enp3s0
