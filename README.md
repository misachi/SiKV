# SiKV
In memory Key Value store

Currently supports single threaded client-server communication. The underlying hashmap is not thread safe -- this will be changed later

Tested on my laptop installed with AMD Ryzen 7 5700U processor running the following software in a VM
```
Ubuntu 20.04.6 LTS
Linux 5.4.0-182-generic
```

# Installing Dependencies[Ubuntu]
This is for Ubuntu users only. To install all requirements:
```
sudo chmod +x install_dependencies_ubuntu.sh
./install_dependencies_ubuntu.sh
```

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
