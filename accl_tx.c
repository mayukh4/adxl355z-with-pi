#include <bcm2835.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>

#define ADXL355_DEVID_AD     0x00
#define ADXL355_RANGE        0x2C
#define ADXL355_POWER_CTL    0x2D
#define ADXL355_FILTER       0x28
#define ADXL355_XDATA3       0x08

#define ADXL355_RANGE_2G     0x01
#define ADXL355_ODR_1000     0x0002

#define SPI_CLOCK_SPEED 10000000  // 10 MHz
#define PORT 65432
#define BUFFER_SIZE 1024
#define WATCHDOG_TIMEOUT 5 // 5 seconds
#define MAX_RETRIES 5
#define RETRY_DELAY 1000000 // 1 second in microseconds

float scale_factor = 0.0000038; // For 2G range
volatile sig_atomic_t keep_running = 1;
FILE *log_file = NULL;

void signal_handler(int signum) {
    keep_running = 0;
}

void log_message(const char *message) {
    time_t now;
    char timestamp[64];
    time(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(stderr, "[%s] %s\n", timestamp, message);
    if (log_file) {
        fprintf(log_file, "[%s] %s\n", timestamp, message);
        fflush(log_file);
    }
}

void create_log_file() {
    char log_folder[256] = "logs";
    char log_filename[512];
    char full_path[768];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    mkdir(log_folder, 0777);
    
    strftime(log_filename, sizeof(log_filename), "%Y-%m-%d_%H-%M-%S_accl_tx.log", t);
    snprintf(full_path, sizeof(full_path), "%s/%s", log_folder, log_filename);
    
    log_file = fopen(full_path, "w");
    if (log_file == NULL) {
        fprintf(stderr, "Error creating log file '%s': %s\n", full_path, strerror(errno));
    } else {
        log_message("Log file created successfully");
    }
}

void adxl355_write_reg(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg << 1, value};
    bcm2835_spi_transfern((char *)buf, 2);
}

uint8_t adxl355_read_reg(uint8_t reg) {
    uint8_t buf[2] = {(reg << 1) | 0x01, 0};
    bcm2835_spi_transfern((char *)buf, 2);
    return buf[1];
}

void adxl355_init() {
    adxl355_write_reg(ADXL355_RANGE, ADXL355_RANGE_2G);
    adxl355_write_reg(ADXL355_FILTER, ADXL355_ODR_1000);
    adxl355_write_reg(ADXL355_POWER_CTL, 0x00); // Measurement mode
}

void adxl355_read_xyz(float *x, float *y, float *z) {
    uint8_t buffer[10];
    int32_t x_raw, y_raw, z_raw;
    
    buffer[0] = (ADXL355_XDATA3 << 1) | 0x01;  // Read command
    bcm2835_spi_transfern((char *)buffer, 10);  // Read 9 bytes of data + 1 command byte
    
    x_raw = ((int32_t)buffer[1] << 12) | ((int32_t)buffer[2] << 4) | (buffer[3] >> 4);
    y_raw = ((int32_t)buffer[4] << 12) | ((int32_t)buffer[5] << 4) | (buffer[6] >> 4);
    z_raw = ((int32_t)buffer[7] << 12) | ((int32_t)buffer[8] << 4) | (buffer[9] >> 4);
    
    // Convert to signed values
    if (x_raw & 0x80000) x_raw |= ~0xFFFFF;
    if (y_raw & 0x80000) y_raw |= ~0xFFFFF;
    if (z_raw & 0x80000) z_raw |= ~0xFFFFF;
    
    // Convert to m/s^2
    *x = x_raw * scale_factor * 9.81;
    *y = y_raw * scale_factor * 9.81;
    *z = z_raw * scale_factor * 9.81;
}

int setup_socket() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        log_message("Socket creation failed");
        return -1;
    }
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        log_message("Setsockopt failed");
        return -1;
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        log_message("Bind failed");
        return -1;
    }
    
    if (listen(server_fd, 3) < 0) {
        log_message("Listen failed");
        return -1;
    }
    
    log_message("Socket setup complete. Waiting for connection...");
    return server_fd;
}

int main(void) {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};
    struct timespec start, end, sleep_time;
    float x, y, z;
    long loop_count = 0;
    struct timeval last_activity;
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    create_log_file();
    log_message("Program started");
    
    if (!bcm2835_init()) {
        log_message("Failed to initialize BCM2835 library");
        return 1;
    }
    
    if (!bcm2835_spi_begin()) {
        log_message("Failed to initialize SPI");
        bcm2835_close();
        return 1;
    }
    
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_32);
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
    
    adxl355_init();
    log_message("ADXL355 initialized");
    
    server_fd = setup_socket();
    if (server_fd < 0) {
        log_message("Failed to set up socket");
        bcm2835_spi_end();
        bcm2835_close();
        return 1;
    }
    
    while (keep_running) {
        log_message("Waiting for client connection...");
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            log_message("Accept failed");
            continue;
        }
        
        log_message("Client connected. Starting data streaming at 1000 Hz...");
        gettimeofday(&last_activity, NULL);
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        while (keep_running) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            
            adxl355_read_xyz(&x, &y, &z);
            
            snprintf(buffer, BUFFER_SIZE, "%ld.%09ld,%.6f,%.6f,%.6f\n", ts.tv_sec, ts.tv_nsec, x, y, z);
            
            int retry_count = 0;
            while (retry_count < MAX_RETRIES) {
                if (send(client_socket, buffer, strlen(buffer), 0) < 0) {
                    log_message("Send failed. Retrying...");
                    usleep(RETRY_DELAY);
                    retry_count++;
                } else {
                    break;
                }
            }
            
            if (retry_count == MAX_RETRIES) {
                log_message("Max retries reached. Closing connection.");
                break;
            }
            
            loop_count++;
            
            clock_gettime(CLOCK_MONOTONIC, &end);
            long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);
            long target_ns = (loop_count * 1000000);  // 1000000 ns = 1 ms (1000 Hz)
            long sleep_ns = target_ns - elapsed_ns;
            
            if (sleep_ns > 0) {
                sleep_time.tv_sec = sleep_ns / 1000000000;
                sleep_time.tv_nsec = sleep_ns % 1000000000;
                nanosleep(&sleep_time, NULL);
            }
            
            // Check watchdog
            struct timeval now;
            gettimeofday(&now, NULL);
            double elapsed = (now.tv_sec - last_activity.tv_sec) + 
                             (now.tv_usec - last_activity.tv_usec) / 1000000.0;
            if (elapsed > WATCHDOG_TIMEOUT) {
                log_message("Watchdog timeout. Resetting connection.");
                break;
            }
            
            gettimeofday(&last_activity, NULL);
        }
        
        close(client_socket);
        log_message("Client disconnected");
    }
    
    log_message("Shutting down...");
    close(server_fd);
    bcm2835_spi_end();
    bcm2835_close();
    if (log_file) {
        fclose(log_file);
    }
    return 0;
}
