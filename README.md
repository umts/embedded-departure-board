# embedded-departure-board
## Development
### Requirements
- Python 3
- [Zephyr SDK](https://github.com/zephyrproject-rtos/sdk-ng/releases)
- [32 bit Swiftly API key](#building)

### Setup
1. Clone and enter the repo
2. Create a Python virtual environment
   ```sh
   python3 -m venv ./.venv
   ```
4. Activate your Python virtual environment
   ```sh
   source .venv/bin/activate
   # Or, if you're a direnv user, leave and come back
   cd
   cd -
   ```
6. Install West
   ```sh
   pip install west
   ```
8. Run
   ```sh
   west update
   ```
9. Install Python dependencies
   ```sh
   pip install -r zephyr/scripts/requirements.txt
   pip install -r nrf/scripts/requirements.txt
   pip install -r bootloader/mcuboot/scripts/requirements.txt
   ```

### Recomended
- Read the [Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html) to install the required development tools (system packages, Zephyr SDK, and udev rules).

- To save on space, you may want to intall a minimal bundle Zephyr SDK [release](https://github.com/zephyrproject-rtos/sdk-ng/releases) and the arm-zephyr-eabi toolchain. The full release includes all avaiable toolchains.

- Read the [Introduction to the nRF9160 Feather](https://docs.circuitdojo.com/nrf9160-introduction.html) to better understand the dev board we are using.

- If you plan making changes to the LTE modem configuration you should understand the various modes: [Maximizing battery lifetime in cellular IoT: An analysis of eDRX, PSM, and AS-RAI](https://devzone.nordicsemi.com/nordic/nordic-blog/b/blog/posts/maximizing-battery-lifetime-in-cellular-iot-an-analysis-of-edrx-psm-and-as-rai)

### Docker
Zephyr supplies various [Docker images](https://github.com/zephyrproject-rtos/docker-image#zephyr-docker-images) for development.

Our Github Actions [build workflow](https://github.com/umts/embedded-departure-board/blob/main/.github/workflows/build_test.yml) uses the Base Image (ci-base).


## Building
Currently this build requires a 32 bit [Swiftly](https://www.goswift.ly/) API key placed in `${CMAKE_CURRENT_SOURCE_DIR}/keys/private/swiftly-api.key` (Wrapped in `""`). If that key file does not exist CMake will create it with a fake key to allow the build to succeed.

If you are not using a [pre-signed binary](https://github.com/umts/embedded-departure-board/releases/latest) and you would like a functioning API key, [you can request one here](https://swiftly.zendesk.com/hc/en-us/requests/new?ticket_form_id=33738491027469).

### Testing

```sh
west build --sysbuild ./app -b circuitdojo_feather/nrf9160/ns
```
### Release

**You need a copy of the MCUBoot key file placed here:** `app/keys/private/boot-ecdsa-p256.pem`
```sh
west build --sysbuild ./app -b circuitdojo_feather/nrf9160/ns -- -DFILE_SUFFIX=release
```

## Programming
### Flashing
Flashing the device with an external programmer is quicker than using a bootloader. More importantly, it's the easiest way (and currently the only tested way) to secure the bootloader, update the modem firmware, and use the cortex-debugger.

#### Requirements
- External programming device, the [nRF5340 Dk](https://www.nordicsemi.com/Products/Development-hardware/nRF5340-DK) is what we currently use.
- 6-pin [Tag Connect cable](https://www.tag-connect.com/product/tc2030-ctx-nl-6-pin-no-legs-cable-with-10-pin-micro-connector-for-cortex-processors)
- [J-Link](https://www.segger.com/downloads/jlink/) software.
- [nRF Command Line Tools](https://www.nordicsemi.com/Products/Development-tools/nrf-command-line-tools/download#infotabs).

#### Running
```sh
west flash -r nrfjprog --erase --softreset
```

### Uploading via the bootloader
The only way to flash the device without an external programming device, uses serial over the builtin USB port.

#### Requirements
- [Newtmgr](https://mynewt.apache.org/latest/newtmgr/install/index.html)

#### Setup
1. Create a "serial" profile in newtmgr:
```sh
newtmgr conn add serial type=serial connstring="dev=/dev/ttyUSB0,baud=1000000"
```
2. Put the device into bootloader mode.

#### Run
```sh
newtmgr -c serial image upload ./build/app/zephyr/zephyr.signed.bin
```

## VSCode
This repo includes `.vscode/tasks.json` to make develpoment easier. The included tasks are:
- Build
- Load image via bootloader
  - Expects a connection profile named "serial" in `newtmgr`
- Serial Monitor
  - Uses `pyserial-miniterm`, which should already be installed in your venv
  - Assumes the device is connected at `/dev/ttyUSB0`
- West Flash
- West Flash and Monitor
- Build AT Client
  - Builds the At Client sample provided by nrf.
  - Useful for debugging via AT commands. Use a serial console to send AT commands

## Creating a Release
Update the [VERSION file](https://github.com/umts/embedded-departure-board/blob/main/app/VERSION).
On a successful push to the main branch the [release workflow](https://github.com/umts/embedded-departure-board/blob/main/.github/workflows/release.yml) will; create a new release, generate release notes, and upload the freshly built hex/bin files to the release.
