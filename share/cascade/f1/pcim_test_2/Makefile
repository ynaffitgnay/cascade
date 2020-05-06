VPATH = src:include:$(HDK_DIR)/common/software/src:$(HDK_DIR)/common/software/include

INCLUDES = -I$(SDK_DIR)/userspace/include
INCLUDES += -I $(HDK_DIR)/common/software/include

CC = g++
CFLAGS = -O3 -std=c++11 -fpermissive -DCONFIG_LOGLEVEL=4 -g -Wall $(INCLUDES)

LDLIBS = -lfpga_mgmt -lrt

SRC = ${SDK_DIR}/userspace/utils/sh_dpi_tasks.c

all: daemon

daemon: pcim_test.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LDLIBS) $(SRC) pcim_test.c -o pcim_test

clean:
	rm -f pcim_test
