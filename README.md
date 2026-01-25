# BLEMCPServer (MCP over BLE for ESP32)

![Build Status](https://img.shields.io/badge/build-not_configured-lightgrey)
![Version](https://img.shields.io/badge/version-0.0.1-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Code Quality](https://img.shields.io/badge/code_quality-not_configured-lightgrey)

## Description
BLEMCPServer is a lightweight Model Context Protocol (MCP) server implementation for ESP32 that communicates over Bluetooth Low Energy (BLE). It exposes MCP tools via JSON-RPC, supports message fragmentation/reassembly for BLE MTU constraints, and provides a practical WiFi provisioning demo. The library focuses exclusively on BLE transport, making it suitable for embedded devices where WiFi or HTTP transports are unnecessary or too heavy.

## Table of Contents
- [Project Title & Badges](#blemcpserver-mcp-over-ble-for-esp32)
- [Description](#description)
- [Prerequisites & Installation](#prerequisites--installation)
- [Usage](#usage)
- [Configuration](#configuration)
- [Project Structure](#project-structure)
- [Testing](#testing)
- [Deployment](#deployment)
- [Built With](#built-with)
- [Contributing](#contributing)
- [License](#license)
- [Acknowledgments](#acknowledgments)

## Prerequisites & Installation
### System Requirements
- ESP32 (tested with ESP32-S3)
- PlatformIO (recommended) or Arduino IDE
- Python (for PlatformIO)
- Git

### Clone the Repository
```bash
git clone git@github.com:solnera/BLEMCPServer.git
cd BLEMCPServer
```

### Install Dependencies (PlatformIO)
Dependencies are managed via `platformio.ini` in the example projects and include:
- h2zero/NimBLE-Arduino
- bblanchon/ArduinoJson

Install and build:
```bash
cd examples/config_wifi
pio run
```

## Usage
### Flash the Example (PlatformIO)
```bash
cd examples/config_wifi
pio run -t upload
pio device monitor
```

### BLE MCP Client Flow
1. Connect to the BLE device (default name: `MCP_Server_BLE`).
2. Use the MCP JSON-RPC API to discover tools and call them.

#### List Available Tools
```json
{"jsonrpc":"2.0","id":1,"method":"tools/list"}
```

#### Configure WiFi
```json
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"config_wifi","arguments":{"ssid":"YOUR_SSID","password":"YOUR_PASSWORD"}}}
```

#### Check WiFi Status
```json
{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"get_status","arguments":{}}}
```

### BLE GATT Details
The BLE transport uses the following service/characteristics:
- **Service UUID**: `00001999-0000-1000-8000-00805F9B34FB`
- **RX UUID**: `4963505F-5258-4000-8000-00805F9B34FB` (client writes)
- **TX UUID**: `4963505F-5458-4000-8000-00805F9B34FB` (server notifies)

## Configuration
### Server Metadata
The server name, version, and instructions are configured when creating `MCPServer` in the example:
```cpp
MCPServer mcpServer("ESP32-MCP-BLE", "1.0.0", "MCP WiFi configuration tool");
```

### WiFi Provisioning
WiFi credentials are sent over MCP via `config_wifi` and validated on-device. The example logs connection status to the serial monitor.

### BLE Device Name
The default BLE device name is `MCP_Server_BLE`. To customize it, initialize BLE before starting the MCP server:
```cpp
McpBle::getInstance().init("Your_Device_Name");
mcpServer.begin();
```

## Project Structure
```
.
├── examples/
│   └── config_wifi/        # WiFi provisioning demo using MCP tools
├── include/                # Public headers (MCPServer, McpBle, transport API)
├── src/                    # Core implementation (MCPServer, BLE, transport)
├── library.json            # PlatformIO library manifest
```

## Testing
No automated test suite is included yet. Validation is currently performed by flashing the example to an ESP32 device and exercising the MCP tools over BLE.

## Deployment
Deployment consists of flashing the firmware to an ESP32 device. No cloud or server deployment is required.

## Built With
- ESP32 Arduino Framework
- NimBLE-Arduino
- ArduinoJson
- FreeRTOS (ESP32 SDK)

## Contributing
1. Fork the repository
2. Create a feature branch: `git checkout -b feature/your-feature`
3. Commit your changes with clear messages
4. Push to your fork and open a Pull Request

Please open issues for bug reports or feature requests with clear reproduction steps.

## License
MIT License. See `library.json` for license metadata.

## Acknowledgments
- NimBLE-Arduino for BLE stack support
- ArduinoJson for efficient JSON handling
- The MCP community and reference implementations for protocol alignment
