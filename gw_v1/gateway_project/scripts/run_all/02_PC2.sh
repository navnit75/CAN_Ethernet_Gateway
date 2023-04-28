cd ../../
cd pc2/eth_ecu/non_time_critical_eth_ecus/eth_ecu/
sh build.sh
cd ../eth_ecu_req
sh build.sh
cd ../../time_critical_eth_ecus/
# cd tscf_ecu/
# sh build.sh
# cd ../tscf_ecu_req
# sh build.sh
cd ntscf_ecu/
sh build.sh
cd ../ntscf_ecu_req/
sh build.sh
