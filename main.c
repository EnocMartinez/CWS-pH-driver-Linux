/*
 * CWS10101 driver in C with for Linux. This code uses the costof_simulator software layer
 * which tries to emulate the Costof2 behavior
 *
 *  @author: Enoc Martínez
 *  @institution: Universitat Politècnica de Catalunya (UPC)
 *  @contact: enoc.martinez@upc.edu
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "costof_simulator.h"


typedef enum  {  // Operational states of the sensor
	UNKNOWN = 0,
	IDLE = 1,
	OPERATING = 2,
	SLEEPING = 3
}cws_state;

char* cws_states_str[] = {"UNKNOWN", "IDLE", "OPERATING", "SLEEPING"};

// Internal functions
int cws_get_prompt(LibSensor* self);
int cws_sleep(int msecs);
int cws_send_command(LibSensor* self, char* cmd, int prompt);
int cws_get_response(LibSensor* self, char* response, int respsize, int timeoutMs);
int cws_get_state(LibSensor *self, cws_state* state);
int cws_wait_until_state(LibSensor* self, cws_state target_state, int timeoutMs);
int cws_get_sample(LibSensor* self );


// Global functions
int sensor_init(LibSensor *self);
int sensor_measure(LibSensor *self);


//#define SIMULATE_RESPONSE  // if set, the driver will simulate a response instead of waiting for the sensor
//#define CWS_DEBUG_COMMS

#define CHLORINATOR_TIME_SECS 5
#define RISING_MODE_TIME_SECS 5
#define CWS_MEAS_TIMEOUT_MIN 20 // 20 minutes

#define PROMPT 1 // Wait for prompt
#define NO_PROMPT 0 // Don't wait for the prompt


/*
 * ==================================================================
 *                          Utils
 * ------------------------------------------------------------------
 * Useful functions not related to CWS10101, but thay may be useful
 * ==================================================================
 */


/*
 * This function gets the substrings delimited by the string token. A pointer
 * to an string array is returned. The number of strings found is stored in
 * the nStrings pointer. This function modifies the 'buffer' string, setting
 * to '0' the token substrings. The substrings returned are pointing at the
 * buffer memory, so freeing the buffer will automatically erase all the
 * substrings.
 *
 * example
 *  buffer* "string1---string2"
 *  token* "---"
 *
 *  after:
 *  buffer* "string1\0\0\0strings2"
 *    char[0]^            ^
 *                 char[1]^
 *
 *  char*[0]="string1"        (pointer to buffer[0])
 *  char*[1] "string2"        (pointer to buffer[10])
 *
 */
char** cws_get_substrings(char* buffer, const char* token, int* nStrings){
	if(buffer==NULL || token==NULL || nStrings==NULL){
		speLOG(LOG_ERR, "cws_get_substrings: NULL pointer received");
		return NULL;
	}
	uint sLen=strlen(buffer);
	uint tLen=strlen(token);

	uint strpos[sLen];
	uint nstrs;
	char** strings=NULL;
	uint i;

	i=0;
	nstrs=1;
	strpos[0]=0; //there's at least 1 string
	while(i<(sLen-tLen)){
		if(!memcmp(&buffer[i], token, tLen)){ //found token
			void* point=&buffer[i];
			memset(point, 0, tLen); //erase the separator

			i+=tLen;
			if(i<sLen){ //check that the function still is inside the string
				strpos[nstrs]=i;
				nstrs++;
			}
		} else{
			i++;
		}
	}
	strings=fastMalloc((nstrs+1)*sizeof(char*)); //store one more element, to end the array with NULL
	strings=memset(strings, 0, (nstrs+1)*sizeof(char*));
	for(i=0; i<nstrs; i++){
		strings[i]=&buffer[strpos[i]];
	}
	*nStrings=nstrs;
	return strings;
}



/*
 * Executes a function and returns error if the return value is negative.
 * The second argument is the error message to be displayed in case of failure
 */
#define TRY_CATCH(func, errmsg) ({ \
	int __temp_return = func;\
	if (__temp_return < 0) { \
		speLOG(LOG_ERR, "Caught error at %s, line %d",  __FILE__, __LINE__);\
		if (strlen(errmsg) > 0) { \
			speLOG(LOG_ERR, errmsg);\
		}  \
		return __temp_return; \
	}\
	__temp_return; \
})


