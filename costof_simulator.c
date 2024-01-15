/*
 * 
 *
 *  @author: Enoc Martínez
 *  @institution: Universitat Politècnica de Catalunya (UPC)
 *  @contact: enoc.martinez@upc.edu
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "costof_simulator.h"
#include "linux_uart.h"

void* fastMalloc(int size){
	void *mem = malloc(size);
	return mem;
}

void fastFree(void* p) {
	free(p);
}

int les_writeLine(int fd, int timeoutMs, char* line){
	return les_write(fd, timeoutMs, line, strlen(line));
}

int les_write(int fd, int timeoutMs, char* buff, int nbChars) {
	return linux_write_uart(fd, buff, nbChars);
}


int les_open_serial_port(char* device, int baudrate) {
	return linux_open_uart(device, baudrate);
}

int les_read(int fd, int timeoutMs, char* buff, int nbChars){
	int r = linux_read_uart(fd, buff, nbChars, timeoutMs);
	return r;
}

int les_getLine(int fd, int timeoutMs, char* buff, int maxLineSize)
{
	char rxChar;
	int cnt = 0;
	int res          = -1;
	while (les_read(fd, timeoutMs, &rxChar, 1) >= 0)
	{
		buff[cnt] = rxChar;
		cnt++;

		if (cnt == (((unsigned int)maxLineSize) - 1))
		{
			buff[cnt] = 0; //end of string
			res = 0;
			break;
		}

		if (rxChar == '\n')
		{
			res       = cnt;
			buff[cnt] = 0;
			break;
		}
	}
	if (res == -1)
	{
		//speLOG(LOG_ERR, "Serial ,Error receiving line");
	}
	return res;
}


int les_resetRxFifo(int fd){
	return linux_fflush_uart(fd);
}


int set_log_colour(int level){
	switch(level){
		case LOG_CRITICAL:
			printf(KRED);
			break;
		case LOG_ERR:
			printf(KRED);
			break;
		case LOG_WARNING:
			printf(KYEL);
			break;
		case LOG_DETAIL:
			printf(KBLU);
			break;
		case LOG_INFO:
			printf(KGRN);
			break;
		case LOG_NOTICE:
			printf(KWHT);
			break;
		default:  //nrm
			printf(KRST);
			break;
	}
	return 0;
}

const char* loglvl[] = {"DBG", "DTL", "INF", "WRN", "NTC", "ERR", "CRT"};


double linux_get_epoch_time(){
	struct timespec ts;
	double epoch;
	// Get time from system-wide clock
	if (clock_gettime(CLOCK_REALTIME, &ts) < 0) {
		return -1.0;
	}
	epoch = (double)ts.tv_sec;
	epoch += ((double)ts.tv_nsec) / 1e9;
	return (double)epoch;
}

int speLOG(int level,  const char *format, ...){
	va_list	__ap;
	va_start(__ap, format);
	set_log_colour(level);

	time_t rawtime;
	struct tm * timeinfo;

	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	printf("%04d-%02d-%02d %02d:%02d:%02d ",
			timeinfo->tm_year + 1900,
			timeinfo->tm_mon + 1,
			timeinfo->tm_mday,
			timeinfo->tm_hour,
			timeinfo->tm_min,
			timeinfo->tm_sec);

	printf("%s: ", loglvl[level]);
	vprintf(format, __ap);
	va_end(__ap);
	printf("\r\n");
	printf(KNRM);
	fflush(stdout);
	return 0;
}
