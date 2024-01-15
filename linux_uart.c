
#include <termios.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/file.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <termios.h>
#include <stdio.h>

struct termios current_settings;
struct termios original_settings;


int linux_set_baudrate(int fd, long int baudrate_in);

/*
 * Opens a Linux UART and returns a pointer to the Linux_UART structure containing
 * its settings and file descriptor
 */
int linux_open_uart(char* serial_device, int baudrate){
	int status;
	int error;

	int cbits=CS8,
	      cpar=0,
	      ipar=IGNPAR,
	      bstop=0;

	//open serial port and assign a file descriptor
	int fd = open(serial_device, O_RDWR | O_NOCTTY | O_NDELAY);
	if(fd==-1)   {
		printf("ERROR unable to open comport \n");
		exit(1);
	}

	  /* lock access so that another process can't also use the port */
	if(flock(fd, LOCK_EX | LOCK_NB) != 0) {
		close(fd);
		printf( "ERROR Another process has locked the comport\n");
		exit(1);
	}

	error = tcgetattr(fd, &original_settings);
	if(error==-1)  {
		close(fd);
	    flock(fd, LOCK_UN);  /* free the port so that others can use it. */
	    printf("ERROR unable to read portsettings\n");
	    exit(1);
	}
	memset(&current_settings, 0, sizeof(current_settings));  /* clear the new struct */

	current_settings.c_cflag = cbits | cpar | bstop | CLOCAL | CREAD;
	current_settings.c_iflag = ipar;
	current_settings.c_oflag = 0;
	current_settings.c_lflag = 0;
	current_settings.c_cc[VMIN] = 0;      /* block until n bytes are received */
	current_settings.c_cc[VTIME] = 0;     /* block until a timer expires (n * 100 mSec.) */

	//set baudrate

	 linux_set_baudrate(fd, baudrate);

	if(ioctl(fd, TIOCMGET, &status) == -1) {
		tcsetattr(fd, TCSANOW, &original_settings );
		flock(fd, LOCK_UN);  /* free the port so that others can use it. */
		printf("ERROR unable to get portstatus\n");
		exit(1);
	}

	status |= TIOCM_DTR;    /* turn on DTR */
	status |= TIOCM_RTS;    /* turn on RTS */

	if(ioctl(fd, TIOCMSET, &status) == -1) {
		tcsetattr(fd, TCSANOW, &original_settings);
		flock(fd, LOCK_UN);  /* free the port so that others can use it. */
		printf("ERROR unable to set port status");
		exit(1);
	}
	return fd;
}


int linux_set_baudrate(int fd, long int baudrate_in){
	ulong baudr;

	if (baudrate_in == 0){
		printf("Baudrate not specific, falling back to default %d\n", 9600);
	}

	switch(baudrate_in)
	  {
		case      50 : baudr = B50; break;
		case      75 : baudr = B75; break;
		case     110 : baudr = B110; break;
		case     134 : baudr = B134; break;
		case     150 : baudr = B150; break;
		case     200 : baudr = B200; break;
		case     300 : baudr = B300; break;
		case     600 : baudr = B600; break;
		case    1200 : baudr = B1200; break;
		case    1800 : baudr = B1800; break;
		case    2400 : baudr = B2400; break;
		case    4800 : baudr = B4800; break;
		case    9600 : baudr = B9600; break;
		case   19200 : baudr = B19200; break;
		case   38400 : baudr = B38400; break;
		case   57600 : baudr = B57600; break;
		case  115200 : baudr = B115200; break;
		case  230400 : baudr = B230400; break;
		case  460800 : baudr = B460800; break;
		case  500000 : baudr = B500000; break;
		case  576000 : baudr = B576000; break;
		case  921600 : baudr = B921600; break;
		case 1000000 : baudr = B1000000; break;
		case 1152000 : baudr = B1152000; break;
		case 1500000 : baudr = B1500000; break;
		case 2000000 : baudr = B2000000; break;
		case 2500000 : baudr = B2500000; break;
		case 3000000 : baudr = B3000000; break;
#ifndef __CYGWIN__ // Use only if CYGWIN is not defined (i.e. "pure" Linux)
		case 3500000 : baudr = B3500000; break;
		case 4000000 : baudr = B4000000; break;
#endif
		default      : printf( "ERROR invalid baudrate %li \n", baudrate_in);
		   return -1;
		   break;
	  }
	  cfsetispeed(&current_settings, baudr);
	  cfsetospeed(&current_settings, baudr);

	  if((tcsetattr(fd, TCSANOW, &current_settings))==-1) {
		tcsetattr(fd, TCSANOW, &original_settings);
		close(fd);
		flock(fd, LOCK_UN);  /* free the port so that others can use it. */
		printf("unable to adjust port settings \n");
		return(1);
	  }

	  return 0;
}



int linux_write_uart(int fd, void* buffer, int size){
	return write(fd, buffer, size);
}

int linux_fflush_uart(int fd){
	int nbytes=0;
	tcflush(fd, TCIOFLUSH);
	//get the number of available bytes
	ioctl(fd, FIONREAD, &nbytes);
	if(nbytes>0) {
		char buffer[nbytes+1];
		return read(fd, buffer, nbytes);
	}
	return 0;
}


int char_ready(int fd, int tmout){
	fd_set fds;
	struct timeval tv;
	tv.tv_sec=0;
	tv.tv_usec=tmout;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if(select(fd+1, &fds, NULL, NULL, &tv)<1) return -1;
	int ret=FD_ISSET(fd, &fds);
	return ret;
}

int linux_read_uart(int fd, char* buffer, int max_bytes, long int timeout_us){
	if (fd <= 0) {
		return(-1);
	}
	int nbytes=0;


	char end_tx_token=0;
	char tmout_flag=0;
	uint char_tmout=timeout_us;

	struct timeval t;
	gettimeofday(&t, NULL);
	//double start_time= (double)t.tv_sec + ((double)t.tv_usec)/1000000;
	ulong start=t.tv_sec*1000000+t.tv_usec;
	ulong now;

	tmout_flag=0;
	while((!tmout_flag) && (!end_tx_token) && (nbytes<max_bytes)) {
		gettimeofday(&t, NULL);
		now=t.tv_sec*1000000+t.tv_usec;
		if(now > (start + timeout_us)){
			tmout_flag=1;
		}
		if(char_ready(fd, char_tmout)>0){
			int tmp_bytes=read(fd, buffer+nbytes , (max_bytes-nbytes));
			char_tmout=10*timeout_us/max_bytes;
			if(tmp_bytes<0){
				printf( "ERROR UART, %d", tmp_bytes);
				return nbytes;
			}
			nbytes += tmp_bytes;
		}
	}

	return nbytes;
}

int linux_close_uart(int fd){
	return close(fd);
}


