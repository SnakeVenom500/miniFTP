CC=gcc

all: client server

client: myftp.c myftp.h
	$(CC) myftp.c -o client

server: myftpserve.c myftp.h
	$(CC) myftpserve.c -o server

clean:
	rm -f client server
