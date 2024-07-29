# ADXL355 Accelerometer Data Collection and Analysis System

This system collects data from an ADXL355 accelerometer connected to a Raspberry Pi, streams it wirelessly over WiFi to a local computer for storage and analysis, and provides options for live plotting and post-recording analysis.

![IMG_6595](https://github.com/user-attachments/assets/fd3d1882-ac5f-4cc6-88a1-8f395acd1b52)


## Quick Start

For live plotting only:
```bash
./live_streamer.sh
```

For full data streaming and recording functionality:
```bash
./run_accl.sh
```

Note: The main scripts are written in C for optimal performance.

## System Components

1. Raspberry Pi (transmitter)
2. Local computer (receiver)
3. ADXL355 accelerometer

## File Structure

### On Raspberry Pi:
```
/home/bvex/accl_c/
├── accl_tx.c
├── accl_tx (compiled executable)
├── accl3.py
└── logs/ (created during execution)
```

### On Local Computer:
```
/path/to/project/
├── accl_rx.c
├── run_accl.sh
├── live_streamer.sh
├── live_streamer.py
├── accl_data_analysis.ipynb
├── logs/ (created during execution)
└── outputs/ (created during execution)
```

## Setup Instructions

### 1. Raspberry Pi Setup

1. Install the bcm2835 library:
   ```bash
   wget http://www.airspayce.com/mikem/bcm2835/bcm2835-1.60.tar.gz
   tar zxvf bcm2835-1.60.tar.gz
   cd bcm2835-1.60
   ./configure
   make
   sudo make check
   sudo make install
   ```

2. Enable SPI interface:
   ```bash
   sudo raspi-config
   ```
   Navigate to "Interfacing Options" > "SPI" and select "Yes" to enable it.

3. Copy `accl_tx.c` and `accl3.py` to `/home/bvex/accl_c/` on the Raspberry Pi.

4. Compile the transmitter program:
   ```bash
   gcc -o accl_tx accl_tx.c -lbcm2835 -lm
   ```

### 2. Local Computer Setup

1. Ensure you have GCC installed for compiling C programs.

2. Install required Python libraries:
   ```bash
   pip install numpy matplotlib pandas
   ```

3. Copy `accl_rx.c`, `run_accl.sh`, `live_streamer.sh`, `live_streamer.py`, and `accl_data_analysis.ipynb` to your project directory.

## Execution Instructions

1. Ensure the ADXL355 is properly connected to the Raspberry Pi (see Pin Connections table below).

2. For live plotting only:
   ```bash
   ./live_streamer.sh
   ```
   This will start the `accl3.py` script on the Raspberry Pi and the `live_streamer.py` script on your local computer for real-time visualization.

3. For full data streaming and recording:
   ```bash
   ./run_accl.sh
   ```
   This script will compile both the transmitter (on Raspberry Pi) and receiver programs, start the data collection, and display the elapsed time.

4. To stop the data collection, press Ctrl+C.

5. For post-recording analysis, use the `accl_data_analysis.ipynb` Jupyter notebook on your local computer.

## Pin Connections

Here's a table showing the connections between the ADXL355 and the Raspberry Pi:

| ADXL355 Pin | Raspberry Pi Pin | Description |
|-------------|------------------|-------------|
| VDD         | 3.3V             | Power supply |
| GND         | Ground           | Ground |
| SCLK        | GPIO 11 (PIN 23) | SPI Clock |
| MOSI        | GPIO 10 (PIN 19) | SPI MOSI |
| MISO        | GPIO 9 (PIN 21)  | SPI MISO |
| CS          | GPIO 8 (PIN 24)  | SPI Chip Select |

Note: Make sure to double-check these connections with your specific ADXL355 module's datasheet, as pin layouts can vary between different breakout boards.

## Output

The collected data will be stored in binary files in the `outputs/` directory on your local computer. Each file contains 10 minutes of data and is named with the timestamp of when it was created.

## Live Streaming

The `live_streamer.sh` script provides real-time visualization of the accelerometer data. It starts the `accl3.py` script on the Raspberry Pi, which streams data over WiFi, and the `live_streamer.py` script on your local computer, which receives the data and creates a live plot.

## Data Analysis

Use the `accl_data_analysis.ipynb` Jupyter notebook on your local computer for post-recording analysis. This notebook provides tools for loading the binary data files, processing the accelerometer data, and creating various visualizations and analyses.

## Troubleshooting

- If you encounter permission issues, ensure that the scripts are executable (`chmod +x script_name.sh`).
- Check the log files in the `logs/` directory on both the Raspberry Pi and local computer for any error messages.
- Verify that the SPI interface is enabled on the Raspberry Pi.
- Ensure that the bcm2835 library is properly installed on the Raspberry Pi.
- Make sure both the Raspberry Pi and local computer are on the same WiFi network.

## Data streamer panel

![0728-ezgif com-video-to-gif-converter](https://github.com/user-attachments/assets/a577f8fe-b92e-4522-93c0-8648a3c19b7c)


## Note on Performance

The main data collection scripts (`accl_tx.c` and `accl_rx.c`) are written in C for optimal performance, allowing for high-speed data collection and transmission. Data is transmitted wirelessly over WiFi from the Raspberry Pi to the local computer.
