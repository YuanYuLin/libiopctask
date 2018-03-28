
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
#SRC += task_main_db.c
#SRC += task_uds_db.c
SRC += task_www.c

SRC += task_main.c
SRC += task_uds_www.c
#SRC += task_syscmd.c
SRC += task_sysinit.c

include Makefile.include.lib
