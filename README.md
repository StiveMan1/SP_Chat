# Game Lobby Chat Implementation
This project involves creating a game lobby chat system in C, comprising a server and multiple clients that connect to it. The server broadcasts messages from any client to all connected clients, simulating a chat room environment.

## Overview
The goal of this project is to implement a simple chat system where multiple clients can connect to a central server (the game lobby). Messages typed by any client are broadcasted to all other connected clients, resembling a real-time chat room.

## Requirements: 
### Server:
* Acts as a central hub for all connected clients.
* Broadcasts messages received from any client to all other clients.
* Ignores messages that are empty or consist only of whitespace.
* Trims leading and trailing whitespace from messages before broadcasting.
### Clients:
* Connect to the server upon execution.
* Display all messages received from other clients via the server.
* Messages typed by the client are sent to the server for broadcasting.
* Own messages are not echoed back to the client.
## Usage
### Server:
* Start the server using `./server`.
### Clients:
* Start one or more clients using `./client`.
* Each client will connect to the server and display messages from all connected clients, including their own.
```shell
$> ./server             $> ./client             $> ./client
I am client-1!          I am client-1!          I am client-1!
I am client-2!          I am client-2!          I am client-2!
```
## Message Format
* Messages are terminated by `\n` (newline).
* Empty messages (those consisting only of spaces or empty lines) are not broadcasted.
* Leading and trailing spaces in messages are trimmed before broadcasting.