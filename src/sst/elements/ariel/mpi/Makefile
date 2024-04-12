all: mpilauncher-onenode hello fakepin reduce libarielapi.so

clean:
	rm -rf mpilauncher-onenode mpilauncher hello fakepin reduce *.o *.so

mpilauncher-onenode: mpilauncher-onenode.cc
	g++ -g -o mpilauncher-onenode mpilauncher-onenode.cc

hello: hello.cc
	mpic++ -fopenmp -g -o hello hello.cc
reduce: reduce.cc libarielapi.so
	mpic++ -fopenmp -g -o reduce reduce.cc -L. -larielapi

fakepin: fakepin.cc
	g++ -g -o fakepin fakepin.cc

libarielapi.so: libarielapi.o
		gcc -shared -o libarielapi.so libarielapi.o

libarielapi.o: libarielapi.c
		gcc -o libarielapi.o -fPIC -I./ -c libarielapi.c

.PHONY: all clean

