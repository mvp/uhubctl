#!/bin/bash
#
# Examples of using $UHUBCTL JSON output with jq
# 
# This script demonstrates various ways to parse and use the JSON output
# from $UHUBCTL to find devices and generate control commands.
#
# Requirements: uhubctl with -j support and jq installed
#

# Use local uhubctl if available, otherwise use system version
UHUBCTL="uhubctl"
if [ -x "./uhubctl" ]; then
    UHUBCTL="./uhubctl"
elif [ -x "../uhubctl" ]; then
    UHUBCTL="../uhubctl"
fi

echo "=== Example 1: Find all FTDI devices and their control paths ==="
echo "# This finds all FTDI devices and shows how to control them"
$UHUBCTL -j | jq -r '.hubs[] | . as $hub | .ports[] | select(.vendor == "FTDI") | "Device: \(.description)\nControl with: $UHUBCTL -l \($hub.location) -p \(.port) -a off\n"'

echo -e "\n=== Example 2: Find a device by serial number ==="
echo "# This is useful when you have multiple identical devices"
SERIAL="A10KZP45"  # Change this to your device's serial
$UHUBCTL -j | jq -r --arg serial "$SERIAL" '.hubs[] | . as $hub | .ports[] | select(.serial == $serial) | "Found device with serial \($serial):\n  Location: \($hub.location)\n  Port: \(.port)\n  Control: $UHUBCTL -l \($hub.location) -p \(.port) -a cycle"'

echo -e "\n=== Example 3: List all mass storage devices with their control commands ==="
$UHUBCTL -j | jq -r '.hubs[] | . as $hub | .ports[] | select(.is_mass_storage == true) | "Mass Storage: \(.description)\n  Power off: $UHUBCTL -l \($hub.location) -p \(.port) -a off\n  Power on:  $UHUBCTL -l \($hub.location) -p \(.port) -a on\n"'

echo -e "\n=== Example 4: Find all USB3 devices (5Gbps or faster) ==="
$UHUBCTL -j | jq -r '.hubs[] | . as $hub | .ports[] | select(.speed_bps >= 5000000000) | "\(.description) at \($hub.location):\(.port) - Speed: \(.speed)"'

echo -e "\n=== Example 5: Create a device map for documentation ==="
echo "# This creates a sorted list of all connected devices"
$UHUBCTL -j | jq -r '.hubs[] | . as $hub | .ports[] | select(.vid) | "[\($hub.location):\(.port)] \(.vendor // "Unknown") \(.product // .description) (Serial: \(.serial // "N/A"))"' | sort

echo -e "\n=== Example 6: Find empty ports where you can plug devices ==="
$UHUBCTL -j | jq -r '.hubs[] | . as $hub | .ports[] | select(.vid == null) | "Empty port available at hub \($hub.location) port \(.port)"'

echo -e "\n=== Example 7: Generate power control script for specific device type ==="
echo "# This generates a script to control all FTDI devices"
echo "#!/bin/bash"
echo "# Script to control all FTDI devices"
echo "# Usage: $0 [on|off|cycle]"
$UHUBCTL -j | jq -r '.hubs[] | . as $hub | .ports[] | select(.vendor == "FTDI") | "# \(.description)\n$UHUBCTL -l \($hub.location) -p \(.port) -a $1"'

echo -e "\n=== Example 8: Monitor device changes ==="
echo "# Save current state and compare later to see what changed"
echo "# Save current state:"
echo "$UHUBCTL -j > devices_before.json"
echo "# Later, check what changed:"
echo "$UHUBCTL -j > devices_after.json"
echo "diff <(jq -S . devices_before.json) <(jq -S . devices_after.json)"

echo -e "\n=== Example 9: Find devices by class ==="
echo "# Find all Hub devices"
$UHUBCTL -j | jq -r '.hubs[] | . as $hub | .ports[] | select(.class_name == "Hub") | "Hub: \(.description) at \($hub.location):\(.port)"'

echo -e "\n=== Example 10: Export to CSV ==="
echo "# Export device list to CSV format"
echo "Location,Port,VID,PID,Vendor,Product,Serial,Speed"
$UHUBCTL -j | jq -r '.hubs[] | . as $hub | .ports[] | select(.vid) | "\($hub.location),\(.port),\(.vid),\(.pid),\(.vendor // ""),\(.product // ""),\(.serial // ""),\(.speed)"'

echo -e "\n=== Example 11: Find devices on specific hub ==="
echo "# Find all devices on hub 3-1"
LOCATION="3-1"
$UHUBCTL -j | jq -r --arg loc "$LOCATION" '.hubs[] | select(.location == $loc) | .ports[] | select(.vid) | "Port \(.port): \(.description)"'

echo -e "\n=== Example 12: Power cycle all devices of a specific type ==="
echo "# This creates a one-liner to power cycle all USB mass storage devices"
echo -n "$UHUBCTL"
$UHUBCTL -j | jq -r '.hubs[] | . as $hub | .ports[] | select(.is_mass_storage == true) | " -l \($hub.location) -p \(.port)"' | tr '\n' ' '
echo " -a cycle"