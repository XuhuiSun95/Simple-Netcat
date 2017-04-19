all:
	gcc -w snc.c -o snc -lpthread
clean:
	rm -rf snc *~