# Multiple User Single Server Chat Application
## Multiple user chat application using Socket Programming and Threads

Project by: Supratim Sarkar 

Org.: IIT Bhubaneswar

The project has these facilities:
1. one server runs on a specific port and oversees the whole operation to which users are currently connected and not and shows that.
2. Multiple users/clients can be created with specific usernames on a given server IP address and server port number.
3. All the conservation between two clients is end-to-end encrypted using RC4 encryption.
4. For specific users, we are providing the private key when the conversation window for a user is created.
5. The supported commands are:
	1. status: List all the users' status
	2. connect <username>: Connect to username and start chatting
	3. goodbye: Ends current chatting session
	4. close: Disconnects you from the server
6. Two of the users can be connected and they can chat between them(the messages between users should not be the same as commands.)
7. When the server is closed all the clients are disconnected.

How to Run:

1. to run the server:

#### g++ server.cpp -o server
##### ./server

2. to run clients:

##### g++ client.cpp -o client
#### ./client 
then enter a username for this user and connect with other users.
