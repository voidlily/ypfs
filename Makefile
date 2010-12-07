ypfs : ypfs.o
	gcc -g `pkg-config fuse --libs` -o ypfs ypfs.o

ypfs.o : ypfs.c
	gcc -g -Wall `pkg-config fuse --cflags` -c ypfs.c

clean:
	rm -f ypfs *.o
