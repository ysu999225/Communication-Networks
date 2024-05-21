/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#include <iostream>
#include <fstream>
using namespace std;

#define PORT "3490" // the port client will be connecting to 

#define MAXDATASIZE 4096 // max number of bytes we can get at once 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;  
	char buf[MAXDATASIZE];
	// create an instance of addrinfo called hints, servinfo points to the start of the linked list, p is an iterator.
	struct addrinfo hints, *servinfo, *p;
	// rv stores the return value of getaddrinfo(), 0 is the success value, errors otherwise.
	int rv;
	// used to save the characterized addrinfo data later.
	char s[INET6_ADDRSTRLEN];
    string addr, protocol, host, port, path;

	// exit code if not provoked in the right format

	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof(hints));
	// this line specifies protocol to either IPv4 or IPv6.
	hints.ai_family = AF_UNSPEC;
	// specifies the connection to using TCP.
	hints.ai_socktype = SOCK_STREAM;
    
	// below is edited
	// store the ip address, or hostname in addr.
    // Assign the IP address or hostname from command line arguments.
	addr = argv[1];

	// Extract the protocol (e.g., "http") by locating the first occurrence of "//".
	protocol = addr.substr(0, addr.find("://"));
	// Update addr to exclude the protocol part.
	addr = addr.substr(addr.find("://") + 3);

	// Determine the path by finding the first "/" after the hostname; default to "/" if not found.
	path = (addr.find('/') != addr.npos) ? addr.substr(addr.find('/')) : "/";
	// Extract the host name by taking the substring up to the first "/".
	host = addr.substr(0, addr.find('/'));

	// Extract the port number if specified; otherwise, default to 80.
	if (host.find(':') != host.npos) {
		port = host.substr(host.find(':') + 1);
		host = host.substr(0, host.find(':'));
	} else {
		port = "80";
	}

	// resuming to the default code

	// check if the hostname can be resolved.
	// the host.data() implementation returns a pointer to the string's underlying char array.
	// we do this because getaddrinfo is a C based function, it only accepts C-style strings.
	if ((rv = getaddrinfo(host.data(), port.data(), &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		// set socket with connection type and data streaming method.
		// ai_protocol is set to 0 to auto use the correct method with ai_socktype.
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	// convert the addrinfo address into readable form according to the connection type,
	// then store it into variable s.
	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure
	// get rid of the linked list of addrinfo, finally.
	
	// edited below
	// everything above is meant to establish the TCP connection,
	// starting here we create the request message.

	// Construct the HTTP request string.
	string request = "GET " + path + " HTTP/1.1\r\n" +
					"User-Agent: Wget/1.12(linux-gnu)\r\n" +
					"Host: " + host + ":" + port + "\r\n" +
					"Connection: Keep-Alive\r\n\r\n";

	cout<<request<<endl;

	// send request through the socket.
	send(sockfd, request.data(), request.size(), 0);

	// initialize an output object out.
	ofstream out;
    out.open("output", ios::binary);
	// a flag, stating whether the header have been recieved.
    bool is_head = true;
    while (true) {
        memset(buf, '\0', MAXDATASIZE);
		// here, officially recieve the data.
        numbytes = recv(sockfd, buf, MAXDATASIZE, 0);
        if (numbytes > 0) {
			// if this is the first block, then it contains the header.
			printf("recieved from server\n");
            if (is_head) {
				is_head = false;
				// set start to point to the end of the header, the beginning of the actual content.
                char* start = strstr(buf, "\r\n\r\n") + 4;
                out.write(start, strlen(start));
            } 
			// if not containing the header, just continue writing.
			else out.write(buf, sizeof(char) * numbytes);
        } 
		else {
			break;
		}
    }

	out.close(); 

	// continuing to default

	if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
	    perror("recv");
	    exit(1);
	}

	// buf[numbytes] = '\0';

	// printf("client: received '%s'\n",buf);

	close(sockfd);

	return 0;
}