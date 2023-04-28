cd ../pc1/platform/can_dummy_drivers/
sudo dmesg -C
make clean

sudo rmmod can_ecu_dummy_driver
sudo rmmod can_ecu_dummy_driver1
sudo rmmod can_ecu_dummy_driver2
sudo rmmod can_ecu_dummy_driver3
sudo rmmod can_ecu_dummy_driver4
sudo rmmod can_ecu_dummy_driver5
sudo rmmod can_ecu_dummy_driver6
sudo rmmod can_ecu_dummy_driver7
sudo rmmod can_ecu_dummy_driver_TSCF
sudo rmmod can_ecu_dummy_driver_TSCF_req
sudo rmmod can_ecu_dummy_driver_NTSCF
sudo rmmod can_ecu_dummy_driver_NTSCF_req

sudo rmmod gw_ecu_dummy_driver
sudo rmmod gw_ecu_dummy_driver1
sudo rmmod gw_ecu_dummy_driver2
sudo rmmod gw_ecu_dummy_driver3
sudo rmmod gw_ecu_dummy_driver4
sudo rmmod gw_ecu_dummy_driver5
sudo rmmod gw_ecu_dummy_driver6
sudo rmmod gw_ecu_dummy_driver7
sudo rmmod gw_ecu_dummy_driver_TSCF
sudo rmmod gw_ecu_dummy_driver_TSCF_req
sudo rmmod gw_ecu_dummy_driver_NTSCF
sudo rmmod gw_ecu_dummy_driver_NTSCF_req

make

sudo insmod gw_ecu_dummy_driver.ko
sudo insmod gw_ecu_dummy_driver1.ko
sudo insmod gw_ecu_dummy_driver2.ko
sudo insmod gw_ecu_dummy_driver3.ko
sudo insmod gw_ecu_dummy_driver4.ko
sudo insmod gw_ecu_dummy_driver5.ko
sudo insmod gw_ecu_dummy_driver6.ko
sudo insmod gw_ecu_dummy_driver7.ko
sudo insmod gw_ecu_dummy_driver_TSCF.ko
sudo insmod gw_ecu_dummy_driver_TSCF_req.ko
sudo insmod gw_ecu_dummy_driver_NTSCF.ko
sudo insmod gw_ecu_dummy_driver_NTSCF_req.ko

sudo insmod can_ecu_dummy_driver.ko
sudo insmod can_ecu_dummy_driver1.ko
sudo insmod can_ecu_dummy_driver2.ko
sudo insmod can_ecu_dummy_driver3.ko
sudo insmod can_ecu_dummy_driver4.ko
sudo insmod can_ecu_dummy_driver5.ko
sudo insmod can_ecu_dummy_driver6.ko
sudo insmod can_ecu_dummy_driver7.ko
sudo insmod can_ecu_dummy_driver_TSCF.ko
sudo insmod can_ecu_dummy_driver_TSCF_req.ko
sudo insmod can_ecu_dummy_driver_NTSCF.ko
sudo insmod can_ecu_dummy_driver_NTSCF_req.ko
