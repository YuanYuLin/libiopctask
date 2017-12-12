
TARGET = libiopctask.so

CFLAGS += -I$(SDKSTAGE)/usr/include/libiopcmisc
CFLAGS += -I$(SDKSTAGE)/usr/include/libiopcdao
CFLAGS += -I$(SDKSTAGE)/usr/include/
CFLAGS += -I./include/
CFLAGS += 

LDFLAGS += -L$(SDKSTAGE)/lib/ 
LDFLAGS += -L$(SDKSTAGE)/usr/lib/ 
LDFLAGS += -lc -lpthread -ldl

SRC = 
SRC += ops_task.c
SRC += task_main.c
SRC += task_www.c

include Makefile.include.lib
