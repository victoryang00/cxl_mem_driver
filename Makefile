$(warning KERNELRELEASE=$(KERNELRELEASE))
ifeq ($(KERNELRELEASE),)

KERNELDIR ?= /lib/modules/$(shell uname -r)/build  
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules 

clean:
	rm -rf *.o *~ core .depend .*.cmd  *.mod.c .tmp_versions Module* modules*

.PHONY: all clean

else
  obj-m := cxl_mem_driver.o
endif

