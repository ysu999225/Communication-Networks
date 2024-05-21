#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>
#include <math.h>
#include <iostream>
#include <queue>

using namespace std;

#define MSS 1400
#define BUFFER_SIZE 300
#define RTT 30*1000
#define MIN_CWND 16

// Packet structure used for transferring
#define DATA 0
#define ACK 2
#define FIN 3
#define FIN_ACK 4
typedef struct {
    int data_size;
    int seq_num;
    int ack_num;
    int msg_type;
    char data[MSS];
} packet;

FILE *fp;
unsigned long long int num_pkt_total;
unsigned long long int bytesToRead;

// socket setup
struct sockaddr_storage their_addr;
socklen_t addr_len = sizeof their_addr;
struct addrinfo hints, *recvinfo, *p;
int numbytes;

// window setup
double cwnd = 1.0;
int ssthresh = 512;
int duplicate_acks = 0;
enum socket_state { SLOW_START, CONGESTION_AVOID, FAST_RECOVERY, FIN_WAIT };
int congestion_state = SLOW_START;

unsigned long long int seq_number = 0;
// The buffer used for current sending
char send_buf[sizeof(packet)];
// The queue for data yet to be sent
queue<packet> buffer;
// The queue for data awaiting acknoledgement
queue<packet> wait_ack;

void diep(char *s) {
    perror(s);
    exit(1);
}

// Set socket timeout
void timeOut(int socket) {
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = RTT;
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        diep("timeOut");
        return;
    }
}

// Fill packet buffer
int fillBuffer(int packet_number) {
    if (packet_number == 0) {
        return 0;
    }
    int count = 0;

    for (int i = 0; i < packet_number; ++i){
        if (bytesToRead == 0) break;
        packet pkt;
        char temp_buf[MSS];
        int packet_byte = min(static_cast<unsigned long long>(bytesToRead), static_cast<unsigned long long>(MSS));
        int read_size = fread(temp_buf, sizeof(char), packet_byte, fp);
        if (read_size > 0) {
            pkt.data_size = read_size;
            pkt.msg_type = DATA;
            pkt.seq_num = seq_number;
            memcpy(pkt.data, &temp_buf, sizeof(char) * packet_byte);
            buffer.push(pkt);
            seq_number = seq_number + 1;
        }
        count = i;
        bytesToRead -= read_size;
    }
    return count;
}

// Send packets
void sendPackets(int socket) {
    int packets_tosend =(cwnd - wait_ack.size()) <= buffer.size() ? cwnd - wait_ack.size() : buffer.size();
    // Handle the case where no new buffer is needed to send, resend oldest unacked data.
    if (cwnd - wait_ack.size() < 1) {
        memcpy(send_buf, &wait_ack.front(), sizeof(packet));
        // send one packet
        if ((numbytes = sendto(socket, send_buf, sizeof(packet), 0,p->ai_addr, p->ai_addrlen)) == -1) {
            diep("sending");
        }
        cout << "packet sent: " << wait_ack.front().seq_num << endl;
        cout << "window size: " << cwnd << endl;
        return;
    }
    if (buffer.empty()) {
        cout << "packet buffer empty" <<endl;
        return;
    }
    // Now handle the case of multiple send required
    for (int i = 0; i < packets_tosend; ++i) {
        memcpy(send_buf, &buffer.front(), sizeof(packet));
        if ((numbytes = sendto(socket, send_buf, sizeof(packet), 0, p->ai_addr, p->ai_addrlen)) == -1) {
            diep("sending");
        }
        cout << "packet sent: " << buffer.front().seq_num << endl;
        cout << "window size: " << cwnd << endl;
        // Move packet to the awaiting ack queue
        wait_ack.push(buffer.front());
        buffer.pop();
    }
    fillBuffer(packets_tosend);
}


// Congestion control algorithm
void congestionControl(bool isNew, bool timeout) {
    switch (congestion_state) {
        case SLOW_START:
            if (timeout) {
                ssthresh = cwnd / 2.0;
                cwnd = 1.0;
                duplicate_acks = 0;
                return;
            }
            if (isNew) {
                cwnd = (cwnd*2 >= BUFFER_SIZE) ? BUFFER_SIZE-1 : cwnd*2;
                duplicate_acks = 0;
            } else {
                duplicate_acks++;
            }
            if (cwnd >= ssthresh) {
                congestion_state = CONGESTION_AVOID;
            }
            break;
        case CONGESTION_AVOID:
            if (timeout) {
                ssthresh = cwnd / 2.0;
                cwnd = MIN_CWND;
                if (ssthresh < MIN_CWND) ssthresh = MIN_CWND;
                duplicate_acks = 0;
                congestion_state = SLOW_START;
                return;
            }
            if (isNew) {
                cwnd = (cwnd + 1.0/cwnd < BUFFER_SIZE) ? cwnd + 1.0/cwnd : BUFFER_SIZE - 1;
                duplicate_acks = 0;
            } else {
                duplicate_acks++;
            }
            break;
        case FAST_RECOVERY:
            if (timeout) {
                ssthresh = cwnd / 2.0;
                cwnd = MIN_CWND;
                if (ssthresh < MIN_CWND) ssthresh = MIN_CWND;
                duplicate_acks = 0;
                congestion_state = SLOW_START;
                return;
            }
            if (isNew) {
                cwnd = ssthresh;
                duplicate_acks = 0;
                congestion_state = CONGESTION_AVOID;
            } else {
                cwnd = (cwnd + 1.0/cwnd < BUFFER_SIZE) ? cwnd + 1.0/cwnd : BUFFER_SIZE - 1;
            }
            break;
        default:
            break;
    }
}

