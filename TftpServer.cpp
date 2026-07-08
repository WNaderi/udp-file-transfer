//
// Created by B Pan on 12/3/23.
//
//for system calls, please refer to the MAN pages help in Linux
//TFTP server program over udp - CSS432 - winter 2024
#include <cstdio>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <filesystem>
#include "TftpCommon.cpp"

#define SERV_UDP_PORT 61125

char *program;

int createInitPack(int& reqtype, std::fstream& file, int& whereweare, int& sockfd, struct sockaddr_storage* to) {

    if (reqtype == TFTP_WRQ) { //sending data to client

        char buffer[MAX_PACKET_LEN];

        char* bpt = buffer;

        unsigned short * opCode = (unsigned short *) bpt;

        *opCode = htons(3);

        unsigned short * blockNum = opCode + 1;

        *blockNum = htons(whereweare);

        char* data = bpt + 4;

        char dataBuff[MAX_DATA_SIZE];

        file.read(data, MAX_DATA_SIZE);
        std::cout << "Pointer: " << file.tellg() << std::endl;
        //memcpy(data, dataBuff, file.gcount());

        char ackBuffer[MAX_PACKET_LEN];

        std::cout << data << std::endl;

        std::cout << "Sent init data" << std::endl;
        printBuffer(bpt, 4+file.gcount());
        sendData(sockfd, bpt, 4+file.gcount(), to, ackBuffer);
        std::cout << "Init data sent." << std::endl;
        //Should get ACK 0, need to process and check
        if (file.gcount() < MAX_DATA_SIZE) { //this the only packet we need to send!
            bool ackRecieved = false;
            while (!ackRecieved) {

                unsigned short * ackoPCode = (unsigned short*) ackBuffer;

                unsigned short * ackBlockNum = ackoPCode+1;
                if (ntohs(*ackoPCode) == 4) {
                    if (ntohs(*ackBlockNum) == whereweare) {
                        whereweare++; // Sent first data, now expecting ack for first data (Ack 0)
                        return 2;

                    }

                }


            }

        }

        char* apt = ackBuffer;

        if (!file) {

            throw std::runtime_error("uh ohhhhh");

        }

        unsigned short * ackOpCode = (unsigned short *) apt;
        unsigned short * ackBlockNum = ackOpCode + 1;

        bool replyRecieved = false;
        while (!replyRecieved) {

            if (ntohs(*ackOpCode) == 4) {
                
                std::cout << "Expected: " << whereweare << " Recieved: " << ntohs(*ackBlockNum) << std::endl;
                if (ntohs(*ackBlockNum) == whereweare) {

                    whereweare++;
                    replyRecieved = true;

                    return 1;

                } else {

                    sendData(sockfd, bpt, 4+file.gcount(), to, ackBuffer);

                }

            } else {

                std::cerr << "recieved wrong init opCode" << std::endl;

                char* message = "Expected different init. OpCode";


                sendError(sockfd, to, 0, message, strlen(message));
                return -1;

            }

        }



    } else if (reqtype == TFTP_RRQ) { // Downloading from client (Sending ACKs)

        char buffer[MAX_PACKET_LEN];

        char* bpt = buffer;

        unsigned short * opCode = (unsigned short *) bpt;

        *opCode = htons(4);

        unsigned short * blockNum = opCode + 1;

        *blockNum = htons(whereweare);

        size_t packSize = 4;

        char dataBuff[MAX_PACKET_LEN];

        ssize_t dSize = sendData(sockfd, bpt, packSize, to, dataBuff);

        whereweare++;

        //Should get first piece of data, need to write it

        char* dpt = dataBuff;

        unsigned short * dOpCode = (unsigned short *) dpt;

        unsigned short * dBlockNum = dOpCode + 1;

        bool replyRecieved = false;

        while (!replyRecieved) {

            if (ntohs(*dOpCode) == 3) {

                if (ntohs(*dBlockNum) == whereweare) {
                    
                    file.write(dpt+4, dSize-4);
                    replyRecieved = true;
                    if (dSize - 4 < MAX_DATA_SIZE) {

                        //send a final ACK
                        char ackBuffer[MAX_PACKET_LEN];
                        unsigned short * ackOp = (unsigned short*) ackBuffer;
                        *ackOp = htons(4);

                        unsigned short * ackBlock = ackOp + 1;

                        *ackBlock = htons(whereweare);

                        sendto(sockfd, ackBuffer, 4, 0, (sockaddr*) to, sizeof(sockaddr_storage));

                        return 2;

                    }
                    return 1;

                } else {

                    sendData(sockfd, bpt, packSize, to, dataBuff);

                }

            } else {

                std::cerr << "recieved wrong init opCode. Recieved: " << ntohs(*dOpCode) << std::endl;

                char* message = "Expected different init. OpCode";

                sendError(sockfd, to, 0, message, strlen(message));
                throw std::runtime_error("Error sent");

            }

        }

    }

    return -1;

}

