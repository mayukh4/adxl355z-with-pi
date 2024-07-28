#!/bin/bash

# Configuration
RPI_USER="bvex"
RPI_HOST="raspberrypi.local"
RPI_C_FILE="/home/bvex/accl_c/accl_tx.c"
RPI_EXECUTABLE="/home/bvex/accl_c/accl_tx"
RPI_LOG_DIR="/home/bvex/accl_c/logs"
LOCAL_C_FILE="accl_rx.c"
LOCAL_EXECUTABLE="accl_rx"
LOG_DIR="logs"
SCRIPT_LOG="${LOG_DIR}/run_accl_$(date +%Y-%m-%d_%H-%M-%S).log"

# Function to log messages
log_message() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$SCRIPT_LOG"
}

# Create log directory if it doesn't exist
mkdir -p "$LOG_DIR"

# Start logging
log_message "Starting run_accl.sh script"

# Record start time
start_time=$(date +%s)

# Function to display elapsed time
display_elapsed_time() {
    current_time=$(date +%s)
    elapsed=$((current_time - start_time))
    printf "\rElapsed time: %02d:%02d:%02d" $((elapsed/3600)) $((elapsed%3600/60)) $((elapsed%60))
}

# Create log directory on Raspberry Pi
log_message "Creating log directory on Raspberry Pi..."
ssh ${RPI_USER}@${RPI_HOST} "mkdir -p ${RPI_LOG_DIR} && chmod 777 ${RPI_LOG_DIR}"

# Function to kill the C program on the Raspberry Pi
kill_rpi_program() {
    log_message "Stopping the C program on Raspberry Pi..."
    ssh ${RPI_USER}@${RPI_HOST} "sudo killall accl_tx" || log_message "Failed to stop the C program. It may have already exited."
}

# Set up trap to kill the C program when this script exits
trap kill_rpi_program EXIT

# Compile the C program on Raspberry Pi
log_message "Compiling the C program on Raspberry Pi..."
ssh ${RPI_USER}@${RPI_HOST} "gcc -o ${RPI_EXECUTABLE} ${RPI_C_FILE} -lbcm2835 -lm"

# Check if compilation was successful
if [ $? -ne 0 ]; then
    log_message "Compilation on Raspberry Pi failed. Exiting."
    exit 1
fi

# Compile the local C program for data reception
log_message "Compiling the local C program for data reception..."
gcc -o ${LOCAL_EXECUTABLE} ${LOCAL_C_FILE}

# Check if local compilation was successful
if [ $? -ne 0 ]; then
    log_message "Local compilation failed. Exiting."
    exit 1
fi

# Start the C program on Raspberry Pi
log_message "Starting the C program on Raspberry Pi..."
ssh ${RPI_USER}@${RPI_HOST} "sudo ${RPI_EXECUTABLE}" &

# Wait for the C program to start and begin listening
log_message "Waiting for the C program to start..."
sleep 10

# Check if the C program is running
if ! ssh ${RPI_USER}@${RPI_HOST} "ps aux | grep -v grep | grep -q accl_tx"; then
    log_message "C program failed to start or exited prematurely. Checking for error messages..."
    ssh ${RPI_USER}@${RPI_HOST} "ls -l ${RPI_LOG_DIR} && cat ${RPI_LOG_DIR}/*"
    exit 1
fi

# Check if the port is open
if ! ssh ${RPI_USER}@${RPI_HOST} "netstat -tuln | grep -q :65432"; then
    log_message "C program is running but not listening on port 65432. Exiting."
    exit 1
fi

log_message "C program is running and listening on port 65432."

# Start the local C program for data reception
log_message "Starting the local C program for data reception..."
./${LOCAL_EXECUTABLE} &

# Display elapsed time every second
while true; do
    display_elapsed_time
    sleep 1
done