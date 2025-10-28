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
#define MAX_DESTINATIONS 50
#define BROADCAST_ADDR "255.255.255.255"
#define BUFFER_SIZE 512
#define INFINITY_DISTANCE 999

// Neighbor structure
typedef struct {
    char ip_address[INET_ADDRSTRLEN];
    uint16_t last_sequence;
    time_t last_heard;
    int active;
} Neighbor;

// Distance entry: stores distance to a destination via a specific neighbor
typedef struct {
    char neighbor_ip[INET_ADDRSTRLEN];  // Via which neighbor
    int distance;                        // Distance to destination via this neighbor
} DistanceEntry;

// Distance Table entry: for each destination, store distances via all neighbors
typedef struct {
    char destination_ip[INET_ADDRSTRLEN];
    DistanceEntry entries[MAX_NEIGHBORS];
    int num_entries;
    int active;
} DistanceTableEntry;

// Global variables
Neighbor neighbor_table[MAX_NEIGHBORS];
pthread_mutex_t neighbor_mutex = PTHREAD_MUTEX_INITIALIZER;
DistanceTableEntry distance_table[MAX_DESTINATIONS];
pthread_mutex_t distance_mutex = PTHREAD_MUTEX_INITIALIZER;
int updated_dv = 0;  // Flag to indicate if DV needs to be sent
char my_ip_address[INET_ADDRSTRLEN];
uint16_t hello_sequence = 0;
int sock_fd;

// Forward declarations
int update_distance_table(const char *destination, const char *neighbor, int distance);
void remove_neighbor_from_distance_table(const char *neighbor);
void dvUpdate();
void dvSent();
int get_shortest_distance(const char *destination);
void print_neighbor_table();
void print_distance_table();

// Get the primary IP address of this machine
int get_my_ip_address(char *ip_buffer) {
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return -1;
    }

    printf("Available network interfaces:\n");
    // Look for first non-loopback IPv4 address
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;

        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            const char *ip = inet_ntoa(addr->sin_addr);
            printf("  %s: %s\n", ifa->ifa_name, ip);

            // Skip loopback
            if (strcmp(ip, "127.0.0.1") != 0 && ip_buffer[0] == '\0') {
                strncpy(ip_buffer, ip, INET_ADDRSTRLEN);
            }
        }
    }

    freeifaddrs(ifaddr);

    if (ip_buffer[0] == '\0') {
        return -1;
    }
    return 0;
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
    int is_new = 0;

    // Check if neighbor already exists
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbor_table[i].active &&
            strcmp(neighbor_table[i].ip_address, ip) == 0) {

            // Update only if sequence number is newer
            if (sequence > neighbor_table[i].last_sequence) {
                neighbor_table[i].last_sequence = sequence;
                neighbor_table[i].last_heard = now;
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
                is_new = 1;
                printf("[NEW] Neighbor detected: %s (seq: %u)\n", ip, sequence);

                // Add neighbor to distance table with distance 1
                if (update_distance_table(ip, ip, 1)) {
                    dvUpdate();
                }

                break;
            }
        }
    }

    pthread_mutex_unlock(&neighbor_mutex);

    // Print neighbor table only when a new neighbor is added
    if (is_new) {
        print_neighbor_table();
        print_distance_table();
    }
}

// Check for timed-out neighbors
void check_neighbor_timeouts() {
    pthread_mutex_lock(&neighbor_mutex);

    time_t now = time(NULL);
    int any_timeout = 0;

    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbor_table[i].active) {
            if (now - neighbor_table[i].last_heard > NEIGHBOR_TIMEOUT) {
                printf("[TIMEOUT] Neighbor lost: %s\n", neighbor_table[i].ip_address);

                // Remove neighbor from distance table
                remove_neighbor_from_distance_table(neighbor_table[i].ip_address);
                dvUpdate();

                neighbor_table[i].active = 0;
                any_timeout = 1;
            }
        }
    }

    pthread_mutex_unlock(&neighbor_mutex);

    // Print tables only when a neighbor times out
    if (any_timeout) {
        print_neighbor_table();
        print_distance_table();
    }
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

// Initialize distance table
void init_distance_table() {
    for (int i = 0; i < MAX_DESTINATIONS; i++) {
        distance_table[i].active = 0;
        distance_table[i].num_entries = 0;
        distance_table[i].destination_ip[0] = '\0';
    }
}

