CC= gcc
CFLAGS= -Wall -g -c
LFLAGS = -Wall -g

OBJS = fs.o ricardo.o main.o utils.o

all: $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) -o dcc_fs.exe
	
main.o:fs.o ricardo.o
	$(CC) $(CFLAGS) main.c	
fs.o:fs.c fs.h utils.o
	$(CC) $(CFLAGS) fs.c
ricardo.o:fs.h ricardo.c utils.o
	$(CC) $(CFLAGS) ricardo.c
utils.o: utils.c utils.h
	$(CC) $(CFLAGS) utils.c
	
clean:
	rm *.o *~ *.exe
	
