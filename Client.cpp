#include <iostream>
#include <unistd.h>
#include <string.h>
#include <string>
#include <sstream>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <fstream>
#include <netdb.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>

#include <vector>

using namespace std;

int main(int argc, char *argv[]){
    
    if (argc < 4) {
        cerr << "***Too less arguments.. <argument list : clientId, servername, port>***" << endl;
        exit(0);
    }

    char *clientId = argv[1];
    char *hostserver = argv[2];
    int port_number = atoi(argv[3]);

    char buffer[1500]; 

    struct hostent* host = gethostbyname(hostserver);
    sockaddr_in sendSockAddr;  
    memset((char*)&sendSockAddr,0, sizeof(sendSockAddr));
    
    sendSockAddr.sin_family = AF_INET;
    sendSockAddr.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*)*host->h_addr_list));
    sendSockAddr.sin_port = htons(port_number);

    int client = socket(AF_INET, SOCK_STREAM, 0);

    int status = connect(client,(sockaddr*) &sendSockAddr, sizeof(sendSockAddr));

    if(status< 0)
    {
        cerr<<"**Error connecting to socket**"<<endl; 
        exit(1);
    }
    
    while(true){

        cout << "Input file name: ";
        string filename;
        getline(cin, filename);
        
        ostringstream ss;
		ss <<  clientId << " " << filename;
		string strOut = ss.str();

        if(filename == "nullfile")
        {
            send(client,strOut.c_str(), strOut.size()+1, 0);
            break;
        }
        
        send(client,strOut.c_str(), strOut.size()+1, 0);
        
        memset(&buffer, 0, sizeof(buffer));//clear the buffer
        recv(client, (char*)&buffer, sizeof(buffer), 0);

        string receivedData ;
        receivedData+= buffer ;
        vector <string> tokens; 
      
                      
        stringstream check1(receivedData); 
      
        string intermediate; 
      
        // Tokenizing w.r.t. space ' ' 
        while(getline(check1, intermediate, ' ')) 
        { 
          tokens.push_back(intermediate); 
        } 
                        
        string sum = tokens[1];

        cout << clientId << ", "<<filename<<", " << sum << endl; 


    }


    close(client);
    
    return 0;


}