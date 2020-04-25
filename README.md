# client-server-framwork-in-linux
A multiprocessing(use fork) client/server framework implement in linux, using shared memory(shmget etc.) and signal(semget etc.). 

Client can send message to server and receive message from server. 

Server can receive message from multiple clients in a non-blocking way, and broadcast the message to all clients.