// Get the shortest distance to a destination
int get_shortest_distance(const char *destination) {
    int shortest = INFINITY_DISTANCE;

    for (int i = 0; i < MAX_DESTINATIONS; i++) {
        if (distance_table[i].active &&
            strcmp(distance_table[i].destination_ip, destination) == 0) {

            // Find minimum distance among all neighbors
            for (int j = 0; j < distance_table[i].num_entries; j++) {
                if (distance_table[i].entries[j].distance < shortest) {
                    shortest = distance_table[i].entries[j].distance;
                }
            }
            break;
        }
    }

    return shortest;
}

// Update distance table with a new distance to a destination via a neighbor
// Returns 1 if the shortest distance changed, 0 otherwise
int update_distance_table(const char *destination, const char *neighbor, int distance) {
    pthread_mutex_lock(&distance_mutex);

    int old_shortest = get_shortest_distance(destination);
    int dest_index = -1;

    // Find or create destination entry
    for (int i = 0; i < MAX_DESTINATIONS; i++) {
        if (distance_table[i].active &&
            strcmp(distance_table[i].destination_ip, destination) == 0) {
            dest_index = i;
            break;
        }
    }

    // Create new destination entry if not found
    if (dest_index == -1) {
        for (int i = 0; i < MAX_DESTINATIONS; i++) {
            if (!distance_table[i].active) {
                dest_index = i;
                distance_table[i].active = 1;
                strncpy(distance_table[i].destination_ip, destination, INET_ADDRSTRLEN);
                distance_table[i].num_entries = 0;
                break;
            }
        }
    }

    if (dest_index == -1) {
        pthread_mutex_unlock(&distance_mutex);
        printf("[WARN] Distance table full\n");
        return 0;
    }

    // Find or create entry for this neighbor
    int entry_index = -1;
    for (int i = 0; i < distance_table[dest_index].num_entries; i++) {
        if (strcmp(distance_table[dest_index].entries[i].neighbor_ip, neighbor) == 0) {
            entry_index = i;
            break;
        }
    }

    if (entry_index == -1 && distance_table[dest_index].num_entries < MAX_NEIGHBORS) {
        // Add new entry
        entry_index = distance_table[dest_index].num_entries;
        strncpy(distance_table[dest_index].entries[entry_index].neighbor_ip,
                neighbor, INET_ADDRSTRLEN);
        distance_table[dest_index].num_entries++;
    }

    if (entry_index != -1) {
        distance_table[dest_index].entries[entry_index].distance = distance;
    }

    int new_shortest = get_shortest_distance(destination);
    int changed = (old_shortest != new_shortest);

    pthread_mutex_unlock(&distance_mutex);
    return changed;
}

// Remove all entries for a specific neighbor (when link breaks)
void remove_neighbor_from_distance_table(const char *neighbor) {
    pthread_mutex_lock(&distance_mutex);

    for (int i = 0; i < MAX_DESTINATIONS; i++) {
        if (!distance_table[i].active) continue;

        // Remove entries for this neighbor
        for (int j = 0; j < distance_table[i].num_entries; j++) {
            if (strcmp(distance_table[i].entries[j].neighbor_ip, neighbor) == 0) {
                // Shift remaining entries
                for (int k = j; k < distance_table[i].num_entries - 1; k++) {
                    distance_table[i].entries[k] = distance_table[i].entries[k + 1];
                }
                distance_table[i].num_entries--;
                j--;  // Check this position again
            }
        }

        // If no entries left, mark destination as inactive
        if (distance_table[i].num_entries == 0) {
            distance_table[i].active = 0;
        }
    }

    pthread_mutex_unlock(&distance_mutex);
}

// Get distance vector as a string
// Format: senderIPAddress:DV:(dest1,dist1):(dest2,dist2):...:(destN,distN):
char* getDistanceVector() {
    static char dv_string[BUFFER_SIZE];
    pthread_mutex_lock(&distance_mutex);

    int offset = snprintf(dv_string, BUFFER_SIZE, "%s:DV:", my_ip_address);

    // For each destination, add the shortest distance
    for (int i = 0; i < MAX_DESTINATIONS; i++) {
        if (!distance_table[i].active) continue;

        int shortest = get_shortest_distance(distance_table[i].destination_ip);
        if (shortest < INFINITY_DISTANCE) {
            offset += snprintf(dv_string + offset, BUFFER_SIZE - offset,
                             "(%s,%d):", distance_table[i].destination_ip, shortest);
        }
    }

    pthread_mutex_unlock(&distance_mutex);
    return dv_string;
}

