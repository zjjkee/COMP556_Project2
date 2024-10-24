# COMP556 Project 2: Reliable File Transfer Protocol via UDP

## Team Members

1. **Aathmika Neelakanta (S01469766)** 
2. **Tzuhan Su(S01469920)**
3. **Jingke Zou(S01505152)**

## Files Included

- `sendfile.c`: The sender program that reads a file and transmits it using a sliding window protocol.
- `recvfile.c`: The receiver program that listens for incoming packets, verifies the data, and writes it to a local file named `output`.
- `Makefile`: A makefile to compile both `sendfile.c` and `recvfile.c`.

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
    - Our packet defined consist of sequence_number(uint32_t), data_length(uint32_t), checksum(uint32_t), is_last_packet(int), filename and directory(char []) and data(which is PACKET_SIZE)
    - Thus we have our PACKET_SIZE: 1500 bytes (MTU)− 11 bytes(IP header) − 8 bytes (UDP header) - 200 bytes(filename& directory) = 1281 bytes (max payload size)

    How we determine WINDOW_SIZE:
    - After we test the ping from the CLEAR server to look.cs.rice.edu, we have the RTT is 0.287ms(need to be test on CLEAR). Thus we set timeout = 3000us;
    - Based on this, we have BDP = RTT × Bandwidth; We set each packet size is 1481 bytes. Thus the WindowSize = (RTT × Bandwidth)/PACKET_SIZE  is 24.


2. `recvfile.c`: A file receiver that receives the file, verifies the data integrity using CRC32, and saves the file in the local directory as `output`. In the recvfile, we implemented following mechanisms:
    - ACK: Sends ACKs for successfully received packets to inform the sender that the data was correctly received.
    - Checksum (CRC32): Verifies the integrity of each packet to detect and discard corrupted data.
    - In-Order&Out-of-Order Packet Processing: Writes correctly ordered packets to the file and slides the window forward.
    - Duplicate Packet Handling: Ignores duplicate packets that have already been processed and acknowledged.
    - File Writing: Writes the received data to a file while maintaining the correct order and structure.

## Design
### Protocol

- UDP Protocol: The User Datagram Protocol (UDP) is used to support unreliable network simulation.
- Sliding Window Protocol: The Sliding Window Protocol is used to ensure reliable packet transmission, maximize network's bandwidth utilization, and avoid network congestion.

### Packet Format
- sequence_number(int): the sequence number of the packet
- is_last_packet(bool): true if the packet is the last packet of the file
- checksum(int): to detect data corruption by the receiver
- data_length(int): the data length of the packet
- filename(char []): the buffer for filename,100B
- directory(char []): the buffer for directory,100B
- data(char[]): the data of the packet, with maximum 1481 bytes (1500 bytes (MTU)− 11 bytes(IP header) − 8 bytes (UDP header - 200 bytes(filename + directory)) = 1281 bytes)

### Algorithms
Sliding Window Protocol（Go-Back-N). Go-Back-N (GBN) is a sliding window protocol used to achieve reliable transmission over unreliable networks, particularly in the context of transport layer protocols like UDP (which itself does not provide reliability). It ensures that packets are delivered in order and without errors, handling packet loss, corruption, and reordering. GBN allows a sender to transmit multiple packets without waiting for an acknowledgment (ACK) for each packet, but with a constraint on the number of unacknowledged packets at any time.
- **Sender Operations**:
  - **Initialization**: The sender starts with an empty send window, which can hold up to 1481 packets (window size).
  - **Sending Packets**: The sender can send packets up to the window size N. Each packet is assigned a sequence number. After sending each packet, the sender starts a timer for the oldest unacknowledged packet .
  - **Acknowledgment Handling**: The sender continues sending packets until it reaches the limit of the window size or runs out of data.If the sender receives an acknowledgment ACK(k), it moves the window forward so that it can send new packets. The window slides to k+1, meaning all packets up to  k are now acknowledged and out of the window.
  - **Timeout and Retransmission**: If the timer for the oldest unacknowledged packet expires, the sender retransmits all packets in the window starting from the oldest one.
The timer is then restarted.
- **Receiver Operations**:
  - **Receiving Packets**: The receiver expects packets in a strict sequential order. If it receives the expected packet (matching the expected sequence number), it processes the packet and sends an acknowledgment for it.
  - **Handling Out-of-Order Packets**: If the receiver receives a packet that is out-of-order (i.e., not the next expected packet), it discards the packet and retransmits an acknowledgment for the last correctly received packet
  - **Sending Acknowledgments**: The receiver sends cumulative ACKs. If the receiver expects packet k and receives it correctly, it sends an acknowledgment ACK(k), confirming that it has successfully received all packets up to and including k.

### Features
- **Minimized Packet Size Through Packet Format Design**:  
In this design, we optimize packet size by only including essential datagram fields. Each packet is limited to a maximum data size of 1481 bytes, so files are often split across multiple packets. To ensure the receiver can identify the last packet and reassemble the file, we use a boolean field called `is_last_packet`. This simplifies the packet structure and reduces overhead.

  Other necessary fields include:
  - `sequence_number`: For ensuring reliable transmission and correct packet ordering.
  - `checksum`: To detect data corruption during transmission.
  - `data_length`: To manage any discrepancies in data size between the last packet and other packets, particularly when the last packet is smaller.

- **Reliable Transmission – Handling Corrupted Packets with Checksum**: On the receiver side, packet integrity is verified using `CRC32`. If a packet is found to be corrupted, it is immediately discarded to ensure reliable data transmission.
- **Reliable Transmission – Handling Lost or Delayed Packets/Acknowledgments with Timeout and Retransmission**: If an acknowledgment (ACK) is not received within a specified timeout period, the sender will retransmit the unacknowledged packets to ensure reliable delivery. This mechanism also handles cases of delayed packets or ACKs.
- **Reliable Transmission – Handling Duplicated and Reordered Packets with Sequence Numbers**: The receiver uses sequence numbers to detect duplicate packets, as packets with the same sequence number are considered duplicates. Additionally, sequence numbers enable the receiver to correctly reassemble packets in the correct order, even if they arrive out of sequence.
- **Maximized Network's Bandwidth Utilization by the Sliding Window Protocol**: Unlike the Stop-and-Wait protocol, which pauses to wait for an acknowledgment (ACK) before sending the next packet, we implemented the sliding window protocol. This allows the sender to transmit up to `WINDOW_SIZE` packets before needing to receive any ACKs, significantly improving bandwidth utilization and overall network efficiency.

- **Network Congestion Prevention by the Sliding Window Protocol**: The sliding window protocol helps prevent network congestion by limiting the number of packets the sender can transmit at one time. The controlled window size ensures that the sender does not overwhelm the network by sending an unlimited number of packets, promoting efficient data flow and avoiding congestion.

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

Check the process and Memory usage  on CLEAR:
```
ps aux | grep recvfile
```

Check the process and Memory usage  on emulator machine:
```
ps aux | grep sendfile
```
