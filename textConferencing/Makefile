CC=gcc
MV=mv
CLIENT := deliver
SERV := serv

client.o: $(CLIENT)/client.c
	$(CC) -I$(CLIENT) -c $< -o $@ -g -O0

conference_server.o: $(SERV)/conference_server.c
	$(CC) -I$(SERV) -c $< -o $@ -g -O0

all: conference_server client
server: conference_server.o
	$(CC) $^ -o $@ -pthread
client: client.o
	$(CC) $^ -o $@ -pthread

clean:
	rm -f *.o server client