// Process a distance vector received from a neighbor
// Format: senderIPAddress:DV:(dest1,dist1):(dest2,dist2):...:(destN,distN):
void processDistanceVector(char* dv) {
    char sender_ip[INET_ADDRSTRLEN];

    // Parse sender IP
    char *first_colon = strchr(dv, ':');
    if (first_colon == NULL) return;

    int ip_len = first_colon - dv;
    strncpy(sender_ip, dv, ip_len);
    sender_ip[ip_len] = '\0';

    // Verify it's a DV message
    if (strncmp(first_colon + 1, "DV:", 3) != 0) {
        return;
    }

    printf("[PROCESS] Distance Vector from %s\n", sender_ip);

    // Parse entries: (dest,dist):
    char *ptr = first_colon + 4;  // Skip ":DV:"
    int changes = 0;

    while (*ptr != '\0') {
        if (*ptr == '(') {
            char dest[INET_ADDRSTRLEN];
            int dist;

            // Parse destination IP
            ptr++;
            char *comma = strchr(ptr, ',');
            if (comma == NULL) break;

            int dest_len = comma - ptr;
            if (dest_len >= INET_ADDRSTRLEN) break;
            strncpy(dest, ptr, dest_len);
            dest[dest_len] = '\0';

            // Parse distance
            ptr = comma + 1;
            dist = atoi(ptr);

            // Skip to end of this entry
            while (*ptr != ')' && *ptr != '\0') ptr++;
            if (*ptr == ')') ptr++;
            if (*ptr == ':') ptr++;

            // Update distance table: distance via sender is sender's distance + 1
            int new_distance = dist + 1;
            if (update_distance_table(dest, sender_ip, new_distance)) {
                printf("  Updated: %s via %s, distance %d\n", dest, sender_ip, new_distance);
                changes = 1;
            }
        } else {
            ptr++;
        }
    }

    if (changes) {
        dvUpdate();
        print_distance_table();
    }
}

// Called when the distance vector is updated
void dvUpdate() {
    pthread_mutex_lock(&distance_mutex);
    updated_dv = 1;
    printf("[DV_UPDATE] Distance vector has changed, will send update\n");
    pthread_mutex_unlock(&distance_mutex);
}

// Called when distance vector is sent
void dvSent() {
    pthread_mutex_lock(&distance_mutex);
    updated_dv = 0;
    pthread_mutex_unlock(&distance_mutex);
}

// Check if DV needs to be sent
int dvNeedsSending() {
    pthread_mutex_lock(&distance_mutex);
    int needs_sending = updated_dv;
    pthread_mutex_unlock(&distance_mutex);
    return needs_sending;
}

