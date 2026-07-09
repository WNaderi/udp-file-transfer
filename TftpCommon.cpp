//
// Created by B Pan on 1/15/24.
//

#include <cstdio>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <csignal>
#include <chrono>
#include <thread>
#include <cstring>
#include "TftpError.h"
#include "TftpOpcode.h"
#include "TftpConstant.h"
#include <iostream>
#include <fstream>

// To track how retransmit/retry has occurred.
static int retryCount = 0;

// Helper function to print the first len bytes of the buffer in Hex
static void printBuffer(const char * buffer, unsigned int len) {
    for(int i = 0; i < len; i++) {
        printf("%x,", buffer[i]);
    }
    printf("\n");
}

// increment retry count when timeout occurs. 
static void handleTimeout(int signum ){
    retryCount++;
    printf("timeout occurred! count %d\n", retryCount);
}

static int registerTimeoutHandler( ){
    signal(SIGALRM, handleTimeout);

    /* disable the restart of system call on signal. otherwise the OS will be stuck in
     * the system call
     */
    if( siginterrupt( SIGALRM, 1 ) == -1 ){
        printf( "invalid sig number.\n" );
        return -1;
    }
    return 0;
}

/*
 * Useful things:
 * alarm(1) // set timer for 1 sec
 * alarm(0) // clear timer
 * std::this_thread::sleep_for(std::chrono::milliseconds(200)); // slow down transmission
 */

// Build and send a TFTP ERROR packet with the provided error code and message.
inline void sendError(int& sockfd, struct sockaddr_storage* to, int errorNum, char* error, size_t __N) {

    char errorBuff[MAX_PACKET_LEN];

    char* ept = errorBuff;

    unsigned short * opCode = (unsigned short *) ept;

    *opCode = htons(5);

    unsigned short * errCode = opCode + 1;

    *errCode = htons(errorNum);

    char* message = (char*) (errCode+1);

    memcpy(message, error, __N);

    if (sendto(sockfd, ept, __N+5, 0, (sockaddr*) to, sizeof(sockaddr_storage)) < 0) {
            std::cerr << "Error sending error packet" << " *SkullEmoji*" << std::endl;
    }

}


inline ssize_t sendData(int& sockfd, char* bpt, const int& __N, struct sockaddr_storage* to, char* replyBuffer) {

    bool replyRecieved = false; // Flag to indicate if reply has been received

    int recvResult;

    while (retryCount < MAX_RETRY_COUNT && !replyRecieved) {
        // Send the data packet

        int size = sendto(sockfd, bpt, __N, 0, (sockaddr*) to, sizeof(sockaddr_storage));

        if (size < 0) {
            std::cerr << "Error sending data packet" << std::endl;
        }

        int fromlen = sizeof(sockaddr_storage);

        // Attempt to receive the ACK

        alarm(RETRY_SECONDS);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        recvResult = recvfrom(sockfd, replyBuffer, MAX_PACKET_LEN, 0, (sockaddr*) to, (socklen_t*)&fromlen);
        if (recvResult < 0) {
            // Check if the error was due to a timeout
            if (errno == EINTR) {
                continue;
            } else {
                std::cerr << "Error receiving reply" << std::endl;
            }
        } else {
            std::cout << "Got reply..." << std::endl;
        }

        return recvResult;
    }

    if (!replyRecieved) {
        std::cerr << "Failed to receive Reply after " << MAX_RETRY_COUNT << " retries." << std::endl;
        return -1;
    }

    return -1;

}

