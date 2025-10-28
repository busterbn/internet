# Distance Vector Routing Protocol - Part 1

Implementation of neighbor detection for a Distance Vector Routing Protocol using UDP broadcast.

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
- Listen for HELLO messages from other routers
- Display a neighbor table showing detected routers
- Remove neighbors that haven't sent HELLO for >10 seconds

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

- **Neighbor Detection**: Routers announce presence every 5 seconds
- **Timeout**: Neighbors are removed after 10 seconds of inactivity
- **Thread-Safe**: Uses mutex for neighbor table access
- **Non-Blocking**: Separate threads for sending and receiving
- **Self-Detection**: Ignores messages from itself