/*
 * Similar to TRY_CATCH but tries the same command several times with a delay between tries
 * func: function
 * tries: tries
 * delayMs: delay (in Ms) between tries)
 * errmsg: error message to display
 */
#define RETRIES(func, tries, delayMs, errmsg) ({ \
		int __tries = tries; \
		int __ret; \
		while (__tries--) { \
			__ret = func; \
			if (__ret >= 0 ) { \
				break;\
			} \
			if (strlen(errmsg) > 0) { \
				speLOG(LOG_ERR, errmsg);\
			}  \
			cws_sleep(delayMs); \
		} \
	if (__tries == 0){ \
		return __ret; \
	} \
	__ret; \
})



/*
 * ==================================================================
 *                          MAIN
 * ------------------------------------------------------------------
 * Main program that simulates costof2 behavior
 * ==================================================================
 */

int main() {
	speLOG(LOG_INFO, "=== Start CWS 10101 Driver ===");
	char device[256] = "/dev/ttyUSB0";
	int baudrate = 9600;

	LibSensor self;
	self.fd = les_open_serial_port(device, baudrate);
	TRY_CATCH(sensor_init(&self), "ERROR Could not initialize sensor!");
	TRY_CATCH(sensor_measure(&self), "ERROR, could not get measure!");
	return 0;
}


/*
 * ==================================================================
 *                     CWS10101 Common functions
 * ------------------------------------------------------------------
 * Common functions to abstract the operation of the CWS10101 sensor
 * including get prompt, and send commands and parse response.
 * ==================================================================
 */


/*
 * Tries to get the sensor prompt "WETCHEM>"
 * If after 3 opportunities it fails, an error is thrown
 * returns 1 on exit -1 on failure
 *
 */
int cws_get_prompt(LibSensor* self){
	char buff[256];
	int indx = 0;
	int n = 0;
	char prompt[20] =  "WETCHEM>";
	int prompt_length =  strlen("WETCHEM>");

	memset(buff, 0, 256);
	n = les_read(self->fd, 200, &buff[indx], 200);
	indx += n;

#ifdef CWS_DEBUG_COMMS
	/*
	char temp[50];
	temp[prompt_length] = 0;
	speLOG(LOG_DETAIL, "buff: [%s]", buff);
	memcpy(temp, &buff[indx - prompt_length], prompt_length);
	speLOG(LOG_DETAIL, "    comparing buff: '%s'", prompt);
	speLOG(LOG_DETAIL, "    with prompt   : '%s'", &buff[indx - prompt_length]);
	*/
#endif
	//Compare last bytes of the buffer, prompt should be at the end
	if (!memcmp(prompt, &buff[indx - prompt_length], prompt_length)) {
#ifdef CWS_DEBUG_COMMS
	//speLOG(LOG_DETAIL, "PROMPT FOUND!!!");
#endif
		return 0;
	}

	return -1;
}


/*
 * Wrapper for sleep
 */
int cws_sleep(int msecs) {
	return usleep(1000*msecs);
}


/*
 * Gets the sensor current state by using the command
 *  self: LibSensor
 *  cmd: command to send (without \r\n)
 *  prompt: if > 0 after sending the command we will wait for the prompt
 */

int cws_send_command(LibSensor* self, char* cmd, int prompt) {
	int r;
	char buff[strlen(cmd) + 4];
	sprintf(buff, "%s\r\n", cmd);
	r = les_writeLine(self->fd, 200, buff);
#ifdef CWS_DEBUG_COMMS
	speLOG(LOG_DETAIL, "   TX [%s]", cmd);
#endif

	if (prompt) {
		RETRIES(cws_get_prompt(self), 5, 1000, "");
	}

	return r;
}

/*
 * Reads the response of the sensor, until a new prompt is found. Return the string until the prompt
 */
