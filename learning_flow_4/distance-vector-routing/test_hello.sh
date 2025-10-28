#!/bin/bash
# Send a fake HELLO message to test neighbor detection
# Usage: ./test_hello.sh <fake_ip_address>

FAKE_IP=${1:-"10.0.5.99"}

# Create HELLO message: IP:HELLO: + 2-byte sequence number (binary)
# Using printf to create binary sequence number (value 42 = 0x002a in network byte order)
printf "%s:HELLO:\x00\x2a" "$FAKE_IP" | nc -u -b 255.255.255.255 5555

echo "Sent HELLO from fake IP: $FAKE_IP (seq: 42)"
