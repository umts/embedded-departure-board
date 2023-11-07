# embedded-departure-board

## Requirements
Follow the [Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html) to install the required development tools (system packages, Zephyr SDK, and udev rules).

To save on space, you may want to intall a minimal bundle Zephyr SDK [release](https://github.com/zephyrproject-rtos/sdk-ng/releases) and the arm-zephyr-eabi toolchain. The full release includes all avaiable toolchains.

## Setup
1. Clone this repository
2. Create a Python virtual environment
   ```sh
   cd embedded-departure-board && python3 -m venv ./.venv
   ```
4. Activate your Python virtual environment
   ```sh
   source .venv/bin/activate
   ```
6. Install West
   ```sh
   pip install west
   ```
8. Run
   ```sh
   west update
   ```

## Build
```sh
west build app
```

## Flashing
```sh
west flash -r nrfjprog --erase --softreset
```
