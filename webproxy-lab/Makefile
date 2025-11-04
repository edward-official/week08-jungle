# Makefile for Proxy Lab 
#
# You may modify this file any way you like (except for the handin
# rule). Your instructor will type "make" on your specific Makefile to
# build your proxy from sources.

CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread

all: proxy

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

# event-log 폴더 안의 event-log.c 빌드
event-log.o: event-log/event-log.c event-log/event-log.h
	$(CC) $(CFLAGS) -c event-log/event-log.c -o event-log.o

# proxy.c는 event-log/event-log.h도 include 하므로 의존성에 추가
proxy.o: proxy.c csapp.h event-log/event-log.h proxy-help.h
	$(CC) $(CFLAGS) -c proxy.c

# 링크할 때 event-log.o 까지 같이 묶어주기
proxy: proxy.o csapp.o event-log.o
	$(CC) $(CFLAGS) proxy.o csapp.o event-log.o -o proxy $(LDFLAGS)

# Creates a tarball in ../proxylab-handin.tar that you can then
# hand in. DO NOT MODIFY THIS!
handin:
	(make clean; cd ..; tar cvf $(USER)-proxylab-handin.tar proxylab-handout --exclude tiny --exclude nop-server.py --exclude proxy --exclude driver.sh --exclude port-for-user.pl --exclude free-port.sh --exclude ".*")

clean:
	rm -f *~ *.o proxy core *.tar *.zip *.gzip *.bzip *.gz
