#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <errno.h>

#define PORT 5555
#define HELLO_INTERVAL 5
#define NEIGHBOR_TIMEOUT 10
#define MAX_NEIGHBORS 10
#define BROADCAST_ADDR "255.255.255.255"
#define BUFFER_SIZE 512

// Neighbor structure
typedef struct {
    char ip_address[INET_ADDRSTRLEN];
    uint16_t last_sequence;
    time_t last_heard;
    int active;
} Neighbor;

// Global variables
Neighbor neighbor_table[MAX_NEIGHBORS];
pthread_mutex_t neighbor_mutex = PTHREAD_MUTEX_INITIALIZER;
char my_ip_address[INET_ADDRSTRLEN];
uint16_t hello_sequence = 0;
int sock_fd;

// Get the primary IP address of this machine
int get_my_ip_address(char *ip_buffer) {
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return -1;
    }

    // Look for first non-loopback IPv4 address
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;

        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            const char *ip = inet_ntoa(addr->sin_addr);

            // Skip loopback
            if (strcmp(ip, "127.0.0.1") != 0) {
                strncpy(ip_buffer, ip, INET_ADDRSTRLEN);
                freeifaddrs(ifaddr);
                return 0;
            }
        }
    }

    freeifaddrs(ifaddr);
    return -1;
}

// Initialize neighbor table
void init_neighbor_table() {
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        neighbor_table[i].active = 0;
        neighbor_table[i].ip_address[0] = '\0';
        neighbor_table[i].last_sequence = 0;
        neighbor_table[i].last_heard = 0;
    }
}

// Add or update a neighbor
void update_neighbor(const char *ip, uint16_t sequence) {
    pthread_mutex_lock(&neighbor_mutex);

    time_t now = time(NULL);
    int found = 0;

    // Check if neighbor already exists
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbor_table[i].active &&
            strcmp(neighbor_table[i].ip_address, ip) == 0) {

            // Update only if sequence number is newer
            if (sequence > neighbor_table[i].last_sequence) {
                neighbor_table[i].last_sequence = sequence;
                neighbor_table[i].last_heard = now;
                printf("[UPDATE] Neighbor %s updated (seq: %u)\n", ip, sequence);
            }
            found = 1;
            break;
        }
    }

    // Add new neighbor if not found
    if (!found) {
        for (int i = 0; i < MAX_NEIGHBORS; i++) {
            if (!neighbor_table[i].active) {
                strncpy(neighbor_table[i].ip_address, ip, INET_ADDRSTRLEN);
                neighbor_table[i].last_sequence = sequence;
                neighbor_table[i].last_heard = now;
                neighbor_table[i].active = 1;
                printf("[NEW] Neighbor detected: %s (seq: %u)\n", ip, sequence);
                break;
            }
        }
    }

    pthread_mutex_unlock(&neighbor_mutex);
}

// Check for timed-out neighbors
void check_neighbor_timeouts() {
    pthread_mutex_lock(&neighbor_mutex);

    time_t now = time(NULL);

    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbor_table[i].active) {
            if (now - neighbor_table[i].last_heard > NEIGHBOR_TIMEOUT) {
                printf("[TIMEOUT] Neighbor lost: %s\n", neighbor_table[i].ip_address);
                neighbor_table[i].active = 0;
            }
        }
    }

    pthread_mutex_unlock(&neighbor_mutex);
}

// Print current neighbor table
void print_neighbor_table() {
    pthread_mutex_lock(&neighbor_mutex);

    printf("\n=== Neighbor Table ===\n");
    int count = 0;
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbor_table[i].active) {
            printf("  %s (seq: %u, last heard: %ld sec ago)\n",
                   neighbor_table[i].ip_address,
                   neighbor_table[i].last_sequence,
                   time(NULL) - neighbor_table[i].last_heard);
            count++;
        }
    }
    if (count == 0) {
        printf("  (no neighbors)\n");
    }
    printf("======================\n\n");

    pthread_mutex_unlock(&neighbor_mutex);
}

