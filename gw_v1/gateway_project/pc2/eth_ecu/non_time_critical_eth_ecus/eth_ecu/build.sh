make
gnome-terminal -t "ETH_ECU1" --command="bash -c './eth_ecu_exe 50.4.2.22 /opt/gateway/eth1_rt.txt stats_udp/eth1_rrt_stats.log stats_tcp/eth1_rrt_stats.log; $SHELL'"
gnome-terminal -t "ETH_ECU2" --command="bash -c './eth_ecu_exe 60.5.3.42 /opt/gateway/eth2_rt.txt stats_udp/eth2_rrt_stats.log stats_tcp/eth2_rrt_stats.log; $SHELL'"
gnome-terminal -t "ETH_ECU3" --command="bash -c './eth_ecu_exe 70.6.4.52 /opt/gateway/eth3_rt.txt stats_udp/eth3_rrt_stats.log stats_tcp/eth3_rrt_stats.log; $SHELL'"
