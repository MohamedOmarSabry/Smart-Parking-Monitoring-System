# Smart Parking Monitoring System

## Getting Started

### Prerequisites

- [VS Code](https://code.visualstudio.com/)
- [PlatformIO extension](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide) installed in VS Code

### Opening the Project

1. Clone the repo
2. Open VS Code
3. Click the PlatformIO icon in the left sidebar
4. Click **Open Project** and select the cloned folder
5. PlatformIO will automatically download the required framework and dependencies

## Wiring

### HC-SR04 Ultrasonic Sensor

| HC-SR04 Pin | ESP32 Pin |
| ----------- | --------- |
| VCC         | VIN       |
| GND         | GND       |
| TRIG        | D5        |
| ECHO        | D18       |

## Setup

### Add PlatformIO to PATH

PlatformIO's CLI tools aren't available in the terminal by default. To fix this, add the following to your shell config file (`.zshrc`, `.bashrc`, etc.):

```bash
export PATH="$HOME/.platformio/penv/bin:$PATH"
```

Then restart your terminal or run `source ~/.zshrc`. You should now be able to run `pio` commands.

## Build, Upload, and Monitor

### Using the Terminal

Build only:

```bash
pio run
```

Upload to board (automatically builds first if needed):

```bash
pio run --target upload
```

Upload and open Serial Monitor:

```bash
pio run --target upload && pio device monitor
```

### Using VS Code

1. Click the PlatformIO icon in the left sidebar
2. Expand **esp32dev → General**
3. You will find the following options:

- **Build** — compiles the code and checks for errors, nothing is sent to the board
- **Upload** — builds if needed, then flashes the code to the board
- **Upload and Monitor** — builds if needed, flashes the code, and immediately opens the Serial Monitor so you can see Serial output from the board
