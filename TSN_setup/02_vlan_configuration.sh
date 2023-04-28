sudo ip link add link enp2s0 name enp2s0.5 type vlan id 5 egress-qos-map 2:2 3:3
sudo ip link set enp2s0.5 up