int cws_get_response(LibSensor* self, char* response, int respsize, int timeoutMs) {

	char buff[respsize];
	int indx = 0;
	int trials = 3;
	int n = 0;
	char prompt[20] =  "WETCHEM>";
	int prompt_length =  strlen("WETCHEM>");
	memset(buff, 0, respsize);

	while (trials-- > 0) {
		n = les_read(self->fd, timeoutMs, &buff[indx], respsize);
		indx += n;

		if (indx < prompt_length) {
			//speLOG(LOG_DEBUG, "   read %d bytes, but no prompt yet [%s]", indx, buff);
		}
		// Compare last bytes of the buffer, prompt should be at the end
		else if (!memcmp(prompt, &buff[indx - prompt_length], prompt_length)) {
			//speLOG(LOG_DEBUG,"showing buffer (n=%d):\n", indx);

			int length = indx-prompt_length;
			if (length < respsize) {
				length = respsize;
			}

			memcpy(response, &buff, indx - prompt_length);

			// in case it ends with \r\n shorten it to erase the newlines
			if (response[indx - prompt_length - 2] == '\r') {
				response[indx - prompt_length - 2] = 0;
			}

			return indx;
		}
		cws_sleep(timeoutMs);
	}

	if (trials == 0){
		return -1;
	}

#ifdef CWS_DEBUG_COMMS
	speLOG(LOG_DETAIL, "   RX [%s]", response);
#endif

	return strlen(response);
}




int cws_get_state(LibSensor *self, cws_state* state){
	char resp[256];
	char *state_str;
	int nbytes;

	char **splits;
	int nsplits;

	*state = UNKNOWN;

	memset(resp, 0, 256);
	les_resetRxFifo(self->fd);

	cws_send_command(self, "GETSTATUS", NO_PROMPT);
	nbytes = TRY_CATCH(cws_get_response(self, resp, 256, 2000), "Could not get response");

	if (nbytes < 1 ){
		speLOG(LOG_ERR, "empty buffer");
		return -1;
	}

	// right now in response we should have something like:
	// "CWS10101,4,1691166748,1691166748,11.5,27.8,IDLE,0"
	// the field of interest right now is the field 7


	// WARNING: cws_get_substrings allocates memory!
	splits = cws_get_substrings(resp, ",", &nsplits);

	if (nsplits != 8) {
		speLOG(LOG_ERR, "Could not parse response! expcted 8 fields, got %d", nsplits);
		return -1;
	}
	state_str = splits[6];  // The state should be the 6th string (starting from 0)

	if (!strcmp(state_str, "OPERATING")) {
		*state = OPERATING;
	}
	else if (!strcmp(state_str, "IDLE")) {
		*state = IDLE;
	}
	else if (!strcmp(state_str, "SLEEPING")) {
		*state = SLEEPING;
	} else {
		speLOG(LOG_ERR, "Unrecognized CWS state '%s'", state_str);
		fastFree(splits);
		*state = UNKNOWN;
		return -1;
	}

	fastFree(splits);
	return 0;
}

/*
 * Waits until the sensor reached the desired state or until timeout expires.
 * If Timeout expires, return -1, otherwise 0.
 */
int cws_wait_until_state(LibSensor* self, cws_state target_state, int timeoutMs) {
	// TODO apply the timeout!
	cws_state s = UNKNOWN;
	long int time = 0;
	int ret;
	while (s != target_state) {
		ret = cws_get_state(self, &s);
		if (ret < 0) {
			speLOG(LOG_DEBUG, "Can't get state!");
			return -1;
		}
		if (s != target_state) {
			cws_sleep(1000);
			time += 1000;
			if (time > timeoutMs) {
				speLOG(LOG_ERR,"Timeout error!");
				return -1;
			}
		}
	}
	return 0;
}



/*
 * Gets a sample from the sensor. The sample frame looks like:
 * 'CWS10101,4,1691166748,8.123,20.0,0.1234,1.1234,2.1234,11.5,27.8'
 *
 * where:
 * 0-> CWS + serial number
 * 1-> sensor type (ph=4, nitrate=1, ....)
 * 2->epoch timestamp
 * 3->SAMPLE VALUE
 * 4-> validity code 0=invalid 1=aparrently good
 * 5->param1 (unspecied for pH)
 * 6-> param ("r"?????)
 * 7-> therminstor t (water T?)
 * 8->supply voltage
 * 9->internal temp
 *
 */
