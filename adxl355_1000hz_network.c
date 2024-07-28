#include <bcm2835.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>

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

float scale_factor = 0.0000038; // For 2G range

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

int main(void) {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};
    
    if (!bcm2835_init() || !bcm2835_spi_begin()) {
        fprintf(stderr, "Failed to initialize BCM2835 library\n");
        return 1;
    }
    
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_32);
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
    
    adxl355_init();
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    printf("Waiting for connection on port %d...\n", PORT);
    
    if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
    
    printf("Connection established. Starting data streaming at 1000 Hz...\n");
    
    struct timespec start, end, sleep_time;
    float x, y, z;
    long loop_count = 0;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    while (1) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        
        adxl355_read_xyz(&x, &y, &z);
        
        snprintf(buffer, BUFFER_SIZE, "%ld.%09ld,%.6f,%.6f,%.6f\n", ts.tv_sec, ts.tv_nsec, x, y, z);
        send(client_socket, buffer, strlen(buffer), 0);
        
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
    }
    
    close(client_socket);
    close(server_fd);
    bcm2835_spi_end();
    bcm2835_close();
    return 0;
}
