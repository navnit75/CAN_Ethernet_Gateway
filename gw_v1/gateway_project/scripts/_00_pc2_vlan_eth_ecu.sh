sudo modprobe 8021q
sudo vconfig add enp2s0 1
sudo ip addr add 50.4.2.21/24 broadcast 50.4.2.255 dev enp2s0.1
sudo ip link set up enp2s0.1

sudo modprobe 8021q
sudo vconfig add enp2s0 2
sudo ip addr add 60.5.3.41/24 broadcast 60.5.3.255 dev enp2s0.2
sudo ip link set up enp2s0.2

sudo modprobe 8021q
sudo vconfig add enp2s0 3
sudo ip addr add 70.6.4.51/24 broadcast 70.6.4.255 dev enp2s0.3
sudo ip link set up enp2s0.3

sudo modprobe 8021q
sudo vconfig add enp2s0 4
sudo ip addr add 80.7.6.61/24 broadcast 80.7.6.255 dev enp2s0.4
sudo ip link set up enp2s0.4

sudo modprobe 8021q
sudo vconfig add enp2s0 5
sudo ip addr add 90.8.7.71/24 broadcast 90.8.7.255 dev enp2s0.5
sudo ip link set up enp2s0.5

sudo modprobe 8021q
sudo vconfig add enp2s0 6
sudo ip addr add 100.9.8.81/24 broadcast 100.9.8.255 dev enp2s0.6
sudo ip link set up enp2s0.6

sudo modprobe 8021q
sudo vconfig add enp2s0 7
sudo ip addr add 110.10.9.91/24 broadcast 110.10.9.255 dev enp2s0.7
sudo ip link set up enp2s0.7

sudo modprobe 8021q
sudo vconfig add enp2s0 8
sudo ip addr add 120.11.10.101/24 broadcast 120.11.10.101 dev enp2s0.8
sudo ip link set up enp2s0.8
