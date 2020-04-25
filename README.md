## client-server-framwork-in-linux
A multiprocessing(use fork) client/server framework implementation in linux, using shared memory(shmget etc.) and signal(semget etc.). 

# Client
Client can send message to server and receive message from server. 

# Server
Server can receive message from multiple clients, and broadcast the message to all clients.

The main process creates a subprocess for each client through fork, and uses select to accept clients' connection. By this way, the main process is not blocked.
Main process also maintains a client list and is responsible for broadcasting the message from each client.

The subprocess receives the client's message and passes the content to the main process by modifying the shared memory. In order to modify the shared memory correctly, signal is used. 