/*
* initBuffer = pointer to initial packet
* len = length of initial packet
* file = file we are reading data from to send
* Returns: size in bytes of data sent
*/
inline int processWriting(std::fstream& file, int& sockfd, struct sockaddr_storage* to, int& whereweare) { //Writing (sending data to another machine)

    // Read the file in 512-byte chunks and wrap each chunk in a DATA packet.
    char buffer[MAX_DATA_SIZE];

    bool writedone = false;

    char* bpt = buffer;

    if (!file) {
        std::cerr << "File is not open or has encountered an error." << std::endl;
        throw std::runtime_error("STOP");
        return -1;
    }

    char packBuffer[MAX_PACKET_LEN];
    while (!writedone) {

        if (file.eof()) {

            writedone = true;
            break;

        }
        std::cout << "Pointer: " << file.tellg() << std::endl;
        file.read(buffer, MAX_DATA_SIZE);
        
        int bytes_read = file.gcount(); // How many bytes were actually readasd

        std::cout << bytes_read << std::endl;

        // DATA packets contain opcode, block number, then up to 512 bytes of payload.
        unsigned short* opCode = (unsigned short *) packBuffer;

        *opCode = htons(3);

        unsigned short * blockNum = opCode + 1;

        *blockNum = htons(whereweare);

        char* data = (char*) blockNum + sizeof(unsigned short);

        memcpy(data, buffer, bytes_read);

        std::cout << data << std::endl;

        // Send the block and wait for a matching ACK before advancing the block number.
        char ackBuffer[MAX_PACKET_LEN];

        std::cout << "Sending Block: " << ntohs(*blockNum) << std::endl;
        if (sendData(sockfd, packBuffer, bytes_read+4, to, ackBuffer) < 0) {

            std::cerr << "Error sending Data" << std::endl;
            return -1;

        }

        bool replyRecieved = false;

        unsigned short * ackOpCode = (unsigned short *) ackBuffer;

        while (!replyRecieved) {

            if (ntohs(*ackOpCode) == 4) {
                // Check ACK packet for correct block number
                unsigned short* blockNumber = (unsigned short*)(ackBuffer + 2);
                if (ntohs(*blockNumber) == whereweare) {
                    replyRecieved = true;
                    std::cout << "received ACK for block#: " << whereweare << std::endl;
                    whereweare++; // Prepare for the next block

                } else {
                    std::cerr << "Received different ACKblock#: " << ntohs(*blockNumber) << ". Expected: " << whereweare << std::endl;
                    std::cerr << "Resending DATA Pack." << std::endl;
                    if (sendData(sockfd, packBuffer, bytes_read+4, to, ackBuffer) < 0) {

                        std::cerr << "Error sending data" << std::endl;

                    }
                }

            } else if (ntohs(*ackOpCode) == 5) {

                std::cerr << "Recieved Error Pack. Terminating transmission." << std::endl;
                return -1;

            } else if (ntohs(*ackOpCode) < 1 || ntohs(*ackOpCode) > 5) { //Illegal OPCODE

                    char* message = "Illegal OpCode";

                    sendError(sockfd, to, 4, message, strlen(message));
                    
                    throw std::runtime_error(message);

            }
        }

    }

    std::cout << "File transfer complete!" << std::endl;
    return 1;

}

