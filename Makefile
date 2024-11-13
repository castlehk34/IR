CC=g++
SRCS=$(wildcard *.cpp)
OBJS=$(SRCS:.cpp=.o)

CPPFLAGS=-I../openssl/include

LDFLAGS=-L -lssl -lcrypto 

all: tls_server tls_client dtls_server dtls_client

tls_client: tls_client.o
	$(CC) -o $@ tls_client.o $(LDFLAGS)

tls_server: tls_server.o
	$(CC) -o $@ tls_server.o $(LDFLAGS)

dtls_client: dtls_client.o
	$(CC) -o $@ dtls_client.o $(LDFLAGS)

dtls_server: dtls_server.o
	$(CC) -o $@ dtls_server.o $(LDFLAGS)

%.o: %.cpp
	$(CC) -c $< $(CPPFLAGS)
	@echo "CC <= $<"

clean:
	$(RM) tls_server tls_client dtls_server dtls_client $(OBJS)
