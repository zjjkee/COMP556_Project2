#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <getopt.h>

#define PACKET_SIZE 1476
#define HEADER_SIZE 16  // 4 bytes for sequence number + 4 bytes for checksum + 4 bytes for data length + 4 bytes for  is_last_packet
#define WINDOW_SIZE 100
#define TIMEOUT 1  // Timeout of 1 second

// Structure representing a packet
struct Packet {
    uint32_t sequence_number;
    uint32_t data_length;
    char data[PACKET_SIZE]; // payload, size defined by PACKET_SIZE
    uint32_t checksum;
    int is_last_packet;  // Flag indicating if this is the last packet
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

// Function to print information when sending a packet
void print_send_message(uint32_t start, int length) {
    printf("[send data] %u (%d)\n", start, length);
}

// Function to parse command line arguments
void parse_arguments(int argc, char *argv[], char **host, int *port, char **dir, char **file) {
    int opt;
    while ((opt = getopt(argc, argv, "r:f:")) != -1) {
        switch (opt) {
            case 'r':
                *host = strtok(optarg, ":");
                *port = atoi(strtok(NULL, ":"));
                break;
            case 'f':
                *dir = strtok(optarg, "/");
                *file = strtok(NULL, "/");
                break;
            default:
                fprintf(stderr, "Usage: %s -r <recv host>:<recv port> -f <subdir>/<filename>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    if (*host == NULL || *port == 0 || *dir == NULL || *file == NULL) {
        fprintf(stderr, "Missing required arguments.\n");
        exit(EXIT_FAILURE);
    }
}

// Function to create a UDP socket
int create_socket(struct sockaddr_in *address, const char *ip, int port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return -1;
    }
    address->sin_family = AF_INET;
    address->sin_port = htons(port);
    inet_pton(AF_INET, ip, &(address->sin_addr));
    return sock;
}

// Function to send a file using the sliding window protocol
void send_file(int sock, struct sockaddr_in *receiver_addr, const char *file_path) {
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        perror("File open failed");
        return;
    }

    struct Packet packets[WINDOW_SIZE];
    uint32_t base = 0, next_seq_num = 0;
    // int acked[WINDOW_SIZE] = {0};  // Marks ACK'd packets
    socklen_t addr_len = sizeof(*receiver_addr);
    fd_set fds;
    struct timeval tv;
    tv.tv_sec = TIMEOUT;
    tv.tv_usec = 0;

    while (1) {
        // Continue sending packets as long as there is space in the window
        while (next_seq_num < base + WINDOW_SIZE) {
            size_t read_bytes = fread(packets[next_seq_num % WINDOW_SIZE].data, 1, PACKET_SIZE, file);
            // printf("read_bytes: %zu\n", read_bytes);

            // Check if we have reached the end of the file
            if (read_bytes < PACKET_SIZE) {
                // If fewer than PACKET_SIZE bytes are read, this is the last packet
                packets[next_seq_num % WINDOW_SIZE].is_last_packet = 1;  // 1 indicates the last packet
            } else {
                packets[next_seq_num % WINDOW_SIZE].is_last_packet = 0;  // 0 indicates a regular packet
            }

            if (read_bytes <= 0) break;  // File reading complete
            printf("read_bytes: %zu\n", read_bytes);
            packets[next_seq_num % WINDOW_SIZE].sequence_number = next_seq_num;
            packets[next_seq_num % WINDOW_SIZE].data_length = read_bytes;
            packets[next_seq_num % WINDOW_SIZE].checksum = crc32(packets[next_seq_num % WINDOW_SIZE].data, read_bytes);

            // Send the packet to the receiver
            sendto(sock, &packets[next_seq_num % WINDOW_SIZE], sizeof(struct Packet), 0, (struct sockaddr *)receiver_addr, addr_len);
            print_send_message(next_seq_num * PACKET_SIZE, read_bytes);
            next_seq_num++;
            printf("next_seq_num: %d; base: %d; WINDOW_SIZE: %d \n", next_seq_num, base, WINDOW_SIZE);
        }
        // printf("Waiting for ACK...\n");

        // Wait for ACK
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        int activity = select(sock + 1, &fds, NULL, NULL, &tv);

        if (activity > 0) {
            uint32_t ack;
            int bytes_rcvd = recvfrom(sock, &ack, sizeof(ack), 0, (struct sockaddr *)receiver_addr, &addr_len);
            if (bytes_rcvd > 0 && ack >= base) {
                // Slide the window
                base = ack + 1;
                // printf("Sliding window, base: %d\n", base);
            }
        } else {
            // Retransmit on timeout
            for (uint32_t i = base; i < next_seq_num; i++) {
                printf("Retransmitting on timeout\n");
                sendto(sock, &packets[i % WINDOW_SIZE], sizeof(struct Packet), 0, (struct sockaddr *)receiver_addr, addr_len);
                print_send_message(i * PACKET_SIZE, PACKET_SIZE);
            }
        }

        // Exit after sending the file completely
        if (feof(file) && base >= next_seq_num) {
            printf("Exit after completing transmission\n");
            break;
        }
    }

    printf("[completed]\n");
    fclose(file);
}

int main(int argc, char *argv[]) {
    char *host = NULL, *dir = NULL, *file = NULL;
    int port;

    // Parse command line arguments to get the receiver host, port, subdirectory, and filename
    parse_arguments(argc, argv, &host, &port, &dir, &file);

    struct sockaddr_in receiver_addr;
    // Create a socket for sending data
    int sock = create_socket(&receiver_addr, host, port);
    if (sock < 0) return 1;

    // Construct the file path to be sent
    char file_path[150];
    snprintf(file_path, sizeof(file_path), "%s/%s", dir, file);

    // Start sending the file
    send_file(sock, &receiver_addr, file_path);

    // Close the socket after transmission is complete
    close(sock);
    return 0;
}
