# SiKV
In memory Key Value store

Currently supports single threaded client-server communication. The underlying hashmap is not thread safe -- this will be changed later

Tested on my laptop installed with AMD Ryzen 7 5700U processor running the following software in a VM
```
Ubuntu 20.04.6 LTS
Linux 5.4.0-182-generic
```

# Installing Dependencies
To install all requirements[Tested on Ubuntu]:
```
sudo chmod +x install_dependencies_ubuntu.sh
./install_dependencies_ubuntu.sh
```
To use the custom allocator. Check the documentation on: [Allocator](https://github.com/misachi/allocator)

# Examples

## Running the server
Remember to replace `main.o` with `main.out`

![Server Demo](assets/sikv-server.gif)

## Running the client
Remember to replace `client.o` with `client.out`

![Client Demo](assets/sikv-client.gif)


### Check for potential memory leaks
**NOTE**: This will not run when using custom allocator(i.e USE_CUSTOM_ALLOC is set to value > 0) since Valgrind does not work well with `mmap`

```
make memcheck  # server
make client_memcheck  # client
```
