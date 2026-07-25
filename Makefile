CC = g++
CFLAGS = -Wall -std=c++17 -fPIC
LDFLAGS = -lpthread -ldl

all: server_app client_app

libfileops.so: fileops.cpp
	$(CC) $(CFLAGS) -shared -o libfileops.so fileops.cpp

libprogress.so: progress.cpp
	$(CC) $(CFLAGS) -shared -o libprogress.so progress.cpp

server_app: server.cpp common.h fileops.h libfileops.so
	$(CC) $(CFLAGS) -o server_app server.cpp -L. -lfileops $(LDFLAGS)

client_app: client.cpp common.h fileops.h libfileops.so
	$(CC) $(CFLAGS) -o client_app client.cpp -L. -lfileops $(LDFLAGS)

clean:
	rm -f server_app client_app libfileops.so libprogress.so
