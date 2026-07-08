//
// Created by B Pan on 12/3/23.
//

//TFTP client program - CSS 432 - Winter 2024

#include <cstdio>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include "TftpError.h"
#include "TftpOpcode.h"
#include "TftpCommon.cpp"
#include <fstream>

#define SERV_UDP_PORT 61125
#define SERV_HOST_ADDR "127.0.0.1"

/* A pointer to the name of this program for error reporting.      */
char *program;

/* The main program sets up the local socket for communication     */
/* and the server's address and port (well-known numbers) before   */
/* calling the processFileTransfer main loop.                      */


int sendrequest(int& reqtype, char* name, char* mode, int& sockfd, struct sockaddr_storage* to, int& whereweare, std::fstream& file) {

    //Create REQ packet

    int nameLen = strlen(name)+1; //+1 accounts for empty byte (which represents null terminator)
    int modeLen = strlen(mode)+1;

    char buffer[MAX_PACKET_LEN];

    char* bpt = buffer;

    unsigned short* opCode = (unsigned short*) bpt; //pointer to packets OPCODE

    *opCode = htons(reqtype);

    char* fileName = bpt+2;

    memcpy(fileName, name, nameLen);

    char* Mode = fileName + nameLen;

    memcpy(Mode, mode, modeLen);


    // Packet created! ready to send...
    int packSize = 2 + nameLen + modeLen;

    char replyBuff[MAX_PACKET_LEN];

    std::cout << "Sending Client REQ" << std::endl;
    int recvBytes = sendData(sockfd, bpt, packSize, to, replyBuff); // Send packet throught the internet!!

    if (recvBytes < 0) {

        throw std::runtime_error("Error getting init. REQ reply");

    }

    char* rpt = replyBuff;
    unsigned short* rOpCode = (unsigned short*) rpt;
    unsigned short* rBlockNum = rOpCode + 1;

        if (reqtype == TFTP_RRQ) { //Expecting first data packet

            if (recvBytes > 4) {
                //first make sure we got the right packet
                if (ntohs(*rOpCode) == 3) {

                    if (ntohs(*rBlockNum) == whereweare) {

                        
                        file.write(replyBuff+4, recvBytes-4);

                        if (recvBytes < MAX_PACKET_LEN) {
                            std::cout << recvBytes << std::endl;

                            char finACK[MAX_PACKET_LEN];
                            
                            unsigned short* finOpCode = (unsigned short*) finACK;

                            *finOpCode = htons(4);

                            unsigned short * finBlockNum = finOpCode+1;

                            *finBlockNum = htons(whereweare);

                            sendto(sockfd, finACK, 4, 0, (sockaddr*) to, sizeof(sockaddr_storage));

                            return 1;

                        }

                    }

                } else {

                    std::cerr << "Recieved incorrect packet. Expected a Data Packet. Recieved: " << ntohs(*rOpCode) << std::endl;
                    return -1;

                }
                
            } else if (recvBytes == 4) { // Empty file!

                //send ack!
                char finAck[MAX_PACKET_LEN];
                unsigned short * opCode = (unsigned short*) finAck;
                *opCode = htons(4);
                unsigned short * blockNum = opCode + 1;
                *blockNum = htons(whereweare);

                sendto(sockfd, finAck, 4, 0, (sockaddr*) to, sizeof(sockaddr_storage));
                return 1;

            } else {

                std::cerr << "Expected data. Terminating." << std::endl;
                return -1;

            }
            
        } else if (reqtype == TFTP_WRQ) { // Expecting ACK 1

            if (ntohs(*rOpCode) == 4) {

                if (ntohs(*rBlockNum) == whereweare) {

                    std::cout << "Recieved ACK: " << ntohs(*rBlockNum) << std::endl;
                    file.write(replyBuff+4, recvBytes-4);
                    whereweare++;

                }

            } else {

                std::cerr << "Recieved incorrect packet. Expected a ACK Packet. Recieved: " << ntohs(*rOpCode) << std::endl;
                return -1;

            }

        }


    std::cout << "Init packet sent." << std::endl;


    return 0;

    // Now get ACK packet or the OK Send Data packet
    
}

int main(int argc, char *argv[]) {
    program = argv[0];

    int sockfd;
    struct sockaddr_in cli_addr, serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    memset(&cli_addr, 0, sizeof(cli_addr));

    /*
     * TODO: initialize server and client address, create socket and bind socket as you did in
     * programming assignment 1
     */

    serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERV_UDP_PORT);

    cli_addr.sin_addr.s_addr = INADDR_ANY;
    cli_addr.sin_family = AF_INET;
    cli_addr.sin_port = htons(0);

    sockfd = socket(PF_INET, SOCK_DGRAM, 0);

    if (sockfd == -1)
        std::cerr << "Socket Creation Error" << std::endl;

    if (bind(sockfd, (sockaddr*)&cli_addr, sizeof(cli_addr)) < 0) {

        std::cerr << "Bind Error" << std::endl;

    }

    if (registerTimeoutHandler() == -1) {

        throw std::runtime_error("^");

    }

    /*
     * TODO: Verify arguments, parse arguments to see if it is a read request (r) or write request (w),
     * parse the filename to transfer, open the file for read or write
     */

    std::fstream file;

    if (argc <= 1) {

        throw std::runtime_error("Missing args.");

    }

    int reqtype;
    char* filename = argv[2];

    std::string filePath = "client-files/" + std::string(filename);

    int whereweare; //mark next byte host is expecting to send/recieve.

    if (argv[1][0] == 'w') { // ### WRQ (Upload to server)

        file.open(filePath, std::ios::in | std::ios::binary);
        reqtype = TFTP_WRQ;

        whereweare = 0;

    } else if (argv[1][0] == 'r') { // ### RRQ (Download from server)


        file.open(filePath, std::ios::out | std::ios::trunc | std::ios::binary);
        reqtype = TFTP_RRQ;

        whereweare = 1;

    } else {

        throw std::runtime_error("No given request");

    }

    std::cout << filePath << std::endl;

    if (file.is_open() && file.good()) {
            std::cout << "The file is open and ready for I/O operations." << std::endl;
    } else {

        throw std::runtime_error("File is not open or good");

    }

    /*
     * TODO: create the 1st tftp request packet (RRQ or WRQ) and send it to the server via socket.
     * Remember to use htons when filling the opcode in the tftp request packet.
     */

    int state = sendrequest(reqtype, filename, "octet", sockfd, (sockaddr_storage*)&serv_addr, whereweare, file);
    std::cout << "wwa" << whereweare << std::endl;
    if (state == 1) {

        std::cout << "Transfer complete!" << std::endl;

    } else if (state == -1) {

        throw std::runtime_error("GG");

    } else {

    /*
     * TODO: process the file transfer
     */
    std::cout << "Beginning file transfer" << std::endl;
    processFileTransfer(reqtype, sockfd, (sockaddr_storage*)&serv_addr, file, whereweare);

    }

    /*
     * TODO: Don't forget to close any file that was opened for read/write, close the socket, free any
     * dynamically allocated memory, and necessary clean up.
     */

    file.close();
    close(sockfd);

    exit(0);
}
