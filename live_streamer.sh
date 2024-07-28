#!/bin/bash

# Define remote and local details
REMOTE_USER="bvex"
REMOTE_HOST="raspberrypi.local"
REMOTE_SCRIPT="/home/bvex/accl_c/accl3.py"
LOCAL_SCRIPT="live_streamer.py"

# SSH into Raspberry Pi and start the accl3 script
echo "Starting accl3 script on Raspberry Pi..."
ssh ${REMOTE_USER}@${REMOTE_HOST} "python3 ${REMOTE_SCRIPT}" &
REMOTE_PID=$!

# Check if the remote script started successfully
if [ $? -ne 0 ]; then
    echo "Failed to start accl3 script on Raspberry Pi."
    exit 1
fi

# Wait for a few seconds to ensure the remote script is running
sleep 5

# Start the local live_streamer.py script
echo "Starting live_streamer.py script on local machine..."
python3 ${LOCAL_SCRIPT}

# After the local script ends, stop the remote accl3 script
echo "Stopping accl3 script on Raspberry Pi..."
ssh ${REMOTE_USER}@${REMOTE_HOST} "pkill -f ${REMOTE_SCRIPT}"

# Ensure the remote process is killed
wait ${REMOTE_PID}

echo "All scripts have been stopped."

