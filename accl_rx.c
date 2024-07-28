#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

#define PORT 65432
#define BUFFER_SIZE 4096
#define CHUNK_DURATION 600 // 10 minutes in seconds
#define PRINT_INTERVAL 10000 // Print status every 10,000 samples
#define MAX_RETRIES 5
#define RETRY_DELAY 1000000 // 1 second in microseconds

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
    fprintf(log_file, "[%s] %s\n", timestamp, message);
    fflush(log_file);
}

void create_log_file() {
    char log_folder[256] = "logs";
    char log_filename[512];
    char full_path[768];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    // Create logs directory if it doesn't exist
    mkdir(log_folder, 0777);
    
    // Create the log filename
    strftime(log_filename, sizeof(log_filename), "%Y-%m-%d_%H-%M-%S_accl_rx.log", t);
    
    // Combine folder and filename
    snprintf(full_path, sizeof(full_path), "%s/%s", log_folder, log_filename);
    
    log_file = fopen(full_path, "w");
    if (log_file == NULL) {
        fprintf(stderr, "Error creating log file '%s': %s\n", full_path, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void create_output_folders(char *output_folder) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char folder_name[100];
    
    strftime(folder_name, sizeof(folder_name), "outputs/%d-%m-%Y-%H-%M-accl-output", tm);
    sprintf(output_folder, "%s", folder_name);
    
    mkdir("outputs", 0777);
    mkdir(output_folder, 0777);
    
    log_message("Created output folder");
}

FILE* open_new_file(const char *output_folder, int chunk_number, double start_time) {
    char filename[256];
    sprintf(filename, "%s/%.6f_chunk_%04d.bin", output_folder, start_time, chunk_number);
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Error opening file: %s", strerror(errno));
        log_message(error_msg);
        exit(EXIT_FAILURE);
    }
    setvbuf(file, NULL, _IOFBF, BUFFER_SIZE);  // Set to full buffering
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "Opened new file: %s", filename);
    log_message(log_msg);
    return file;
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char output_folder[256];
    char incomplete_line[256] = {0};
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    create_log_file();
    log_message("Program started");
    
    create_output_folders(output_folder);
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        log_message("Socket creation error");
        return -1;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, "192.168.40.61", &serv_addr.sin_addr) <= 0) {
        log_message("Invalid address/ Address not supported");
        return -1;
    }
    
    int retry_count = 0;
    while (retry_count < MAX_RETRIES) {
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Connection Failed. Retrying... (%d/%d)", retry_count + 1, MAX_RETRIES);
            log_message(error_msg);
            usleep(RETRY_DELAY);
            retry_count++;
        } else {
            break;
        }
    }
    
    if (retry_count == MAX_RETRIES) {
        log_message("Max retries reached. Exiting.");
        return -1;
    }
    
    log_message("Connected to server. Starting data collection...");
    
    FILE *current_file = NULL;
    int chunk_number = 1;
    double chunk_start_time = 0;
    long samples_received = 0;
    time_t start_time_t, current_time_t;
    double start_time = 0;
    time(&start_time_t);
    
    while (keep_running) {
        int valread = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (valread <= 0) {
            if (valread == 0) {
                log_message("Server closed the connection");
            } else {
                log_message("recv failed");
            }
            break;
        }
        
        buffer[valread] = '\0';  // Null-terminate the received data
        
        char *line_start = buffer;
        char *line_end;
        
        // Process any incomplete line from the previous iteration
        if (incomplete_line[0] != '\0') {
            char *newline = strchr(buffer, '\n');
            if (newline) {
                strncat(incomplete_line, buffer, newline - buffer + 1);
                line_start = newline + 1;
                
                double timestamp, x, y, z;
                if (sscanf(incomplete_line, "%lf,%lf,%lf,%lf", &timestamp, &x, &y, &z) == 4) {
                    if (start_time == 0) start_time = timestamp;
                    
                    if (current_file == NULL) {
                        chunk_start_time = timestamp;
                        current_file = open_new_file(output_folder, chunk_number, chunk_start_time);
                    }
                    
                    fwrite(&timestamp, sizeof(double), 1, current_file);
                    fwrite(&x, sizeof(double), 1, current_file);
                    fwrite(&y, sizeof(double), 1, current_file);
                    fwrite(&z, sizeof(double), 1, current_file);
                    
                    samples_received++;
                } else {
                    char error_msg[512];
                    snprintf(error_msg, sizeof(error_msg), "Warning: Invalid data format in incomplete line: %s", incomplete_line);
                    log_message(error_msg);
                    // Write the invalid data to the file anyway
                    fprintf(current_file, "%s\n", incomplete_line);
                }
                
                incomplete_line[0] = '\0';  // Clear the incomplete line buffer
            }
        }
        
        while ((line_end = strchr(line_start, '\n')) != NULL) {
            *line_end = '\0';  // Temporarily replace newline with null terminator
            
            double timestamp, x, y, z;
            if (sscanf(line_start, "%lf,%lf,%lf,%lf", &timestamp, &x, &y, &z) == 4) {
                if (start_time == 0) start_time = timestamp;
                
                if (current_file == NULL) {
                    chunk_start_time = timestamp;
                    current_file = open_new_file(output_folder, chunk_number, chunk_start_time);
                }
                
                fwrite(&timestamp, sizeof(double), 1, current_file);
                fwrite(&x, sizeof(double), 1, current_file);
                fwrite(&y, sizeof(double), 1, current_file);
                fwrite(&z, sizeof(double), 1, current_file);
                
                samples_received++;
                
                if (samples_received % PRINT_INTERVAL == 0) {
                    time(&current_time_t);
                    double elapsed_time = difftime(current_time_t, start_time_t);
                    double average_rate = samples_received / (timestamp - start_time);
                    
                    char status_msg[512];
                    snprintf(status_msg, sizeof(status_msg), "Samples: %ld | Elapsed time: %.0f s | Avg rate: %.2f Hz", 
                           samples_received, elapsed_time, average_rate);
                    log_message(status_msg);
                }
                
                if (timestamp - chunk_start_time >= CHUNK_DURATION) {
                    fclose(current_file);
                    chunk_number++;
                    current_file = open_new_file(output_folder, chunk_number, timestamp);
                    chunk_start_time = timestamp;
                }
            } else {
                char error_msg[512];
                snprintf(error_msg, sizeof(error_msg), "Warning: Invalid data format: %s", line_start);
                log_message(error_msg);
                // Write the invalid data to the file anyway
                fprintf(current_file, "%s\n", line_start);
            }
            
            line_start = line_end + 1;  // Move to the start of the next line
        }
        
        // If there's any remaining incomplete line, save it for the next iteration
        if (*line_start != '\0') {
            strcpy(incomplete_line, line_start);
        }
    }
    
    if (current_file != NULL) {
        fclose(current_file);
    }
    
    close(sock);
    
    char final_msg[512];
    snprintf(final_msg, sizeof(final_msg), "Data collection complete. Total samples received: %ld", samples_received);
    log_message(final_msg);
    
    fclose(log_file);
    return 0;
}