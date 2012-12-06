build:tcp_client tcp_server

CC=gcc
CFLAGS=-g -O2 -Wall 

LIBEVENT_DIR=/home/chlaws/packet/libevent-2.0.21-stable
LIBEVENT_INC=$(LIBEVENT_DIR)/include
LIBEVENT_LIB=$(LIBEVENT_DIR)/.libs/libevent_core.a -lrt

CFLAGS+=-I$(LIBEVENT_INC)

tcp_client:tcp_client.c
	$(CC) $(CFLAGS) $< -o  $@ $(LIBEVENT_LIB)

tcp_server:tcp_server.c
	$(CC) $(CFLAGS) $< -o  $@ $(LIBEVENT_LIB)	

clean:
	rm -f tcp_client
	rm -f tcp_server
