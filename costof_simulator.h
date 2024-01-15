/*
 * 
 *
 *  @author: Enoc Martínez
 *  @institution: Universitat Politècnica de Catalunya (UPC)
 *  @contact: enoc.martinez@upc.edu
 */



#ifndef COSTOF_SIMULATOR_H
#define COSTOF_SIMULATOR_H

#include <stdarg.h>

void* fastMalloc(int size);


typedef struct {
	int fd; // serial port fd
}LibSensor;


int les_open_serial_port(char* device, int baudrate);
int les_read(int fd, int timeoutMs, char* buff, int nbChars);
int les_getLine(int fd, int timeoutMs, char* buff, int maxLineSize);
int les_resetRxFifo(int fd);
int les_writeLine(int fd, int timeoutMs, char* line);
int les_write(int fd, int timeoutMs, char* buff, int nbChars);

int speLOG(int level,  const char *format, ...);


void fastFree(void* p);
void* fastMalloc(int size);





#define LOG_CRITICAL 6
#define LOG_ERR 5
#define LOG_NOTICE 4
#define LOG_WARNING 3
#define LOG_INFO 2
#define LOG_DETAIL 1
#define LOG_DEBUG 0


#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"
#define KRST "\033[0m"


#endif
