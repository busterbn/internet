# Distance Vector Routing Protocol - Parts 1, 2 & 3

Complete implementation of a Distance Vector Routing Protocol using UDP broadcast:
- **Part 1**: Neighbor Detection
- **Part 2**: Distance Table Data Structure & Updates
- **Part 3**: Integration - Automatic DV transmission and convergence

## Build

```bash
make
```

To clean and rebuild:
```bash
make clean && make
```

## Run

```bash
./main
```

The program will:
- Automatically detect your IP address
- Send HELLO messages every 5 seconds via UDP broadcast
- Listen for HELLO and DV messages from other routers
- Maintain a neighbor table showing detected routers
- Maintain a distance table with routes to all known destinations
- Automatically broadcast distance vectors when routing information changes
- Process distance vectors and update routing information using Bellman-Ford
- Remove neighbors that haven't sent HELLO for >10 seconds
- Converge to optimal routes through distributed algorithm

## Requirements

- **Same Network**: All routers must be on the same local network/subnet
- **Port**: Uses UDP port 5555
- **Firewall**: Ensure UDP port 5555 is allowed

### Firewall Configuration

**macOS:**
```bash
# Allow the program through firewall
sudo /usr/libexec/ApplicationFirewall/socketfilterfw --add ./main
```

**Linux/Ubuntu:**
```bash
# Allow UDP port 5555
sudo ufw allow 5555/udp
```

## Implementation Details

### Part 1: Neighbor Detection
- **HELLO Messages**: Routers announce presence every 5 seconds
- **Timeout**: Neighbors are removed after 10 seconds of inactivity
- **Sequence Numbers**: Monotonically incrementing, detects fresh messages
- **Self-Detection**: Messages from self are ignored

### Part 2: Distance Table & Updates
- **Data Structure**: Maintains distance to each destination via each neighbor
- **Bellman-Ford**: distance = neighbor_distance + 1
- **Change Detection**: Tracks when shortest path changes
- **Integration**: Auto-updates when neighbors appear/disappear

### Part 3: Integration & Convergence
- **Automatic DV Broadcast**: Sends DV when routing information changes
- **Event-Driven**: DV sent on neighbor detection, timeout, or DV receipt
- **Distributed Convergence**: Network converges to optimal routes
- **Triggered Updates**: Immediate broadcast when DV changes

### General
- **Thread-Safe**: Mutexes for neighbor and distance tables
- **Non-Blocking**: 4 threads (HELLO sender, receiver, DV sender, main)
- **Interoperable**: Follows assignment message format specifications
