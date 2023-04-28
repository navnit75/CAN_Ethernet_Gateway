make
gnome-terminal -t "ETH_ECU6" --command="bash -c './eth_ecu_req 100.9.8.81 /opt/gateway/eth6.txt; $SHELL'"
gnome-terminal -t "ETH_ECU7" --command="bash -c './eth_ecu_req 110.10.9.91 /opt/gateway/eth7.txt; $SHELL'"
gnome-terminal -t "ETH_ECU8" --command="bash -c './eth_ecu_req 120.11.10.101 /opt/gateway/eth8.txt; $SHELL'"