inline int processReading(int& sockfd, std::fstream& file, struct sockaddr_storage* from, int& whereweare) {

    if (!file) {
        std::cerr << "File is not open or has encountered an error." << std::endl;
        return -1;
    }

    char dataBuffer[MAX_PACKET_LEN];
    bool transferComplete = false;

    while (!transferComplete) {

        char ackPack[MAX_PACKET_LEN];
        char* apt = ackPack;

        unsigned short * opCode = (unsigned short*) apt;
        *opCode = htons(4);
        unsigned short * blockNum = opCode + 1;
        *blockNum = htons(whereweare);

        ssize_t ackSize = 4;
        std::cout << "Sending ACK Pack for block: " << ntohs(*blockNum) << std::endl;
        ssize_t packetLen = sendData(sockfd, apt, ackSize, from, dataBuffer);
        whereweare++; //send x ack, recieved next data in sendData so now we expecting to recieve x+1 ack.
        std::cout << "whereweare: " << whereweare << std::endl;
        if (packetLen < 0) {

            std::cerr << "Error sending Data" << std::endl;
            return -1;

        }

        bool replyRecieved = false;
        if (packetLen > 0) {

            unsigned short* dOpCode = (unsigned short*) dataBuffer;

            unsigned short* blockNumber = (unsigned short*)(dataBuffer + 2);

            std::cout << "Data OPCODE: " << ntohs(*dOpCode) << " blockNum: " << ntohs(*blockNumber) << std::endl;
            while (!replyRecieved) {

                if (ntohs(*dOpCode) == 3) {

                    if (ntohs(*blockNumber) == whereweare) {

                        ssize_t dataLen = packetLen - 4; // Subtracting opcode and block number
                        if (dataLen > 0) {
                            file.write(dataBuffer + 4, dataLen);
                        }
                        if (dataLen < MAX_DATA_SIZE) { // Check if this is the last packet
                            transferComplete = true;
                            //send last ack:
                            //send a final ACK
                            char ackBuffer[MAX_PACKET_LEN];
                            unsigned short * ackOp = (unsigned short*) ackBuffer;
                            *ackOp = htons(4);

                            unsigned short * ackBlock = ackOp + 1;

                            *ackBlock = htons(whereweare);

                            sendto(sockfd, ackBuffer, 4, 0, (sockaddr*) from, sizeof(sockaddr_storage));
                        }

                        replyRecieved = true;

                    } else {

                        std::cerr << "Received different block#: " << ntohs(*blockNumber) << ". Expected: " << whereweare << std::endl;
                        packetLen = sendData(sockfd, apt, ackSize, from, dataBuffer);
                        if (packetLen < 0) {

                            std::cerr << "Error sending ACK" << std::endl;

                        }

                    }

                } else if (ntohs(*dOpCode) == 5) {

                    std::cerr << "Recieved Error Pack. Terminating transmission." << std::endl;
                    return -1;

                } else if (ntohs(*dOpCode) < 1 || ntohs(*dOpCode) > 5) { //Illegal OPCODE

                    char* message = "Illegal OpCode";

                    sendError(sockfd, from, 4, message, strlen(message));

                    throw std::runtime_error(message);

                }

            }

        } else if (packetLen < 0) {
            std::cerr << "Error or timeout receiving DATA" << std::endl;
            break; // Handle error or timeout
        }
    }

    std::cout << "File transfer complete!" << std::endl;
    return 1;
}

inline void processFileTransfer(const int& reqtype, int& sockfd, struct sockaddr_storage* to, std::fstream& file, int& whereweare) {

    
    // process initial ack/data packet

    std::cout << "Processing a: "; 
    if (reqtype == TFTP_WRQ) {
        std::cout << "WRQ" << std::endl;
        if (processWriting(file, sockfd, to, whereweare) == -1) {
            std::cerr << "Write process was terminated" << std::endl;
        }
        
        std::cout << "Write Process Complete!" << std::endl;

    } else if (reqtype == TFTP_RRQ) {
        std::cout << "RRQ" << std::endl;
        if (processReading(sockfd, file, to, whereweare) == -1) {
            std::cerr << "Read process was terminated" << std::endl;
        }

        std::cout << "Read Process Complete!" << std::endl;

    }
    
}

inline void extractFilenameFromPacket(const char* packet, int packetLength, std::string& filename) {
    
    const char* filenameStart = packet + 2;

    // Find the length of the filename by searching for the null terminator
    int filenameLength = strlen(filenameStart);
    // Ensure that the filename does not exceed the packet length and is properly terminated
    if (filenameStart + filenameLength < packet + packetLength) {
        // Copy the filename to a std::string for safe usage
        filename.assign(filenameStart, filenameLength);
        std::cout << filename << std::endl;
    } else {
        // Handle error: invalid packet or missing null terminator for filename
        std::cerr << "Error: Invalid WRQ/RRQ packet format." << std::endl;
        filename.clear(); // Clear the filename to indicate an error
    }
}
