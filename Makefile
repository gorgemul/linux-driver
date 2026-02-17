KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

ifneq ($(KERNELRELEASE),)
ifeq ($(DEBUG),y)
	ccflags-y += -DDEBUG
endif
	obj-m := scull.o
else
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
