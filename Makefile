obj-m := Squeue.o

all:
	make -C /usr/src/linux-headers-$(shell uname -r) SUBDIRS=$(PWD) modules
	
clean:
	rm -rf *.o *.ko *.mod.* *.symvers *.order *~








