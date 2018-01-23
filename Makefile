# Makefile for Proxy Lab 
#
# You may modify this file any way you like (except for the handin
# rule). You instructor will type "make" on your specific Makefile to
# build your proxy from sources.
#

BYUNETID = cwjohn42
VERSION = 1
HANDINDIR = /users/faculty/snell/CS324/handin/Fall2017/ProxyLab2


CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread

all: proxy

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

proxy.o: proxy.c csapp.h cache.h
	$(CC) $(CFLAGS) -c proxy.c

cache.o: cache.c cache.h csapp.h
	$(CC) $(CFLAGS) -c cache.c

aqueue.o: aqueue.c aqueue.h csapp.h
	$(CC) $(CFLAGES) -c aqueue.c

proxy: proxy.o csapp.o cache.o aqueue.o
	$(CC) $(CFLAGS) proxy.o csapp.o cache.o aqueue.o -o proxy $(LDFLAGS)

# Creates a tarball in ../proxylab-handin.tar that you can then
# hand in. DO NOT MODIFY THIS!
handin:
	(make clean; cd ..; tar cvf proxylab-handin.tar proxylab-handout --exclude tiny --exclude nop-server.py --exclude proxy --exclude driver.sh --exclude port-for-user.pl --exclude free-port.sh --exclude ".*"; cp proxylab-handin.tar $(HANDINDIR)/$(BYUNETID)-$(VERSION)-proxylab-handin.tar)

clean:
	rm -f *~ *.o proxy core *.tar *.zip *.gzip *.bzip *.gz

