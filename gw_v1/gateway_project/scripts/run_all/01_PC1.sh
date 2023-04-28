cd ../
sh _01_pc1_can_run.sh
cd ../pc1/gateway_ecu/non_critical/
make
sh build.sh
cd ../critical/
make
sh build.sh
#cd ../../ntscf/make_for_eth2can/
#make 
#sh build.sh
#cd ../make_for_can2eth/
#make
#sh build.sh
