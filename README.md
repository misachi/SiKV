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

# Examples

## Running the server
![Server Demo](assets/sikv-server.gif)

## Running the client
![Client Demo](assets/sikv-client.gif)


### Check for potential memory leaks
```
make memcheck  # server
make client_memcheck  # client
```
