obj-m := tartarus.o

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# TODO make a debug version perhaps?
all: 
	$(MAKE) -C $(KERNELDIR) M=$(PWD)

# TODO this clean could probably be *cleaner*
.PHONY: clean
clean:
	@echo Cleaning directory
	rm -f .[!.]*.cmd .[!.]*.o .[!.]*.ko .[!.]*.d *.mod* .[!.]Module.symvers modules.order
