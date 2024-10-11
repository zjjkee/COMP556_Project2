# COMP556 Project 2: Reliable File Transfer Protocol via UDP

## Team Members

1. **Aathmika Neelakanta (S01469766)** 
2. **Tzuhan Su(S01469920)**
3. **Jingke Zou(S01505152)**

## Project Overview

This project implements a sliding window transport protocol for reliable file transfer over an unreliable network. The protocol is designed to handle potential network issues such as packet delay, loss, reordering, duplication, and corruption. The project includes two components:

1. `sendfile.c`: A file sender that transmits a file over a UDP connection using a sliding window protocol. In the sendfile, we implemented following mechanisms:
    - Timeout and Retransmission: Implements retransmission of lost or delayed packets.
    - ACK Handling: Receives and processes ACKs, and advances the window based on the ACKs received.
    - CRC32 Checksum: Ensures packet integrity by detecting and handling corrupted packets.
    - Last Packet Identification: Marks the last packet to signal the end of the file transmission.
    - Dynamic Window Control: Adjusts the transmission window size dynamically based on ACKs.

    How we determine PACKET_SIZE:
    - For Ethernet networks, the typical MTU is 1500 bytes, which is the standard for many networks.
    - Our packet defined consist of sequence_number(uint32_t), data_length(uint32_t), checksum(uint32_t), is_last_packet(int) and data(which is PACKET_SIZE)
    - Thus we have our PACKET_SIZE: 1500 bytes (MTU)−16 bytes(IP header) − 8 bytes (UDP header) = 1476 bytes (max payload size)

    How we determine WINDOW_SIZE:
    - After we test the ping from the CLEAR server to look.cs.rice.edu, we have the RTT is XXX(need to be test on CLEAR)
    - Based on this, we have BDP = RTT × Bandwidth; We set each packet size is 1024 bytes. Thus the WindowSize we set is XXX.


2. `recvfile.c`: A file receiver that receives the file, verifies the data integrity using CRC32, and saves the file in the local directory as `output`. In the recvfile, we implemented following mechanisms:
    - ACK: Sends ACKs for successfully received packets to inform the sender that the data was correctly received.
    - Checksum (CRC32): Verifies the integrity of each packet to detect and discard corrupted data.
    - In-Order&Out-of-Order Packet Processing: Writes correctly ordered packets to the file and slides the window forward.
    - Duplicate Packet Handling: Ignores duplicate packets that have already been processed and acknowledged.
    - File Writing: Writes the received data to a file while maintaining the correct order and structure.

## Features

- **Packet Handling**: Each packet has a sequence number, data payload, checksum (CRC32), and a flag indicating whether it is the last packet.
- **Checksum Verification**: On the receiver side, each packet's integrity is checked using CRC32. Corrupted packets are discarded.
- **Timeout and Retransmission**: If an acknowledgment (ACK) is not received within a specified timeout, the sender will retransmit the unacknowledged packets.
- **Sliding Window Protocol**: Allows for multiple packets to be sent before waiting for ACKs, increasing the efficiency of the transmission.

## Files Included

- `sendfile.c`: The sender program that reads a file and transmits it using a sliding window protocol.
- `recvfile.c`: The receiver program that listens for incoming packets, verifies the data, and writes it to a local file named `output`.
- `Makefile`: A makefile to compile both `sendfile.c` and `recvfile.c`.

## Usage

To compile the project, use the provided `Makefile`. Run the following command in the terminal:

```bash
make
```

To run the Receiver first, run the following command in the terminal:
```bash
recvfile -p <recv port>
```

Then run sendfile
```bash
sendfile -r <recv host>:<recv port> -f <subdir>/<filename>
```

Clean Build Files:
```bash
make clean
```

Check the process and Memory usage percentage on CLEAR:
```
ps aux | grep recvfile
```

Check the process and Memory usage percentage on emulator machine:
```
ps aux | grep sendfile
```
