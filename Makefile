all:
	gcc cli_pthread.c -pthread -o client
	gcc ser_pthread.c -pthread -o server
clean:
	rm -rf *o server client