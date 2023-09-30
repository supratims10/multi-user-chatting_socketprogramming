#define MAX_SIZE 2048

#include <bits/stdc++.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <signal.h>
#include <mutex>
using namespace std;

// SERVER CLASS
class Server
{
    int socketfd;
    struct sockaddr_in sin;
    int sin_len = sizeof(sin);
    int port = 0;

public:
    bool _construct(int server_port)
    {
        socketfd = socket(AF_INET, SOCK_STREAM, 0);
        if (socketfd < 0)
        {
            cout << "Error creating the socket" << endl;
            exit(EXIT_FAILURE);
        }
        memset(&sin, '\0', sin_len);
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = htonl(INADDR_ANY);
        sin.sin_port = htons(server_port);

        if (bind(socketfd, (sockaddr *)&sin, sin_len) < 0)
        {
            cout << "Error Binding to the address" << endl;
            exit(EXIT_FAILURE);
        }

        port = server_port;
        return true;
    }

    void _listen()
    {
        if (listen(socketfd, 10) < 0)
        {
            cout << "Error Listening" << endl;
            exit(EXIT_FAILURE);
        }
        cout << "Server Listening on port: " << port << endl;
    }

    int _accept()
    {
        int new_socket;
        if ((new_socket = accept(socketfd, (sockaddr *)&sin, (socklen_t *)&sin_len)) < 0)
        {
            cout << "Error Accepting" << endl;
            exit(EXIT_FAILURE);
        }
        return new_socket;
    }

    string _read(int client_socket)
    {
        string msg = "";
        char last_character = '-';
        while (last_character != '%')
        {
            char buffer[MAX_SIZE] = {0};
            read(client_socket, buffer, MAX_SIZE);
            string s(buffer);
            msg += s;
            last_character = s[s.size() - 1];
        }
        return msg.substr(0, msg.size() - 1);
    }

    bool _send(int client_socket, string msg)
    {
        msg = msg + "%";
        char buffer[msg.size()];
        strcpy(buffer, &msg[0]);

        int totalBytes = 0;
        int sentBytes = 0;
        int size = msg.length();
        while (totalBytes < size)
        {
            sentBytes = send(client_socket, &buffer[totalBytes], size - totalBytes, 0);
            if (sentBytes == -1)
            {
                cout << "Error in Server Protocol" << endl;
                return false;
            }
            totalBytes += sentBytes;
        }
        return true;
    }

    void _close(int client_socket)
    {
        close(client_socket);
    }
};

// PARSER FUNCTION
vector<string> parser(string &s, char delimiter)
{
    vector<string> res;
    string curr;
    for (auto x : s)
    {
        if (x == delimiter)
        {
            res.push_back(curr);
            curr = "";
        }
        else
            curr += x;
    }
    res.push_back(curr);
    return res;
}

// PRINT INSTRUCTIONS FUNCITON
string getGeneralInstructions(string username)
{
    string msg = "";
    msg += "\n(server): Hi, " + username + "\n";
    msg += "The supported commands are:\n";
    msg += "1. status             : List all the users status\n";
    msg += "2. connect <username> : Connect to username and start chatting\n";
    msg += "3. goodbye            : Ends current chatting session\n";
    msg += "4. close              : Disconnects you from the server\n";
    return msg;
}

// GLOBAL SERVER OBJECT AND CLIENT QUEUE
Server myserver = Server();
queue<int> waitingClient;

// PARTNER AND SOCKET LIST
unordered_map<string, string> partnerClient;
unordered_map<string, int> socketList;

// MUTEX
mutex mtx;

void showStatus(string username)
{
    vector<string> names;
    string statusMessage = "(server):\n";

    for (auto it : socketList)
        names.push_back(it.first);

    for (int i = 0; i < names.size(); i++)
    {
        string hasPartner = partnerClient.count(names[i]) ? "BUSY" : "FREE";
        statusMessage += "" + names[i] + " " + hasPartner + "\n";
    }

    myserver._send(socketList[username], statusMessage);
    return;
}

