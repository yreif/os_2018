CC=g++
CFLAGS = -g -std=c++11 -Wall

SRC = whatsappServer.cpp whatsappServer.h whatsappClient.cpp whatsappClient.h whatsappCommon.h whatsappio.cpp whatsappio.h

TAR=tar
TARFLAGS=-cvf
TARNAME=ex4.tar
TARSRCS=$(SRC) Makefile README


all: whatsappClient whatsappServer

whatsappClient: whatsappClient.o whatsappio.o
	$(CC) $(CFLAGS) -o whatsappClient whatsappClient.o whatsappio.o

whatsappServer: whatsappServer.o whatsappio.o
	$(CC) $(CFLAGS) -o whatsappServer whatsappServer.o whatsappio.o

whatsappClient.o: whatsappClient.cpp whatsappClient.h whatsappio.h whatsappCommon.h
	$(CC) $(CFLAGS) -c whatsappClient.cpp

whatsappServer.o: whatsappServer.cpp whatsappServer.h whatsappio.h whatsappCommon.h
	$(CC) $(CFLAGS) -c whatsappServer.cpp

whatsappio.o: whatsappio.cpp whatsappio.h
	$(CC) $(CFLAGS) -c whatsappio.cpp

clean:
	$(RM) whatsappClient whatsappServer *.o *~ *core

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)
