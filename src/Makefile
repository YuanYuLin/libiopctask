
TARGET = libiopctask.so

CFLAGS += -I$(SDKSTAGE)/usr/include/libiopcmisc
CFLAGS += -I$(SDKSTAGE)/usr/include/libiopcdao
CFLAGS += -I$(SDKSTAGE)/usr/include/libiopcnet
CFLAGS += -I$(SDKSTAGE)/usr/include/libiopccmd
CFLAGS += -I$(SDKSTAGE)/usr/include/libmicrohttpd
CFLAGS += -I$(SDKSTAGE)/usr/include/
CFLAGS += -I./include/
CFLAGS += 

LDFLAGS += -L$(SDKSTAGE)/lib/ 
LDFLAGS += -L$(SDKSTAGE)/usr/lib/ 
LDFLAGS += -lc -lpthread -ldl

SRC = 
SRC += ops_task.c

SRC += task_bridge_queue.c
SRC += task_timer.c
SRC += task_uds.c

SRC += task_www.c

include Makefile.include.lib
