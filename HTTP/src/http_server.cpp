/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <iostream>
#include <istream>

using namespace std;


#define PORT "80"  // the port users will be connecting to by default

#define BACKLOG 10	 // how many pending connections queue will hold
#define MAXDATASIZE 4096 // max number of bytes we can get at once 

void sigchld_handler(int /*s*/)
{	
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

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
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;
	int numbytes;


	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	char* port_number = PORT;
	if (argc == 2) {
		port_number = argv[1];
		// perror("invalid port number");
	}

	// resolve on local address. argv[1] should be port number.
	if ((rv = getaddrinfo(NULL, port_number, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	// server is set up at this point, now listen for incoming connection on the bound socket.
	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	// not sure how this works exactly, but can prune dead child processes.
	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	// initialize the upload buffer
	char buf[MAXDATASIZE];

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		// picking up incomming connections.
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		// this line actually CREATES the child process.
		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener

			printf("process forked success\n");

			// start recieving data
			recv(new_fd, buf, MAXDATASIZE, 0);
			string inbuf = buf;
			printf("forked, buffer initialized");

            // check whether this is a GET request.
			if (inbuf.find_first_of("GET ") == 0) {
				inbuf = inbuf.substr(4);
				if (inbuf.find("HTTP") != inbuf.npos) {
					string path = inbuf.substr(1, inbuf.find(" HTTP") - 1);
					if (path.find("home") == 0) {
						path = "/"+path;
					}
					printf("path is %s\n", path.data());
					// open the specified file here.
					FILE *return_file = nullptr; 
					return_file = fopen(path.data(), "rb");
<<<<<<< HEAD
					printf("reading return file");
=======
>>>>>>> source/main
					if (return_file != NULL) {
						printf("return file found %s\n", path.data());
                		strcpy(buf, "HTTP/1.1 200 OK\r\n\r\n");
                		send(new_fd, buf, strlen(buf), 0); // Send the HTTP header
                		// Read file content into buf and send w/ file stream.
						while ((numbytes = fread(buf, 1, MAXDATASIZE, return_file)) > 0) {
							send(new_fd, buf, numbytes, 0);
						}
						fclose(return_file);

					// handle the bad requests.
					} else {
						strcpy(buf, "HTTP/1.1 404 Not Found\r\n\r\n");
						send(new_fd, buf, strlen(buf), 0);
					}
				} else {
					strcpy(buf, "HTTP/1.1 400 Bad Request\r\n\r\n");
					send(new_fd, buf, strlen(buf), 0);
				}
			} else {
				strcpy(buf, "HTTP/1.1 400 Bad Request\r\n\r\n");
				send(new_fd, buf, strlen(buf), 0);
			}


			close(new_fd);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}