// Get socket
int getSocket(char *hostname, unsigned short int hostUDPport) {
    int rv, sockfd;
    char portStr[10];
    sprintf(portStr, "%d", hostUDPport);
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    memset(&recvinfo, 0, sizeof recvinfo);
    if ((rv = getaddrinfo(hostname, portStr, &hints, &recvinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // Loop through all the results and bind to the first one
    for (p = recvinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: error opening socket");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }
    return sockfd;
}

// FIN processing
void FIN_processing(int socket) {
    packet pkt;
    while (true) {
        pkt.msg_type = FIN;
        pkt.data_size = 0;
        memcpy(send_buf, &pkt, sizeof(packet));
        if ((numbytes = sendto(socket, send_buf, sizeof(packet), 0, p->ai_addr, p->ai_addrlen)) == -1) {
            diep("sending FIN");
        }
        packet ack;
        if ((numbytes = recvfrom(socket, send_buf, sizeof(packet), 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
            diep("could not recieve FIN");
        }
        memcpy(&ack, send_buf, sizeof(packet));
        if (ack.msg_type == FIN_ACK) {
            cout << "Received FIN_ACK." << endl;
            break;
        }
    }
}

// Reliably transfer function
void reliablyTransfer(char *hostname, unsigned short int hostUDPport, char *filename, unsigned long long int bytesToTransfer) {
    // opening file to send.
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        diep("open file");
    }
    bytesToRead = bytesToTransfer;
    num_pkt_total = (unsigned long long int)ceil(bytesToRead * 1.0 / MSS);
    // cout << num_pkt_total << endl;

    // initiate socket.
    int socket = getSocket(hostname, hostUDPport);

    // sending packets with helper functions.
    fillBuffer(BUFFER_SIZE);
    timeOut(socket);
    sendPackets(socket);
    // processing returned acks.
    while (!buffer.empty() || !wait_ack.empty()) {
        if ((numbytes = recvfrom(socket, send_buf, sizeof(packet), 0, NULL, NULL)) == -1) {
            if (errno != EAGAIN || errno != EWOULDBLOCK) {
                diep("Not receiving ack");
            }
            cout << "Time out, resending " << wait_ack.front().seq_num << endl;
            memcpy(send_buf, &wait_ack.front(), sizeof(packet));
            if ((numbytes = sendto(socket, send_buf, sizeof(packet), 0, p->ai_addr, p->ai_addrlen)) == -1) {
                cout << "Fail to send " << wait_ack.front().seq_num << " pkt" << endl;
                diep("Error: data sending");
            }
            congestionControl(false, true);

        } else {
            packet pkt;
            memcpy(&pkt, send_buf, sizeof(packet));
            if (pkt.msg_type == ACK) {
                cout << "received ack" << pkt.ack_num << endl;
                if (pkt.ack_num == wait_ack.front().seq_num) {
                    congestionControl(false, false);
                    if (duplicate_acks == 3) {
                        ssthresh = cwnd / 2.0;
                        cwnd = ssthresh + 3;
                        duplicate_acks = 0;
                        congestion_state = FAST_RECOVERY;
                        cout << "3 duplicate tp FAST_RECOVERY, cwnd = " << cwnd << endl;
                        // Resend duplicated packet
                        memcpy(send_buf, &wait_ack.front(), sizeof(packet));
                        if ((numbytes = sendto(socket, send_buf, sizeof(packet), 0, p->ai_addr, p->ai_addrlen)) == -1) {
                            cout << "Fail to send " << wait_ack.front().seq_num << " pkt" << endl;
                            diep("sending data");
                        }
                        cout << "3 duplicate ACKs, resend pkt " << wait_ack.front().seq_num << endl;
                    }
                } else if (pkt.ack_num > wait_ack.front().seq_num) {
                    while (!wait_ack.empty() && wait_ack.front().seq_num < pkt.ack_num) {
                        congestionControl(true, false);
                        wait_ack.pop();
                    }
                    sendPackets(socket);
                }
            }
        }
    }
    fclose(fp);

    // processing finish message.
    FIN_processing(socket);
}

//-------
int main(int argc, char **argv) {
    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int)atoi(argv[2]);
    numBytes = atoll(argv[4]);

    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);
    return 0;
}