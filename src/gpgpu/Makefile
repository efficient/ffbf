CC=nvcc
CXX=nvcc
LDFLAGS= #-L./SDKlink/lib -lcutil_i386

all: greptest old_greptest

greptest:  greptest.o getfile.o timing.o
	nvcc -o greptest -gencode arch=compute_20,code=sm_20 greptest.o getfile.o timing.o $(LDFLAGS)

old_greptest:  old_greptest.o getfile.o timing.o
	nvcc -o old_greptest -gencode arch=compute_20,code=sm_20 old_greptest.o getfile.o timing.o $(LDFLAGS)


greptest.o: greptest.cu
	nvcc -gencode arch=compute_20,code=sm_20 -c -o greptest.o greptest.cu -I./SDKlink/common/include -g

old_greptest.o: old_greptest.cu
	nvcc -gencode arch=compute_20,code=sm_20 -c -o old_greptest.o old_greptest.cu -I./SDKlink/common/include -g


getfile.o: getfile.c
	gcc $(CFLAGS) -c getfile.c -g

clean:
	/bin/rm -f greptest.o getfile.o timing.o greptest old_greptest.o old_greptest
