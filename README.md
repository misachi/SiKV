# SiKV
In memory Key Value store

# Running the server
```
make
./main.o
```

# Running the client
```
make client
./client.o <server_name> <port> e.g ./client.o 127.0.0.1 8007
```

### Check for potential memory leaks
```
make memcheck  # server
make client_memcheck  # client
```
