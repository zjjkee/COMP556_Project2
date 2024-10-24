#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include <errno.h>

#define PACKET_SIZE 1281
#define WINDOW_SIZE 24
#define FILENAME_SIZE 100
#define DIRECTORY_SIZE 100

// Structure representing a packet
struct Packet {
    uint16_t sequence_number;
    uint32_t data_length;
    char data[PACKET_SIZE];
    uint32_t checksum;
    bool is_last_packet;  // Flag to indicate if this is the last packet
    char filename[FILENAME_SIZE];  // Filename being sent
    char directory[DIRECTORY_SIZE]; // Directory path being sent
};

// Function to calculate CRC32 checksum
uint32_t crc32(const char *data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (unsigned char)data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

// Function to print information when receiving a packet
void print_recv_message(uint32_t start, int length, const char *status) {
    printf("[recv data] %u (%d) %s\n", start, length, status);
}


// Function to parse command line arguments
void parse_arguments(int argc, char *argv[], int *port) {
    int opt;
    while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {
            case 'p':
                *port = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s -p <recv port>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
}

// Function to create a UDP socket and bind it to a specific port
int create_socket(struct sockaddr_in *address, int port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return -1;
    }

    address->sin_family = AF_INET;
    address->sin_port = htons(port);
    address->sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)address, sizeof(*address)) < 0) {
        perror("Socket bind failed");
        return -1;
    }

    return sock;
}

// Function to receive the file using the sliding window protocol
void receive_file(int sock) {
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    struct Packet packet;
    FILE *file = NULL;
    uint32_t base = 0, next_ack = 0;
    char buffer[WINDOW_SIZE][PACKET_SIZE];  // Buffer to store packets within the sliding window
    int acked[WINDOW_SIZE] = {0};  // Flags to track received packets in the window

    // open output file and write into
    // file = fopen("output", "wb");  
    // if (!file) {
    //     perror("File open failed"); //open output failed
    //     return;
    // }

    while (1) {
        ssize_t bytes_received = recvfrom(sock, &packet, sizeof(packet), 0, (struct sockaddr *)&sender_addr, &addr_len);
        if (bytes_received <= 0) break;

        // Validate the checksum
        uint32_t calculated_checksum = crc32(packet.data, packet.data_length);
        if (calculated_checksum != ntohl(packet.checksum)) {
            
            printf("[recv corrupt packet]\n");
            continue;
        }

        char recv_file_path[FILENAME_SIZE + DIRECTORY_SIZE + 10];
        if (file == NULL) {
            // Create the directory if it doesn't exist
            if (mkdir(packet.directory, 0777) && errno != EEXIST) {
                perror("mkdir failed");
                return;
            }

            // Create the full file path with .recv suffix
            snprintf(recv_file_path, sizeof(recv_file_path), "%s/%s.recv", packet.directory, packet.filename);

            // Open the file for writing
            file = fopen(recv_file_path, "wb");
            if (!file) {
                perror("File open failed");
                return;
            }
        }

        // Check if the packet's sequence number is within the current window range
        if (packet.sequence_number >= base && packet.sequence_number < base + WINDOW_SIZE) {
            // Store the received data in the sliding window buffer
            memcpy(buffer[packet.sequence_number % WINDOW_SIZE], packet.data, packet.data_length);
            acked[packet.sequence_number % WINDOW_SIZE] = 1;  // Mark the packet as received

            // Check if this is an out-of-order packet
            if (packet.sequence_number > base) {   
                // Out-of-order arrival, cache the packet but do not process yet
                print_recv_message(packet.sequence_number * PACKET_SIZE, packet.data_length, "ACCEPTED(out-of-order)");
            } else if (packet.sequence_number == base) {
                // Correct, in-order packet; slide the window and process packets in sequence
                while (acked[base % WINDOW_SIZE]) {

                    fwrite(buffer[base % WINDOW_SIZE], 1, packet.data_length, file);  // Write data to file
                    print_recv_message(base * PACKET_SIZE, packet.data_length, "ACCEPTED(in-order)");
                    acked[base % WINDOW_SIZE] = 0;  // Reset the flag
                    base++;  // Slide the window
                }
            }
        } else {
            // If the packet is outside the window range, ignore it
            print_recv_message(packet.sequence_number * PACKET_SIZE, packet.data_length, "IGNORED");
        }

        // Send ACK for the next expected packet (base)
        next_ack = base - 1;
        
        
        // printf("base:%d, sent ACK:%d,  \n",base, next_ack);
        sendto(sock, &next_ack, sizeof(next_ack), 0, (struct sockaddr *)&sender_addr, addr_len);
        if (packet.sequence_number <= next_ack){
            sendto(sock, &next_ack, sizeof(next_ack), 0, (struct sockaddr *)&sender_addr, addr_len);
        }

        // Exit if this is the last packet in the file
        if ((packet.is_last_packet && packet.sequence_number == next_ack)|| (packet.data_length == 0 && packet.is_last_packet) ) {  // Use is_last_packet flag to detect the end of file transmission

            printf("[completed]\n");
            break;
        }
    }

    // Close the file if it's open
    if (file) fclose(file);
}

int main(int argc, char *argv[]) {
    int port;

    // Parse command line arguments to get the receiving port
    parse_arguments(argc, argv, &port);

    struct sockaddr_in receiver_addr;
    // Create and bind the socket to the provided port
    int sock = create_socket(&receiver_addr, port);
    if (sock < 0) return 1;

    // Start receiving the file
    receive_file(sock);

    // Close the socket after file reception is complete
    close(sock);
    return 0;
}
