all:
	gcc MFS.c -o MFS -g -pthread

clean:
	-rm -rf *.o *.exe
