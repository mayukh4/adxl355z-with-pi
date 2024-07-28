import socket
from datetime import datetime
import matplotlib.pyplot as plt
import time
import signal
import numpy as np

# Network setup
HOST = '192.168.40.61'  # Hardcoded Raspberry Pi IP address
PORT = 65432  # The port used by the server

# Data storage
timestamps = []
x_data, y_data, z_data = [], [], []

# Plotting setup
plt.ion()  # Turn on interactive mode
fig, axes = plt.subplots(3, 1, figsize=(10, 8), sharex=True)
fig.suptitle('ADXL355 Accelerometer Data', fontsize=16)
lines = [ax.plot([], [], '-')[0] for ax in axes]

for ax, title in zip(axes, ['X-axis', 'Y-axis', 'Z-axis']):
    ax.set_ylim(-20, 20)  # Adjusted for m/s^2
    ax.set_title(title)
    ax.grid(True)
    ax.set_ylabel('Acceleration (m/s^2)')

axes[-1].set_xlabel('Samples')
fig.tight_layout()

# Flag to control the main loop
running = True

def signal_handler(sig, frame):
    global running
    print("\nStopping data collection...")
    running = False

signal.signal(signal.SIGINT, signal_handler)

def update_plot():
    for i, line in enumerate(lines):
        data = [x_data, y_data, z_data][i]
        line.set_data(range(len(data)), data)
        axes[i].relim()
        axes[i].autoscale_view()
    fig.canvas.draw()
    fig.canvas.flush_events()

def receive_and_plot_data():
    global timestamps, x_data, y_data, z_data
    
    while running:
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                print(f"Attempting to connect to {HOST}:{PORT}...")
                s.connect((HOST, PORT))
                print(f"Connected to {HOST}:{PORT}. Receiving data...")
                
                last_plot_time = time.time()
                buffer = ""
                
                while running:
                    data = s.recv(1024).decode()
                    if not data:
                        break
                    buffer += data
                    lines = buffer.split('\n')
                    for line in lines[:-1]:  # Process all complete lines
                        try:
                            timestamp, x, y, z = line.strip().split(',')
                            
                            # Update data for plotting
                            timestamps.append(timestamp)
                            x_data.append(float(x))
                            y_data.append(float(y))
                            z_data.append(float(z))
                            
                            # Keep only the last 1000 points for plotting
                            if len(x_data) > 1000:
                                timestamps = timestamps[-1000:]
                                x_data = x_data[-1000:]
                                y_data = y_data[-1000:]
                                z_data = z_data[-1000:]
                        except ValueError as e:
                            print(f"Error parsing data: {e}. Skipping this line.")
                    buffer = lines[-1]  # Keep the last incomplete line in the buffer
                    
                    # Update plot every 100ms
                    current_time = time.time()
                    if current_time - last_plot_time > 0.1:
                        update_plot()
                        last_plot_time = current_time
                
        except ConnectionRefusedError:
            print(f"\nConnection to {HOST}:{PORT} was refused. Retrying in 5 seconds...")
            time.sleep(5)
        except Exception as e:
            print(f"\nAn unexpected error occurred: {e}")
            print("Retrying connection in 5 seconds...")
            time.sleep(5)

if __name__ == "__main__":
    print("Starting live plotting. Press Ctrl+C to stop.")
    receive_and_plot_data()
    print("\nPlotting stopped.")
    plt.close(fig)
