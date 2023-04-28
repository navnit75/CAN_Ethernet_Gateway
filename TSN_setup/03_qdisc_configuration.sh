# Deleting already created Qdiscs 
# Note : Check if the enp2s0 interface is UP. 
sudo tc qdisc del dev enp2s0 root   

# QLAN configuration is always done at the TALKER side. 
sudo tc qdisc add dev enp2s0 parent root handle 6666 mqprio num_tc 3 map 2 2 1 0 2 2 2 2 2 2 2 2 2 2 2 2 queues 1@0 1@1 2@2 hw 0

# Verifying the configuration of classes. 
sudo tc -g class show dev enp2s0

# Configuring the cbs qdiscs 
sudo tc qdisc replace dev enp2s0 parent 6666:1 handle 7777 cbs \
        idleslope 98688 sendslope -901312 hicredit 153 locredit -1389 \
        offload 1

sudo tc qdisc replace dev enp2s0 parent 6666:2 handle 8888 cbs \
        idleslope 3648 sendslope -996352 hicredit 12 locredit -113 \
        offload 1

# Configuring the ETF qdiscs 
sudo tc qdisc add dev enp2s0 parent 7777:1 handle 9999 etf clockid CLOCK_TAI delta 500000 offload
sudo tc qdisc add dev enp2s0 parent 8888:1 handle 5555 etf clockid CLOCK_TAI delta 500000 offload

# Showing the details of all the qdisc properties in the enp2s0
sudo tc -g qdisc show dev enp2s0


# To check the stats use 
# sudo tc -s qdisc show dev enp2s0 

# Summary: 
# 7777 -> CBS qdisc --> handles Class 0 or say prio 3 packets 
# 8888 -> CBS qdisc --> handles Class 1 or say prio 2 packets 

# Whatever packet count we see in 
# 7777 ---> Same should be in 9999
# 8888 ---> Same should be in 5555

