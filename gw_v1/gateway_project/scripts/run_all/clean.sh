cd ../../pc1/platform/can_dummy_drivers/
make clean
cd ../../gateway/non_critical/
rm -f gateway_non_critical
cd src/
rm -f *.o
cd ../../critical/ntscf/make_for_eth2can/
sh clean.sh
cd ../make_for_can2eth/
sh clean.sh
cd ../../tscf/make_for_eth2can/
sh clean.sh
cd ../make_for_can2eth/
sh clean.sh
cd ../../../../can_ecu/output/
rm -f can*
