#!/bin/bash
# Send a fake DV message to test distance vector processing
# Usage: ./test_dv.sh <fake_ip_address>

FAKE_IP=${1:-"10.0.5.99"}

# Create a DV message with some fake destinations
# Format: IP:DV:(dest1,dist1):(dest2,dist2):
DV_MESSAGE="${FAKE_IP}:DV:(10.0.5.100,2):(10.0.5.200,3):(192.168.1.50,4):"

echo "$DV_MESSAGE" | nc -u -b 255.255.255.255 5555

echo "Sent DV from fake IP: $FAKE_IP"
echo "  - 10.0.5.100 at distance 2"
echo "  - 10.0.5.200 at distance 3"
echo "  - 192.168.1.50 at distance 4"
