
obj-m +=  gw_ecu_dummy_driver.o
obj-m +=  gw_ecu_dummy_driver1.o
obj-m +=  gw_ecu_dummy_driver2.o
obj-m +=  gw_ecu_dummy_driver3.o
obj-m +=  gw_ecu_dummy_driver4.o
obj-m +=  gw_ecu_dummy_driver5.o
obj-m +=  gw_ecu_dummy_driver6.o
obj-m +=  gw_ecu_dummy_driver7.o
obj-m +=  gw_ecu_dummy_driver_TSCF.o
obj-m +=  gw_ecu_dummy_driver_TSCF_req.o
obj-m +=  gw_ecu_dummy_driver_NTSCF.o
obj-m +=  gw_ecu_dummy_driver_NTSCF_req.o
obj-m +=  can_ecu_dummy_driver.o
obj-m +=  can_ecu_dummy_driver1.o
obj-m +=  can_ecu_dummy_driver2.o
obj-m +=  can_ecu_dummy_driver3.o
obj-m +=  can_ecu_dummy_driver4.o
obj-m +=  can_ecu_dummy_driver5.o
obj-m +=  can_ecu_dummy_driver6.o
obj-m +=  can_ecu_dummy_driver7.o
obj-m +=  can_ecu_dummy_driver_TSCF.o
obj-m +=  can_ecu_dummy_driver_TSCF_req.o
obj-m +=  can_ecu_dummy_driver_NTSCF.o
obj-m +=  can_ecu_dummy_driver_NTSCF_req.o

export KROOT=/lib/modules/$(shell uname -r)/build

allofit:  modules
modules:
	@$(MAKE) -C $(KROOT) M=$(PWD) modules
modules_install:
	@$(MAKE) -C $(KROOT) M=$(PWD) modules_install
kernel_clean:
	@$(MAKE) -C $(KROOT) M=$(PWD) clean

clean: kernel_clean
	rm -rf   Module.symvers modules.order
