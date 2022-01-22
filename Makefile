CC			= gcc
WARNINGS	= -g  -Wall -Wextra -Wno-unused-variable -pedantic
CFLAGS  	= -std=c99 -D_GNU_SOURCE $(WARNINGS) -lpthread -pthread
DFT			= -D_DEFAULT_SOURCE
LIB			= lib
OBJ			= obj
SRC			= src
CONF		= configs/config.ini
BIN			= bin
ZIP			= francesca_ugazio-CorsoA
TARGETS		= c server client

all:	$(TARGETS)

ini:
	$(CC) $(CFLAGS) -c $(LIB)/ini/ini.c -o $(OBJ)/ini.o

api:
	$(CC) $(CFLAGS) -c $(LIB)/api/api.c -o $(OBJ)/api.o

threadpool:
	$(CC) $(CFLAGS) -c $(SRC)/Server/threadpool.c -o $(OBJ)/threadpool.o


utils:
	$(CC) $(CFLAGS) $(DFT) -c $(LIB)/utils/utils.c -o $(OBJ)/utils.o

storage:
	$(CC) $(CFLAGS) $(DFT) -c $(LIB)/storage/storage.c -o $(OBJ)/storage.o

client: utils api
	$(CC) $(CFLAGS) $(DFT) -c $(SRC)/Client/client.c -o $(OBJ)/client.o
	$(CC) $(CFLAGS) $(OBJ)/utils.o $(OBJ)/api.o $(OBJ)/client.o -o $(BIN)/client.out

server: ini utils threadpool storage
	$(CC) $(CFLAGS) -c $(SRC)/Server/server.c -o $(OBJ)/server.o
	$(CC) $(CFLAGS) $(OBJ)/ini.o $(OBJ)/utils.o $(OBJ)/threadpool.o $(OBJ)/storage.o $(OBJ)/server.o -o $(BIN)/server.out

clean:
	@echo Clean
	-rm -rf $(OBJ)/* $(BIN)/*
	@echo Done

c: clean

.PHONY: c clean server all zip