int cws_get_sample(LibSensor* self ){
	char buff[256];
	char **strings;
	int nstrings;
	strings = fastMalloc(30*sizeof(char*));
	speLOG(LOG_INFO, "getting sample...");

	TRY_CATCH(cws_send_command(self, "GETSAMPLE", NO_PROMPT), "could not send getsample command");



#ifdef SIMULATE_RESPONSE
	// TODO: Forcing response!!!
	strcpy(buff, "CWS10101,4,1691166748,8.123,20.0,0.1234,1.1234,2.1234,11.5,27.8");
#else
	RETRIES(cws_get_response(self, buff, 256, 2000), 3, 1000, "Could not get response");
#endif

	// WARNING: cws_get_substrings allocates memory
	strings = cws_get_substrings(buff, ",", &nstrings);

	if (nstrings != 10 ) {
		speLOG(LOG_ERR, "Expected 10 fields, got %d", nstrings);
		fastFree(strings);
		return -1;
	}

	speLOG(LOG_INFO, "pH %s", strings[3]);
	speLOG(LOG_INFO, "validity %s", strings[4]);
	speLOG(LOG_INFO, "supply voltage %s V", strings[8]);
	speLOG(LOG_INFO, "internal temp %s ºC", strings[9]);


	/* I guess here we should put something like
	 *  do {
			tmp=(uint32_t)measMeanValueTab[i];
			lsd_meas_set(self->sensor_data, &tmp, NULL);
			i++;
		} while (lsd_channel_next(self->sensor_data) > 0);
	 */

	return 0;
}



/*----------------------------------------------------------------
 *		Sensor Init, measure, power
 *----------------------------------------------------------------*/

#define PROMPT_TRIES 4
/*
 * Initializes the sensor
 */
int sensor_init(LibSensor *self) {
	//int tries = PROMPT_TRIES;
	cws_state state = UNKNOWN;

	TRY_CATCH(cws_send_command(self, "", PROMPT), "CWS 10101 Init failed!");
	cws_sleep(1000);
	TRY_CATCH(cws_send_command(self, "STOP", PROMPT), "could not send STOP");
	cws_sleep(1000);
	TRY_CATCH(cws_get_state(self, &state), "could not get state");
	speLOG(LOG_DEBUG, "Current status %s", cws_states_str[state]);

	speLOG(LOG_INFO, "CWS 10101 Initialized");
	return 0;
}


/*
 * Perform a measure
 */
int sensor_measure(LibSensor *self) {
	speLOG(LOG_INFO, "Starting sensor measure");
	TRY_CATCH(cws_send_command(self, "START", PROMPT), "could not send command START");

	speLOG(LOG_INFO, "Waiting until IDLE state (timeout %d minutes)", CWS_MEAS_TIMEOUT_MIN);

#ifdef SIMULATE_RESPONSE
	cws_sleep(1000);
	cws_wait_until_state(self, IDLE, 1000);
	speLOG(LOG_WARNING, "HEADSUP!-> SIMULATING RESPONSE!!");
	TRY_CATCH(cws_send_command(self, "STOP", PROMPT), "error in STOP");

#else
	// Simulator
	TRY_CATCH(cws_wait_until_state(self, IDLE, CWS_MEAS_TIMEOUT_MIN*60*1000), "Sensor not going to SLEEP state, aborting measure");
#endif

	TRY_CATCH(cws_get_sample(self), "Sensor not going to IDLE state, aborting measure");


	// TODO Start chlorinator here!
	TRY_CATCH(cws_send_command(self, "SPECIAL1", PROMPT), "failed to send special1");
	TRY_CATCH(cws_wait_until_state(self, OPERATING, 20000), "Timeout");

	// TODO adjust waiting time
	speLOG(LOG_DEBUG, "Applying chlorinator for %d secs", CHLORINATOR_TIME_SECS);
	cws_sleep(1000*CHLORINATOR_TIME_SECS);


	// TODO stop chlorinator here!
	speLOG(LOG_DEBUG, "stopping chlorination");
	TRY_CATCH(cws_send_command(self, "STOP", PROMPT), "failed to send stop");
	TRY_CATCH(cws_wait_until_state(self, IDLE, 20000), "Sensor not going to IDLE state, aborting measure");

	// TODO start rising mode
	TRY_CATCH(cws_send_command(self, "SPECIAL2", PROMPT), "failed to send SPECIAL2");
	TRY_CATCH(cws_wait_until_state(self, OPERATING, 20000), "Timeout");


	// TODO adjust waiting time
	speLOG(LOG_DEBUG, "Applyling Rising Mode for Waiting %d secs", RISING_MODE_TIME_SECS);
	cws_sleep(1000*RISING_MODE_TIME_SECS);
	speLOG(LOG_DEBUG, "stopping Rise mode");
	TRY_CATCH(cws_send_command(self, "STOP", PROMPT), "failed to send stop");
	TRY_CATCH(cws_wait_until_state(self, IDLE, 20000), "Sensor not going to IDLE state, aborting measure");

	return 0;
}
