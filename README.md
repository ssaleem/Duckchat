# DuckChat
DuckChat is a simple client-server program that uses UDP to cmmunicate. Users run client software to connect to a server and communicate
with other connected users. Users join a channel and communicate with
other users on the channel. Users may join as many channels as they want and can even create their own channels. A multi-server version of this program ca be found [here](https://github.com/ssaleem/Duckchat/tree/multi-server)

## Installation
- Use the Makefile to compile
- To start server `./server server_host_name port_name`
- To start client `./client server_host_name port_name user_name`

## Usage
### Client User Interface
- `/exit`: Logout the user and exit the client software.
- `/join channel`: Join (subscribe in) the named channel, creating the channel if it does not exist.
- `/leave channel`: Leave the named channel.
- `/list`: List the names of all channels.
- `/who channel`: List the users who are on the named channel.
- `/switch channel`: Switch to an existing channel that user has already joined.
