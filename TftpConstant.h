/*
 * TODO: Add constant variables in this file
 */
static const unsigned int MAX_PACKET_LEN = 516; // data 512 + opcode 2 + block num 2
static const int MAX_RETRY_COUNT = 10;
static const char * SERVER_FOLDER = "server-files/"; // DO NOT CHANGE
static const char * CLIENT_FOLDER = "client-files/"; // DO NOT CHANGE

static const size_t MAX_DATA_SIZE = 512; // Maximum payload size for data packets

static const int RETRY_SECONDS = 1;