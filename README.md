# An implementation of Kademlia DHT and Blockchain in C++

This is work in progress, but the code is heavily documented. So far, the DHT is pretty stable with a full implementation of the Kademlia routing table and the Kademlia protocol, with several of the optimizations recommended in the paper.

## Getting the code
```
git clone --recurse-submodules https://github.com/abdes/blocxxi.git
```

## Building
```
mkdir _build && cd _build && cmake --build ..
```
You can also use any of the cmake options, generators, etc...

The code is portable across Linux (g++ and clang), OS X and Visual Studio 2017.

## Example usage of the DHT
The project in the ndagent module contains a fully working example of setting up the dependency injection chains for building a complete DHT session and running it.

The recommended design is to run the DHT in its independent but exclusive  thread. The implementation does not support multiple threads simultaneously manipulating the DHT. Note however that this scenario is not likely as the DHT is very independent.
