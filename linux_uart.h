#ifndef LINUX_UART_H_
#define LINUX_UART_H_



/*
 * Structure that holds the information for LINUX UART devices
 */



int linux_open_uart(char* device, int baudrate);
int linux_write_uart(int fd, void* buffer, int size);
int linux_read_uart(int fd, char* buffer, int max_bytes, long int timeout_us);
int linux_close_uart(int fd);
int linux_fflush_uart(int fd);
int linux_set_baudrate(int fd, long int baudrate);


#endif //LINUX_UART_H_
