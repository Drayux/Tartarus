obj-m := hid-tartarus_v2.o

KERNELDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# TODO make a debug version perhaps?
all: 
	$(MAKE) -C $(KERNELDIR) M=$(PWD)

# TODO this clean could probably be *cleaner*
.PHONY: clean
clean:
	@echo Cleaning directory
	rm -rm *.cmd *.o *.ko Module.symvers modules.order