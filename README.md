# Phototransistor PI Control on Raspberry Pi Pico 2

This project implements a closed-loop PI controller for a phototransistor signal on a Raspberry Pi Pico 2. The firmware samples the ADC at 1 kHz, drives PWM at 20 kHz, and streams captured response data over USB serial for analysis.

## Features

- 1 kHz control loop with PI control (`kp`, `ki` set at runtime)
- 20 kHz PWM output for actuator drive
- Anti-windup behavior in the integral term
- Reference waveform switching between 25% and 75% ADC scale
- USB serial capture of measured vs desired values
- Python script to automate capture and generate plots

## Repository Structure

- `phototransistor.c`: Pico firmware (ADC read, PI loop, PWM update, serial output)
- `CMakeLists.txt`: Pico SDK build configuration
- `capture_plot.py`: Host-side capture and plotting tool
- `plots/`: Generated plot images
- `Media/`: Optional media assets

## Hardware Requirements

- Raspberry Pi Pico 2
- Phototransistor analog signal connected to GPIO26 / ADC0
- PWM output available on GPIO16
- USB connection to host computer

## Software Requirements

- CMake (>= 3.13)
- ARM embedded toolchain compatible with Pico SDK
- Raspberry Pi Pico SDK
- Python 3.9+
- Python packages:
  - `pyserial`
  - `matplotlib`

Install Python dependencies:

```bash
python3 -m pip install pyserial matplotlib
```

## Build Firmware

From the repository root:

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

The build produces `phototransistor.uf2` in the build output.

## Flash Firmware

1. Hold `BOOTSEL` while connecting the Pico to USB.
2. Copy `phototransistor.uf2` to the mounted `RPI-RP2` drive.
3. Reconnect normally for USB serial communication.

## Run a Capture

1. Find your serial device path (example: `/dev/tty.usbmodem1101`).
2. Run the capture script and provide controller gains:

```bash
python3 capture_plot.py --port /dev/tty.usbmodem1101 --kp 1.0 --ki 0.01
```

Useful flags:

- `--baud` (default `230400`)
- `--timeout` (default `2.0` seconds)
- `--save-dir` (default `plots`)
- `--no-show` to skip opening a plot window

Each run saves a timestamped PNG in `plots/`.

## Firmware Runtime Behavior

- On startup, firmware waits for a USB serial connection.
- It prompts for `kp ki` values over serial.
- For each input pair, it captures 200 decimated samples.
- Output lines are emitted as:

```text
index measured_adc desired_adc
```

The host script reads this stream and generates a comparison plot.

## Notes

- Optional demo media can be stored in `Media/`, but project usage does not depend on those files.
- Large binary assets are best managed outside core source history unless required.
