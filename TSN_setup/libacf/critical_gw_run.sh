# Building tscf_listener
cc -I./src -I./include -o critical_gw src/avtp.c src/avtp_tscf.c src/common.c src/avtp_stream.c src/can.c critical_gw.c

# Example: 
sudo ./critical_gw -d 1c:fd:08:71:6d:66  -i enp2s0.5 -p 3 -n 100
