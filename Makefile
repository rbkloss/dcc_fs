CC= gcc -std=gnu99
CFLAGS= -Wall -g -c
LFLAGS = -Wall -g

OBJS = fs.o main.o utils.o StringProc.o

all: $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) -o dcc_fs.exe
	
main.o:fs.o main.c
	$(CC) $(CFLAGS) main.c	
fs.o:fs.c fs.h utils.o
	$(CC) $(CFLAGS) fs.c
mkdir.o:fs.h mkdir.c
	$(CC) $(CFLAGS) mkdir.c
ricardo.o:fs.h ricardo.c utils.o
	$(CC) $(CFLAGS) ricardo.c
utils.o: utils.c utils.h
	$(CC) $(CFLAGS) utils.c
	
StringProc.o: StringProc.c StringProc.h
	$(CC) $(CFLAGS) StringProc.c
	
	
string_test:
	$(CC) $(LFLAGS) StringProc.c StringProc_test.c -o string_test.exe
	
clean:
	rm *.o *~ *.exe
	
