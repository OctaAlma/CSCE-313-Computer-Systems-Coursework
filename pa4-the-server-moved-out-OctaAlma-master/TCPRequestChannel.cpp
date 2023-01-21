#include "TCPRequestChannel.h"

using namespace std;
int BACKLOG = 30;

TCPRequestChannel::TCPRequestChannel (const std::string _ip_address, const std::string _port_no) {
    if (_ip_address.empty()){
        // SEVER!
        
        struct addrinfo hints, *res;

        // fist, load up address structs with get addrinfor():
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        int addrInfoError = getaddrinfo(NULL, _port_no.c_str(), &hints, &res);

        // getAddrInfo returns 0 if it succeeds
        if (addrInfoError){ 
            perror("getaddrinfo failed. Exiting"); 
            exit(0);
        } 

        // make a socket, bind it, and listen to it
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        
        if (sockfd == -1){
            perror("Could not create socket endpoint. Exiting");
            exit(0);
        }

        if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1){
            perror("Could not bind address to socket. Exiting");
            exit(0);
        }

        if (listen(sockfd, BACKLOG) == -1){
            perror("listen() failed. Exiting");
            exit(0);
        }

        freeaddrinfo(res);

    }else{
        // CLIENT!
        struct addrinfo hints, *res;

        // first, load up address structs with getaddrinfo():

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;  // use IPv4 or IPv6, whichever
        hints.ai_socktype = SOCK_STREAM;

        // we could put "80" instead of "http" on the next line:
        int addrInfoError = getaddrinfo(_ip_address.c_str(), _port_no.c_str(), &hints, &res);
        // getAddrInfo returns 0 if it succeeds
        if (addrInfoError){ 
            perror("getaddrinfo failed. Exiting"); 
            exit(0);
        } 

        // make a socket:
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd == -1){
            perror("Could not create socket endpoint. Exiting");
            exit(0);
        }

        // connect it to the address and port we passed in to getaddrinfo():
        if(connect(sockfd, res->ai_addr, res->ai_addrlen)<0) {
            perror("connect failed:");
            exit(0);
        }
        
        freeaddrinfo(res);
    }
}

TCPRequestChannel::TCPRequestChannel (int _sockfd) {
    sockfd = _sockfd;
}

TCPRequestChannel::~TCPRequestChannel () {}

int TCPRequestChannel::accept_conn () {
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    addr_size = sizeof(their_addr);
    int new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
    return new_fd;
}

int TCPRequestChannel::cread (void* msgbuf, int msgsize) {
    return read(this->sockfd, msgbuf, msgsize);
}

int TCPRequestChannel::cwrite (void* msgbuf, int msgsize) {
    return write(this->sockfd, msgbuf, msgsize);
}
