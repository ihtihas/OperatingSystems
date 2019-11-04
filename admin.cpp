#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <string.h>
#include <string>
#include <sstream>
#include <fstream>
#include <queue>
#include <vector>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

using namespace std;

//Global Variables
int adminPipe[2], compPipe[2], adminR, adminW, compR, compW; //Pipe Related

int activeClients[1024], connections = 0, clientPorts[1024]; //Client Connections

struct client_info
{
    int clientId;
    string filename;
    int socket_fd;
    int port;
};
queue<client_info> readyQueue; //Ready Queue
bool terminateFlag = false;


//Function Definitons
int ComputerProcess(int, int);
void qDump(int);
void print_queue(queue<client_info>, int);
void *acceptConnections(void *arg);
void *readConnections(void *);

int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        cerr << "***Too less arguments.. port and sleep time should be provided***" << endl;
        exit(1);
    }

    int port_number = atoi(argv[1]);
    int sleep_time = atoi(argv[2]);

    int status;

    

    pid_t returned_pid;

    pipe(adminPipe);
    compR = adminPipe[0];
    adminW = adminPipe[1];

    pipe(compPipe);
    adminR = compPipe[0];
    compW = compPipe[1];

    returned_pid = fork();

    if (returned_pid < 0)
    {
        cerr << "Failed to fork process \n";
        exit(1);
    }

    else if (returned_pid == 0)
    {   
        usleep(10);
        cout << "Computer, " << returned_pid << ", " << getpid() << "\n";
        //Register signal for Queue Dump
        signal(SIGRTMIN, qDump);
        int value = ComputerProcess(port_number, sleep_time);
        if (value == 0){
            
            return 0;
        }
            
    }

    
    cout << "Admin, " << returned_pid << ", " << getpid() << "\n";

    char command;
    do
    {   
        
        usleep(100000);
        cout << "Admin Command : ";
        cin >> command;

        if (command == 'Q')
        {
            char buffer[256];
            memset(buffer, 0, 256);
            kill(returned_pid, SIGRTMIN);
            read(adminR, buffer, 255);
            cout << buffer << endl;
        }
        else if (command == 'x' || command == 'T')
        {
            write(adminW, &command, sizeof(command));
        }
        
    } while (command != 'T');

    pid_t done = wait(&status);

    

    exit(0);
}

int ComputerProcess(int port_number, int sleep_time)
{
    pthread_t ccThread, cThread;

    for (int i = 0; i < 1024; i++)
    {
        activeClients[i] = 0;
        clientPorts[i] = 0;
    }
 

    //accept Connections -- CC Thread -- New connection - Active client list
    pthread_create(&ccThread, NULL, acceptConnections, (void *)(intptr_t)port_number);

    //Read through Connections - C thread -- Active Client list -input -- put in ready Queue
    pthread_create(&cThread, NULL, readConnections, NULL);

    //Perform admin Operations - A thread -- Read Pipe and Ready Queue

    while (!terminateFlag)
    {
        char command;
        read(compR, &command, sizeof(command));
       
        if (command == 'x') //Execute Ready Queue
        {

            while (!readyQueue.empty())
            {
                client_info client = readyQueue.front();
                string filename = client.filename;
                ifstream fin; // declare an input file stream
                int x = 0, number_of_integers = 0, sum = 0;
                // open file file_name for input
                fin.open(filename.c_str(), ios::in);
                // check if file is opened for input
                if (!fin.is_open())
                {
                    cerr << "Unable to open file " << filename << endl;
                    exit(0);
                }
                fin >> number_of_integers;
                for (int count = 0; count < number_of_integers; count++)
                {
                    fin >> x;
                    sum += x;
                    usleep(sleep_time*1000);
                }
                fin.close();

                ostringstream ss;
                ss << client.clientId << " " << sum;
                string strOut = ss.str();
                send(client.socket_fd, strOut.c_str(), strOut.size() + 1, 0);

                readyQueue.pop();
            }
        }

        else if (command = 'T') //Terminate the system
        {

           

            terminateFlag = true;
        }
    }

  

    return 0;
}

void *acceptConnections(void *arg)
{
    
    int port_number = (intptr_t)arg;

    sockaddr_in servAddr;
    memset((char *)&servAddr, 0, sizeof(servAddr));

    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port_number);

    int listeningSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (listeningSocket < 0)
    {
        cerr << "Error establishing the server socket" << endl;
        exit(0);
    }

    int bindStatus = bind(listeningSocket, (struct sockaddr *)&servAddr, sizeof(servAddr));
    if (bindStatus < 0)
    {
        cerr << "Error binding socket to local address" << endl;
        exit(0);
    }

   printf("Computer process server socket ready.\n" );

    listen(listeningSocket, 1024);

    //take up the connections

    while (!terminateFlag)
    {

        sockaddr_in newSockAddr;
        socklen_t newSockAddrSize = sizeof(newSockAddr);

        int client_fd = accept(listeningSocket, (sockaddr *)&newSockAddr, &newSockAddrSize);
        if (client_fd < 0)
        {
            cerr << "Error accepting request from client!" << endl;
            exit(1);
        }

        activeClients[connections] = client_fd;
        clientPorts[connections] = ntohs(newSockAddr.sin_port);

        connections++;
    }

    pthread_exit(NULL);
}

void *readConnections(void *)
{
    while (!terminateFlag)
    {

        for (int count = 0; count < connections; count++)
        {

            char buf[256];
            memset(buf, 0, 256);
            if (activeClients[count] != 0)
            {

                if (read(activeClients[count], buf, 255) < 0)
                {
                    cerr << "Error reading from client!" << endl;
                    exit(1);
                }
                else
                {

                    client_info client;
                    string data;
                    data += buf;
                    vector<string> tokens;
                    stringstream check1(data);
                    string intermediate;
                    while (getline(check1, intermediate, ' '))
                    {
                        tokens.push_back(intermediate);
                    }
                    client.clientId = atoi(tokens[0].c_str());
                    client.filename = tokens[1];
                    client.socket_fd = activeClients[count];
                    client.port = clientPorts[count];

                    if (client.filename == "nullfile")
                    {
                        close(activeClients[count]);
                        activeClients[count] = 0;
                        
                    }
                    else
                    {
                        readyQueue.push(client);
                       
                    }
                }
            }
        }
    }
    pthread_exit(NULL);
}
void qDump(int sig_num)
{

    if (readyQueue.empty())
    {
        string strOut = "Empty Ready Queue!";
        write(compW, strOut.c_str(), strOut.size() + 1);
    }
    else
    {
        print_queue(readyQueue, compW);
    }

    
}

void print_queue(queue<client_info> q, int compW)
{
    ostringstream ss;
    string strOut;
    while (!q.empty())
    {
        client_info client = q.front();

        ss << client.clientId << ", " <<client.filename<<", " << client.socket_fd <<", " << client.port << "\n";
        strOut = ss.str();

        q.pop();
    }
    write(compW, strOut.c_str(), strOut.size() + 1);
}


