obj-m += main.o

KDIR = /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR)  M=$(shell pwd)/src modules

clean:
	make -C $(KDIR)  M=$(shell pwd)/src clean
