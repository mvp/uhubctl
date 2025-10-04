# uhubctl JSON Output Examples

This directory contains examples of how to use uhubctl's JSON output feature (`-j` flag) to programmatically work with USB hub information.

## Requirements

- uhubctl compiled with JSON support
- jq (command-line JSON processor) - install with:
  - macOS: `brew install jq`
  - Ubuntu/Debian: `sudo apt-get install jq`
  - RedHat/Fedora: `sudo yum install jq`

## Running the Examples

```bash
./json_usage_examples.sh
```

## JSON Output Format

The JSON output provides complete information about all USB hubs and connected devices:

```json
{
  "hubs": [
    {
      "location": "3-1",
      "description": "Hub description string",
      "hub_info": {
        "vid": "0x05e3",
        "pid": "0x0608",
        "usb_version": "2.00",
        "nports": 4,
        "ppps": "ppps"
      },
      "ports": [
        {
          "port": 1,
          "status": "0x0103",
          "flags": {
            "connection": true,
            "enable": true,
            "power": true
          },
          "human_readable": {
            "connection": "Device is connected",
            "enable": "Port is enabled", 
            "power": "Port power is enabled"
          },
          "speed": "USB2.0 High Speed 480Mbps",
          "speed_bps": 480000000,
          "vid": "0x0781",
          "pid": "0x5567",
          "vendor": "SanDisk",
          "product": "Cruzer Blade",
          "device_class": 0,
          "class_name": "Mass Storage",
          "usb_version": "2.00",
          "device_version": "1.00",
          "nconfigs": 1,
          "serial": "4C530001234567891234",
          "is_mass_storage": true,
          "interfaces": [...],
          "description": "0781:5567 SanDisk Cruzer Blade 4C530001234567891234"
        }
      ]
    }
  ]
}
```

## Common Use Cases

### 1. Find Device by Serial Number
```bash
SERIAL="4C530001234567891234"
uhubctl -j | jq -r --arg s "$SERIAL" '.hubs[] | . as $h | .ports[] | select(.serial == $s) | "uhubctl -l \($h.location) -p \(.port) -a cycle"'
```

### 2. List All Mass Storage Devices
```bash
uhubctl -j | jq -r '.hubs[].ports[] | select(.is_mass_storage == true) | .description'
```

### 3. Find Empty Ports
```bash
uhubctl -j | jq -r '.hubs[] | . as $h | .ports[] | select(.vid == null) | "Hub \($h.location) Port \(.port)"'
```

### 4. Generate Control Commands for Device Type
```bash
# Power off all FTDI devices
uhubctl -j | jq -r '.hubs[] | . as $h | .ports[] | select(.vendor == "FTDI") | "uhubctl -l \($h.location) -p \(.port) -a off"' | bash
```

### 5. Monitor for Device Changes
```bash
# Save baseline
uhubctl -j > baseline.json

# Later, check what changed
uhubctl -j | jq -r --argjson old "$(cat baseline.json)" '
  . as $new | 
  $old.hubs[].ports[] as $op | 
  $new.hubs[] | . as $h | 
  .ports[] | 
  select(.port == $op.port and $h.location == $op.hub_location and .vid != $op.vid) | 
  "Change at \($h.location):\(.port)"'
```

## JSON Fields Reference

### Hub Object
- `location` - Hub location (e.g., "3-1", "1-1.4")
- `description` - Full hub description string
- `hub_info` - Parsed hub information
  - `vid` - Vendor ID in hex
  - `pid` - Product ID in hex  
  - `usb_version` - USB version string
  - `nports` - Number of ports
  - `ppps` - Power switching mode
- `ports` - Array of port objects

### Port Object
- `port` - Port number
- `status` - Raw status value in hex
- `flags` - Boolean flags (only true values included)
- `human_readable` - Human-readable flag descriptions
- `speed` - Speed description string
- `speed_bps` - Speed in bits per second (numeric)
- `vid`, `pid` - Device vendor/product IDs
- `vendor`, `product` - Device vendor/product names
- `serial` - Device serial number
- `device_class` - USB device class code
- `class_name` - Device class name
- `is_mass_storage` - Boolean flag for mass storage devices
- `interfaces` - Array of interface descriptors

### USB3-Specific Fields
- `port_speed` - Link speed (e.g., "5gbps")
- `link_state` - USB3 link state (e.g., "U0", "U3")

## Tips

1. Use `jq -r` for raw output (no quotes)
2. Use `select()` to filter results
3. Use `. as $var` to save context when diving into nested objects
4. Use `// "default"` to provide default values for missing fields
5. Combine with shell scripts for automation