// Print distance table
void print_distance_table() {
    pthread_mutex_lock(&distance_mutex);

    printf("\n=== Distance Table ===\n");
    int count = 0;
    for (int i = 0; i < MAX_DESTINATIONS; i++) {
        if (!distance_table[i].active) continue;

        printf("  Destination: %s\n", distance_table[i].destination_ip);
        for (int j = 0; j < distance_table[i].num_entries; j++) {
            printf("    via %s: distance %d\n",
                   distance_table[i].entries[j].neighbor_ip,
                   distance_table[i].entries[j].distance);
        }
        int shortest = get_shortest_distance(distance_table[i].destination_ip);
        printf("    -> Best: %d\n", shortest);
        count++;
    }
    if (count == 0) {
        printf("  (no destinations)\n");
    }
    printf("======================\n\n");

    pthread_mutex_unlock(&distance_mutex);
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
        int bytes_sent = sendto(sock_fd, hello_msg, msg_len, 0,
                                (struct sockaddr *)&broadcast_addr,
                                sizeof(broadcast_addr));
        if (bytes_sent < 0) {
            perror("sendto failed");
        }
        // Uncomment for verbose debugging:
        // else {
        //     printf("[SEND] HELLO broadcast to %s:%d (seq: %u, %d bytes)\n",
        //            BROADCAST_ADDR, PORT, hello_sequence, bytes_sent);
        // }

        hello_sequence++;

        // Check for neighbor timeouts
        check_neighbor_timeouts();

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

    printf("[INFO] Receiver thread started, listening on port %d\n", PORT);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);

        int recv_len = recvfrom(sock_fd, buffer, BUFFER_SIZE - 1, 0,
                                (struct sockaddr *)&sender_addr, &addr_len);

        if (recv_len < 0) {
            perror("recvfrom failed");
            continue;
        }

        // char *sender_ip_from_packet = inet_ntoa(sender_addr.sin_addr);
        // printf("[RECV] Got %d bytes from %s:%d\n",
        //        recv_len, sender_ip_from_packet, ntohs(sender_addr.sin_port));

        // Parse message format: IPAddress:HELLO:sequenceNumber
        // Note: sequenceNumber is binary (2 bytes), not a string!
        char sender_ip[INET_ADDRSTRLEN];
        char msg_type[16];
        uint16_t sequence;

        // Find the colons to parse the message
        char *first_colon = strchr(buffer, ':');
        if (first_colon == NULL) {
            printf("[WARN] Invalid message format - no first colon\n");
            continue;
        }

        // Extract sender IP
        int ip_len = first_colon - buffer;
        if (ip_len >= INET_ADDRSTRLEN) {
            printf("[WARN] IP address too long\n");
            continue;
        }
        strncpy(sender_ip, buffer, ip_len);
        sender_ip[ip_len] = '\0';

        // Check if message is from ourselves
        if (strcmp(sender_ip, my_ip_address) == 0) {
            // printf("[DEBUG] Ignoring message from self (%s)\n", sender_ip);
            continue;
        }

        // Find second colon
        char *second_colon = strchr(first_colon + 1, ':');
        if (second_colon == NULL) {
            printf("[WARN] Invalid message format - no second colon\n");
            continue;
        }

        // Extract message type
        int type_len = second_colon - (first_colon + 1);
        if (type_len >= sizeof(msg_type)) {
            printf("[WARN] Message type too long\n");
            continue;
        }
        strncpy(msg_type, first_colon + 1, type_len);
        msg_type[type_len] = '\0';

        // Process HELLO messages
        if (strcmp(msg_type, "HELLO") == 0) {
            // Sequence number starts right after second colon
            // It's binary data (2 bytes in network byte order)
            char *seq_ptr = second_colon + 1;
            int remaining = recv_len - (seq_ptr - buffer);

            if (remaining >= sizeof(uint16_t)) {
                memcpy(&sequence, seq_ptr, sizeof(uint16_t));
                sequence = ntohs(sequence);

                // Uncomment for verbose debugging:
                // printf("[RECV] HELLO from %s (seq: %u)\n", sender_ip, sequence);
                update_neighbor(sender_ip, sequence);
            } else {
                printf("[WARN] Invalid HELLO message - insufficient data for sequence number (got %d bytes, need %zu)\n",
                       remaining, sizeof(uint16_t));
            }
        } else if (strcmp(msg_type, "DV") == 0) {
            // Process distance vector message
            printf("[RECV] DV message from %s\n", sender_ip);
            processDistanceVector(buffer);
        } else {
            printf("[WARN] Unknown message type: %s\n", msg_type);
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

    // Allow port reuse (needed for multiple instances on same machine)
#ifdef SO_REUSEPORT
    int reuse_port = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT,
                   (void *)&reuse_port, sizeof(reuse_port)) < 0) {
        perror("setsockopt(SO_REUSEPORT) failed");
        close(sock);
        return -1;
    }
#endif

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

    printf("=== Distance Vector Routing Protocol - Part 2 ===\n\n");

    // Initialize my_ip_address buffer
    memset(my_ip_address, 0, INET_ADDRSTRLEN);

    // Get our IP address
    if (get_my_ip_address(my_ip_address) < 0) {
        fprintf(stderr, "Failed to get IP address\n");
        return 1;
    }
    printf("\nSelected IP Address: %s\n", my_ip_address);
    printf("Using UDP port: %d\n", PORT);
    printf("Broadcasting to: %s\n\n", BROADCAST_ADDR);

    // Initialize neighbor table
    init_neighbor_table();

    // Initialize distance table
    init_distance_table();

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
