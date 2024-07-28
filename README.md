# ADXL355 Accelerometer Data Collection System with Pi

This system collects data from an ADXL355 accelerometer connected to a Raspberry Pi and streams it to a local computer for storage and analysis.
Since the core libraries are written in c, making it much faster and more reliable.

## System Components

1. Raspberry Pi (transmitter)
2. Local computer (receiver)
3. ADXL355 accelerometer

## File Structure

### On Raspberry Pi:
```
/home/your_folder_name/accl_c/
├── accl_tx.c
├── accl_tx (compiled executable)
└── logs/ (created during execution)
```

### On Local Computer:
```
/path/to/project/
├── accl_rx.c
├── run_accl.sh
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

3. Copy `accl_tx.c` to `/home/bvex/accl_c/` on the Raspberry Pi.

4. Compile the transmitter program:
   ```bash
   gcc -o accl_tx accl_tx.c -lbcm2835 -lm
   ```

### 2. Local Computer Setup

1. Ensure you have GCC installed for compiling C programs.

2. Copy `accl_rx.c` and `run_accl.sh` to your project directory.


## Execution Instructions

1. Ensure the ADXL355 is properly connected to the Raspberry Pi (see Pin Connections table below).

2. On your local computer, navigate to the project directory and run:
   ```bash
   ./run_accl.sh
   ```

3. The script will compile both the transmitter (on Raspberry Pi) and receiver programs, start the data collection, and display the elapsed time.

   
5. You would need to enter the pi's password multiple times.

6. To stop the data collection, press Ctrl+C.

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

## Troubleshooting

- Check the log files in the `logs/` directory on both the Raspberry Pi and local computer for any error messages.
- Verify that the SPI interface is enabled on the Raspberry Pi.
- Ensure that the bcm2835 library is properly installed on the Raspberry Pi.
