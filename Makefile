# SHELL = /bin/bash
# CPCMD = gcc

# FLAGS = -std=c17 -Wall
# DEVFLAGS = -fsanitize=address  #-g

obj-m := tartarus.o

KERNELDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# TODO make a debug version perhaps?
all: 
	$(MAKE) -C $(KERNELDIR) M=$(PWD)