CC=gcc
CLIENT := client
SERV := serv

deliver.o: $(CLIENT)/deliver.c
	$(CC) -I$(CLIENT) -c $< -o $@ -g -O0

server.o: $(SERV)/server.c
	$(CC) -I$(SERV) -c $< -o $@ -g -O0

all: server deliver
server: server.o
deliver: deliver.o
clean:
	rm -f *.o server deliver