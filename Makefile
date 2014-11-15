CC= gcc -std=gnu99
CFLAGS= -Wall -g -c
LFLAGS = -Wall -g

OBJS = fs.o ricardo.o main.o utils.o

all: $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) -o dcc_fs.exe
	
main.o:fs.o ricardo.o
	$(CC) $(CFLAGS) main.c	
fs.o:fs.c fs.h utils.o
	$(CC) $(CFLAGS) fs.c
ricardo.o:fs.h ricardo.cc utils.o
	$(CC) $(CFLAGS) ricardo.cc
utils.o: utils.cc utils.h
	$(CC) $(CFLAGS) utils.cc
	
string_test:
	$(CC) $(LFLAGS) StringProc.c StringProc_test.c -o string_test.exe
	
clean:
	rm *.o *~ *.exe
	