// Thread: Send HELLO messages every 5 seconds
void *hello_sender_thread(void *arg) {
    struct sockaddr_in broadcast_addr;
    char hello_msg[BUFFER_SIZE];

    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(PORT);
    broadcast_addr.sin_addr.s_addr = inet_addr(BROADCAST_ADDR);

    while (1) {
        // Prepare HELLO message: myIPAddress:HELLO:sequenceNumber
        uint16_t seq_network = htons(hello_sequence);
        int msg_len = snprintf(hello_msg, BUFFER_SIZE, "%s:HELLO:", my_ip_address);
        memcpy(hello_msg + msg_len, &seq_network, sizeof(uint16_t));
        msg_len += sizeof(uint16_t);

        // Send broadcast HELLO
        if (sendto(sock_fd, hello_msg, msg_len, 0,
                   (struct sockaddr *)&broadcast_addr,
                   sizeof(broadcast_addr)) < 0) {
            perror("sendto failed");
        } else {
            printf("[SEND] HELLO broadcast (seq: %u)\n", hello_sequence);
        }

        hello_sequence++;

        // Check for neighbor timeouts
        check_neighbor_timeouts();

        // Print neighbor table
        print_neighbor_table();

        // Sleep for 5 seconds
        sleep(HELLO_INTERVAL);
    }

    return NULL;
}

// Thread: Receive and process incoming messages
void *message_receiver_thread(void *arg) {
    char buffer[BUFFER_SIZE];
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);

        int recv_len = recvfrom(sock_fd, buffer, BUFFER_SIZE - 1, 0,
                                (struct sockaddr *)&sender_addr, &addr_len);

        if (recv_len < 0) {
            perror("recvfrom failed");
            continue;
        }

        // Parse message format: IPAddress:HELLO:sequenceNumber
        char sender_ip[INET_ADDRSTRLEN];
        char msg_type[16];
        uint16_t sequence;

        // Extract IP and message type
        char *token = strtok(buffer, ":");
        if (token == NULL) continue;
        strncpy(sender_ip, token, INET_ADDRSTRLEN);

        // Check if message is from ourselves
        if (strcmp(sender_ip, my_ip_address) == 0) {
            continue; // Ignore messages from self
        }

        token = strtok(NULL, ":");
        if (token == NULL) continue;
        strncpy(msg_type, token, sizeof(msg_type));

        // Process HELLO messages
        if (strcmp(msg_type, "HELLO") == 0) {
            // Sequence number is in network byte order
            char *seq_ptr = strtok(NULL, ":");
            if (seq_ptr != NULL && strlen(seq_ptr) >= sizeof(uint16_t)) {
                memcpy(&sequence, seq_ptr, sizeof(uint16_t));
                sequence = ntohs(sequence);

                printf("[RECV] HELLO from %s (seq: %u)\n", sender_ip, sequence);
                update_neighbor(sender_ip, sequence);
            }
        }
    }

    return NULL;
}

// Create and configure UDP socket
int create_broadcast_socket() {
    int sock;
    int broadcast_permission = 1;
    struct sockaddr_in local_addr;

    // Create UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket creation failed");
        return -1;
    }

    // Enable broadcast
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST,
                   (void *)&broadcast_permission,
                   sizeof(broadcast_permission)) < 0) {
        perror("setsockopt(SO_BROADCAST) failed");
        close(sock);
        return -1;
    }

    // Allow address reuse
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                   (void *)&reuse, sizeof(reuse)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(sock);
        return -1;
    }

    // Bind to port
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(PORT);

    if (bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("bind failed");
        close(sock);
        return -1;
    }

    return sock;
}

int main() {
    pthread_t hello_thread, receiver_thread;

    printf("=== Distance Vector Routing Protocol - Part 1 ===\n\n");

    // Get our IP address
    if (get_my_ip_address(my_ip_address) < 0) {
        fprintf(stderr, "Failed to get IP address\n");
        return 1;
    }
    printf("My IP Address: %s\n", my_ip_address);
    printf("Using UDP port: %d\n\n", PORT);

    // Initialize neighbor table
    init_neighbor_table();

    // Create broadcast-enabled UDP socket
    sock_fd = create_broadcast_socket();
    if (sock_fd < 0) {
        fprintf(stderr, "Failed to create socket\n");
        return 1;
    }

    printf("Socket created and configured for broadcast\n");
    printf("Starting neighbor detection...\n\n");

    // Create threads
    if (pthread_create(&hello_thread, NULL, hello_sender_thread, NULL) != 0) {
        perror("Failed to create HELLO sender thread");
        close(sock_fd);
        return 1;
    }

    if (pthread_create(&receiver_thread, NULL, message_receiver_thread, NULL) != 0) {
        perror("Failed to create receiver thread");
        close(sock_fd);
        return 1;
    }

    // Wait for threads (they run indefinitely)
    pthread_join(hello_thread, NULL);
    pthread_join(receiver_thread, NULL);

    close(sock_fd);
    return 0;
}