void handleIncomingRequest(int sockfd) {

    struct sockaddr cli_addr;

    int reqtype;

    /*
     * TODO: define necessary variables needed for handling incoming requests.
     */

    std::fstream file;

    for (;;) {

        /*
         * TODO: Receive the 1st request packet from the client
         */

        try {
        
            char buffer[MAX_PACKET_LEN];

            socklen_t clilen = sizeof(struct sockaddr);

            ssize_t packetLength = recvfrom(sockfd, buffer, MAX_PACKET_LEN, 0, (struct sockaddr *)&cli_addr, &clilen);

            std::cout << "Recieved a init. packet" << std::endl;

            if (packetLength < 0) {// expecting rrq or wrq

                throw std::runtime_error("Error recieving initial req packet");
                
            }

            /*
            * TODO: Parse the request packet. Based on whether it is RRQ/WRQ, open file for read/write.
            * Create the 1st response packet, send it to the client.
            */

            char* bpt = buffer;

            unsigned short * opCode = (unsigned short *) bpt;

            std::string filename;

            extractFilenameFromPacket(bpt, packetLength, filename);

            std::string filePath = "server-files/" + std::string(filename);

            int whereweare;

            if (ntohs(*opCode) == 1) { //RRQ (We are uploading)

                if (!std::filesystem::exists(filePath)) { //File does not exist... send Error

                    char* message = "File does not exist on Server";

                    sendError(sockfd, (sockaddr_storage*)&cli_addr, 1, message, strlen(message));
                    throw std::runtime_error(message);

                }

                file.open(filePath, std::ios::in | std::ios::binary);
                
                reqtype = TFTP_WRQ; //FLipped to WRQ because we are writing to the client

                whereweare = 1;


            } else if (ntohs(*opCode) == 2) { // WRQ (We are downloading)

                if (std::filesystem::exists(filePath)) { //File already exists... Send Error

                    char* message = "File already exists on Server";

                    sendError(sockfd, (sockaddr_storage*)&cli_addr, 6, message, strlen(message));
                    throw std::runtime_error(message);

                }

                file.open(filePath, std::ios::out | std::ios::binary);
                reqtype = TFTP_RRQ; // FLipped to RRQ because we are reading from client

                whereweare = 0;
                

            } else if (ntohs(*opCode) < 1 || ntohs(*opCode) > 5) { //Illegal OPCODE

                    char* message = "Illegal OpCode";

                    sendError(sockfd, (sockaddr_storage*)&cli_addr, 4, message, strlen(message));

                    throw std::runtime_error(message);

            }

            std::cout << filePath << std::endl;

            if (file.is_open() && file.good()) {
                std::cout << "The file is open and ready for I/O operations." << std::endl;
            } else {

                throw std::runtime_error("File is not open or good");

            }

            int state = createInitPack(reqtype, file, whereweare, sockfd, (sockaddr_storage*)&cli_addr);
            std::cout << "wwa" << whereweare << std::endl;

            if (state == -1) {

                throw std::runtime_error("Error during init. exchange");

            } else if (state == 2) { // Exchanged data in the initial exchange sequence.

                std::cout << "Finished file transfer process!" << std::endl;

            } else {

                std::cout << "Init. packet sent." << std::endl;

                /*
                * TODO: process the file transfer
                */
                std::cout << "Beginning file transfer" << std::endl;

                processFileTransfer(reqtype, sockfd, (sockaddr_storage*)&cli_addr, file, whereweare);

            }

            

            /*
            * TODO: Don't forget to close any file that was opened for read/write, close the socket, free any
            * dynamically allocated memory, and necessary clean up.
            */

            file.close();

        }

        catch (...) { // catch any exception

            std::cerr << "Server Error. Resume Listening..." << std::endl;
            
            if (file.is_open()) {
                file.close();
            }

        }
        
    }
}

int main(int argc, char *argv[]) {

    std::cout << "Starting Server!" << std::endl;
    
    program=argv[0];

    int sockfd;
    struct sockaddr_in serv_addr;

    memset(&serv_addr, 0, sizeof(serv_addr));

    /*
     * TODO: initialize the server address, create socket and bind the socket as you did in programming assignment 1
     */

    sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        std::cerr << "Error Making Socket" << std::endl;
    
    serv_addr.sin_addr.s_addr = htons(INADDR_ANY);
    serv_addr.sin_port = htons(SERV_UDP_PORT);
    serv_addr.sin_family = AF_INET;

    if (bind(sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
        std::cerr << "Bind Error" << std::endl;

    if (registerTimeoutHandler() == -1) {

        throw std::runtime_error("^");

    }


    handleIncomingRequest(sockfd);

    close(sockfd);
    return 0;
}