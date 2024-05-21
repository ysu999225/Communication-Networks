#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <queue>
#include<iostream>
#include<fstream>


using namespace std;

#define MSS 2000
#define BUFFER_SIZE 300
#define RTT 20 * 1000

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


struct sockaddr_in si_me, si_other;

int s;
socklen_t slen = sizeof (si_other);
// the last acknowledged sequence number.
// same as the highest act sequence 
// suggest that there is no packets have got yet
int last_ack_sent = -1; 
//check the status of FIN case
bool expect_fin = false;


FILE *fp;

void diep(char *s) {
    perror(s);
    exit(1);
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    int s;
    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        diep("socket");
    }

    memset((char *) &si_me, 0, sizeof (si_me));
    memset((char *) &si_other, 0, sizeof (si_other));

    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1) {
        diep("bind");
    }
    // open the file
    fp = fopen(destinationFile, "wb");
    if (fp == NULL) {
        diep("fopen");
    }


    /* Now receive data and send acknowledgements */  
    int numbytes;
    // use stack here to allocate memory
    packet recv_packet; 
    cout<<"ready to recieve"<<endl;
    while (!expect_fin) {
        //socklen_t slen = sizeof(si_other);
        numbytes = recvfrom(s, &recv_packet, sizeof(recv_packet), 0, (struct sockaddr *) &si_other, &slen);
        cout<<"recieved "<<recv_packet.data_size<<" bytes, current seq: "<<recv_packet.seq_num<<endl;
        if (numbytes == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                cout << "receive timeout" << endl;
                break; 
            } else {
                diep("recvfrom"); 
            }
        } else if (numbytes == 0) {
            cout << "No data" << endl;
            break;
        } else {
        // type determined
            switch (recv_packet.msg_type) {
                case DATA:
                    // write its data to the file
                    cout<<"is data, last ack sent: "<<last_ack_sent<<endl;
                    if (recv_packet.seq_num == last_ack_sent + 1) {
                        fwrite(recv_packet.data, 1, recv_packet.data_size, fp);
                        last_ack_sent = recv_packet.seq_num;
                    }
                    // send an ACK for the last in-order packet received
                    packet ack_packet;
                    memset(&ack_packet, 0, sizeof(ack_packet)); 
                    ack_packet.ack_num = last_ack_sent + 1;
                    ack_packet.msg_type = ACK;
                    sendto(s, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)&si_other, slen);
                    break;
              //case ACK:
                case FIN:
                // bool expecting_fin = true;
                    expect_fin = true;
                    // send an ACK for the FIN packet here
                    packet fin_ack_packet;
                    memset(&fin_ack_packet, 0, sizeof(fin_ack_packet)); 
                    fin_ack_packet.ack_num = last_ack_sent; 
                    fin_ack_packet.msg_type = FIN_ACK;
                    sendto(s, &fin_ack_packet, sizeof(fin_ack_packet), 0, (struct sockaddr *)&si_other, slen);
                    break;
               //case FIN_ACK
            }
        }
    }
    fclose(fp);
    close(s);
    printf("%s received.", destinationFile);
    return;
}


/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}