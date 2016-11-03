all:
	gcc MFS.c -o MFS -g -pthread

run: all; ./MFS input.txt

clean:
	-rm -rf *.o *.exe