bool connectPartner(string username, vector<string> parsedMsg)
{
    string partner = parsedMsg[1];
    cout << "Session request from " + username + " to " + partner << endl;

    // CHECK 1: USER SHOULD NOT BE CONNECTED TO ANYONE ELSE
    if (partnerClient.count(username))
    {
        string errMsg = "(server): You are already connected to someone.";
        myserver._send(socketList[username], errMsg);
        cout << "Already connected, connect request rejected" << endl;
        return false;
    }

    // CHECK 2: USER CANNOT CONNECT TO HIMSELF/HERSELF
    if (username == partner)
    {
        string errMsg = "(server): Destination name can't be same as source Name";
        myserver._send(socketList[username], errMsg);
        cout << "Destination name can't be same as source Name" << endl;
        return false;
    }

    // CHECK 3: REQUESTED PARTNER IS NOT IN SERVER
    if (!socketList.count(partner))
    {
        string errMsg = "(server): No client named: " + partner;
        myserver._send(socketList[username], errMsg);
        cout << "Couldn't connect: No client named: " << partner << endl;
        return false;
    }

    // CHECK 4: PARTNER SHOULD NOT BE CONNECTED TO ANYONE ELSE
    if (partnerClient.count(partner))
    {
        string errMsg = "(server): " + partner + " is BUSY";
        myserver._send(socketList[username], errMsg);
        cout << "Couldn't connect: client " + partner + " is busy" << endl;
        return false;
    }

    // ESTABLISH THE USER TO PARTNER RELATIONSHIP
    partnerClient[username] = partner;
    partnerClient[partner] = username;

    cout << "Connected: " + username << " and " << partner << endl;
    string successMsg = "(server): You are now connected to ";
    myserver._send(socketList[partner], successMsg + username + "");
    myserver._send(socketList[username], successMsg + partner + "");
    return true;
}

void closeSession(string username)
{
    // IF NO CHATTING PARTNER IS THERE
    if (!partnerClient.count(username))
        return;

    // FIND THE CHAT PARTNER AND SEND THE PARTNER THE CLOSING MESSAGE
    string partner = partnerClient[username];
    string msg = "\n(server): " + username + " closed the chat session";
    myserver._send(socketList[partner], msg);

    // ERASE THE PARTNER AND USER
    if (partnerClient.count(partner))
        partnerClient.erase(partner);

    if (partnerClient.count(username))
        partnerClient.erase(username);

    // SEND MESSAGE TO USER
    msg = "(server): Session closed";
    myserver._send(socketList[username], msg);
    cout << "Disconnected " + username + " and " + partner << endl;
}

void *handleClient(void *arg)
{
    // GETTING THE USER THROUGH MUTEXES TO PREVENT CONTEXT SWITCH
    mtx.lock();
    int client_socket = waitingClient.front();
    waitingClient.pop();
    mtx.unlock();

    string username = myserver._read(client_socket);

    // CHECK IF THE USERNAME READ IS VALID
    if (socketList.count(username))
    {
        string errMsg = "Close: There is already a client with that name...\nDisconnecting...\n";
        myserver._send(client_socket, errMsg);
        cout << "Entered username already exists in the database..." << endl;
        return NULL;
    }

    // CORRECT USERNAME HAS BEEN ENTERED. SO WE ARE ALLOCATING THE SOCKET TO USERNAME
    socketList[username] = client_socket;
    cout << "Client ID: " << username << " service started" << endl;
    myserver._send(client_socket, getGeneralInstructions(username));

    string req = "";
    while (true)
    {
        req = myserver._read(client_socket);
        vector<string> parsedMsg = parser(req, ' ');

        if (parsedMsg[0] == "status")
            showStatus(username);

        else if (parsedMsg[0] == "connect")
            connectPartner(username, parsedMsg);

        else if (parsedMsg[0] == "close")
        {
            closeSession(username);
            break;
        }
        else if (partnerClient.count(username))
        {
            string msg = "(" + username + "): " + req;
            myserver._send(socketList[partnerClient[username]], msg);
            if (req == "goodbye")
                closeSession(username);
            continue;
        }
        else
        {
            string errMsg = "(server): Command Not Found";
            myserver._send(client_socket, errMsg);
        }
    }
    socketList.erase(username);
    myserver._close(client_socket);
    cout << "Username: " << username << " disconnected.." << endl;
    pthread_exit(NULL);
}

void exit_handler(int sig)
{
    cout << "\nShutting down the server...\nDisconnecting Clients...\n";
    for (auto it : socketList)
    {
        myserver._send(it.second, "close");
        myserver._close(it.second);
    }
    exit(0);
    return;
}

int main(int argc, char **argv)
{
    try
    {
        myserver._construct(stoi(argv[1]));
    }
    catch (exception e)
    {
        cout << "Please provide command in the following format:\n";
        cout << "./server port\n";
        exit(0);
    }
    signal(SIGINT, exit_handler);
    myserver._listen();
    cout << "Server started successfully..." << endl;
    while (true)
    {
        int client_socket = myserver._accept();
        waitingClient.push(client_socket);
        pthread_t tid;
        pthread_create(&tid, NULL, handleClient, NULL);
    }
    return 0;
}