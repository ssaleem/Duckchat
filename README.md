# DuckChat-multiServer
This version of DuckChat uses multiple servers for scalability and supports inter-server communication. Separate broadcast trees are formed for each channel where nodes in the tree are servers listening to that channel. To guard against loops, inter-server messages include a unique identifier (generated randmly from `""/dev/urandom"`). Removing unnecessary servers from a channel is done in a lazy fashion. When a server is a leaf in tree with no users, it removes itself from the tree. To guard against network failures, every server renews its subscriptions once per minute.

## Installation
- Use the Makefile to compile
- To start server `./server server_host_name port_name [server1_host_name server1_port_name server2_host_name server2_port_name ...servern_host_name servern_port_name]`
- To start client `./client server_host_name port_name user_name`

## Usage
### Client User Interface
- `/exit`: Logout the user and exit the client software.
- `/join channel`: Join (subscribe in) the named channel, creating the channel if it does not exist.
- `/leave channel`: Leave the named channel.
- `/list`: List the names of all channels.
- `/who channel`: List the users who are on the named channel.
- `/switch channel`: Switch to an existing channel that user has already joined.