#C for object files, E for executables, A for all, L for lib flags
CFLAGS=-c -Wall -pedantic
EFLAGS=-Wall -pedantic
AFLAGS=-Wall -pedantic 

main: mftp mftpserve

mftp: mftp.o mftp.h
	cc -o mftp $(EFLAGS) mftp.o

mftpserve: mftpserve.o mftp.h
	cc -o mftpserve $(EFLAGS) mftpserve.o

mftp.o: mftp.c mftp.h
	cc $(CFLAGS) mftp.c

mftpserve.o: mftpserve.c mftp.h
	cc $(CFLAGS) mftpserve.c

all: mftpserve.c mftp.c mftp.h
	cc -o mftp $(AFLAGS) mftp.c
	cc -o mftpserve $(AFLAGS) mftpserve.c 

clean:
	rm -f mftpserve.o && rm -f mftp.o && rm -f mftp && rm -f mftpserve

run: mftpserve
	./mftpserve

client: mftp
	./mftp localhost