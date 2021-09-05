all:
	gcc cli_pthread.c -pthread -o client
	gcc ser_pthread.c -pthread -lssl -lcrypto -o server
clean:
	rm -rf *o server client