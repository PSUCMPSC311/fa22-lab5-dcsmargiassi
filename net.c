#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n (len) bytes from fd; returns true on success and false on failure. 
It may need to call the system call "read" multiple times to reach the given size len. 
*/
static bool nread(int fd, int len, uint8_t *buf) {
	int bytesread = 0; // Variable to keep track of data already read
	while(bytesread < len){ // Loop to continue reading data until finished
	int n = read(fd, &buf[bytesread], len - bytesread);// ring from buf to fd using read as offset
	if(n < 0) { // Failure check for negative n value
	return false; // Return false if n < 0
	}
	bytesread += n; // Incrementing read varable by n bytes read
	}
  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on failure 
It may need to call the system call "write" multiple times to reach the size len.
*/
static bool nwrite(int fd, int len, uint8_t *buf) {
	int byteswrote = 0; // Variable to keep track of data already wrote
	while(byteswrote < len){ // Loop to continue writing data until finished
	int n = write(fd, &buf[byteswrote], len - byteswrote); // writing from buf to fd using wrote as offset
	if(n < 0) { // Failure check for negative n value
	return false; // Return false if n < 0 
	}
	byteswrote += n; // Incrementing wrote variable by n bytes wrote
	}
  return true;
}

/* Through this function call the client attempts to receive a packet from sd 
(i.e., receiving a response from the server.). It happens after the client previously 
forwarded a jbod operation call via a request message to the server.  
It returns true on success and false on failure. 
The values of the parameters (including op, ret, block) will be returned to the caller of this function: 

op - the address to store the jbod "opcode"  
ret - the address to store the info code (lowest bit represents the return value of the server side calling the corresponding jbod_operation function. 2nd lowest bit represent whether data block exists after HEADER_LEN.)
block - holds the received block content if existing (e.g., when the op command is JBOD_READ_BLOCK)

In your implementation, you can read the packet header first (i.e., read HEADER_LEN bytes first), 
and then use the length field in the header to determine whether it is needed to read 
a block of data from the server. You may use the above nread function here.  
*/
static bool recv_packet(int sd, uint32_t *op, uint8_t *ret, uint8_t *block) {
	uint8_t header[HEADER_LEN]; // Array to store the received array variable "sd"
	int offSet = 0; // Variable to keep track of buffer offset
	uint8_t byte = 0; // Variable to keep track of byte number
	//uint16_t len = 0;
	//byte = ret; // Check casting of this variable
	if(nread(sd, HEADER_LEN, header) == false){ // Reading data into array from server also checking invalid read
	return false;
	}
	//Bytes 1-4 Opcode
	//Byte 5 Info code
	//Bytes 6-261 Data block Payload
	
	
	// Use a series of Memcpy functions to copy data into proper variables from server
	memcpy(op, header + offSet, sizeof(*op)); // Copying the first four byes of OP Code
	offSet += sizeof(*op); // Incrementing offset by 4 base
	*op = htonl(*op);
	memcpy(ret, header + offSet, sizeof(*ret)); // Coping info code into ret variable
//	*ret = ntohs(*ret); // Converting bytes NOT NEEDED
	
	/* Info Code Meaning 4 different cases Note: Each '#' represents a different case
	# 00000000 = JBOD operation returns 0 && datablock not present return -1 byte = 0
	# 00000001 = JBOD operation returns -1 && datablock no present also returns -1 byte = 1
	# 00000010 = JBOD operation returns 0  && datablock is present also returns 0 byte = 2
	# 00000011 = JBOD operation returns -1 && datablock is present returns 0 byte = 3
	*/ // This result gets returned to JBOD_connect and states whether successful or not
	// ret at this point already has the assginment of 1 byte
	// Method to read specific bit in ret variable
	//byte = (ret >> ); // Shifting bits Using &1 operator to mask other bits
	//if(byte == 0){ // Case 1
	//return false;
	//}
	//if(byte == 1){ // Case 2
	//return false;
	//}
	
	
	
	byte = ret;
	byte &= 2;
	if(byte == 2){ // Case 3//replaced byte variable
	nread(sd, 256, block); // Since data is present read the block
	return true;
	}
	//if(byte == 3){ // Case 4
	//return false;
	//}

	return false;
}



/* The client attempts to send a jbod request packet to sd (i.e., the server socket here); 
returns true on success and false on failure. 

op - the opcode. 
block- when the command is JBOD_WRITE_BLOCK, the block will contain data to write to the server jbod system;
otherwise it is NULL.

The above information (when applicable) has to be wrapped into a jbod request packet (format specified in readme).
You may call the above nwrite function to do the actual sending.  
*/
static bool send_packet(int sd, uint32_t op, uint8_t *block) {
	uint8_t header[264];
	uint16_t length = HEADER_LEN; // Setting length to size of header len
	uint16_t ops = op >> 12; // Shifting 12 blocks over to read 
	if(block != NULL){ // Checking to see fi block is passed as NULL
	length += JBOD_BLOCK_SIZE; // If not NULL, increase length by JBOD Block size
	}
	
	length = htons(length);
	ops = htonl(op);
	
	
	
	memcpy(header, &length, sizeof(length));
	memcpy(header + (sizeof(length)), &ops, sizeof(ops));
	if(ops == JBOD_WRITE_BLOCK){
	memcpy(header + HEADER_LEN, block, JBOD_BLOCK_SIZE);
	}
	if(nwrite(sd, length, header) == true){
	return true;
	}
	return false;
}



/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. 
 * this function will be invoked by tester to connect to the server at given ip and port.
 * you will not call it in mdadm.c
*/
bool jbod_connect(const char *ip, uint16_t port) {
	/* Five Steps to connect client to server
	# Figure out Address/Port to connect
	# Create a socket
	# Connect the socket to the remote server
	# read() and write() data using the socket
	# Close the socket
	*/
	// Variable Declarations
	cli_sd = socket(AF_INET, SOCK_STREAM, 0);// Setting cli_sd variable to socket
	if(cli_sd == -1){ // Checking to see if socket connection successful
	return false; // If failed, return false
	}
	struct sockaddr_in caddr; // IPv4 connection
	
	// Creating connect to server
	caddr.sin_family = AF_INET;
	caddr.sin_port = htons(port);
	
	if(inet_aton(ip, &caddr.sin_addr) == 0){ // First way slide 23
	return false;
	}

	if(connect(cli_sd, (const struct sockaddr *)&caddr, sizeof(caddr)) == 0){
	return true; 
	}
	//cli_sd = 1;
	return false; // Return false if it reaches end of function

}




/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
	close(cli_sd); // Checking to see if client side variable is already false
	cli_sd = -1; // Setting client side variable back to -1
}



/* sends the JBOD operation to the server (use the send_packet function) and receives 
(use the recv_packet function) and processes the response. 

The meaning of each parameter is the same as in the original jbod_operation function. 
return: 0 means success, -1 means failure.
*/
int jbod_client_operation(uint32_t op, uint8_t *block) {
	uint8_t ret; // Variable to return what the server receives from packet
	//if((cli_sd = jbod_connect(JBOD_SERVER, JBOD_PORT)) == -1){ // Connecting to client
	//return -1; // If connection to client fails return -1
	//}
	
	if(send_packet(cli_sd, op, block) == false){ // Send packet to server
	return -1; // If send packet fails return -1
	}
	
	if(recv_packet(cli_sd, &op, &ret, block) == false){ // Collect info from server
	return -1; // If collection fails return -1
	}
	//while(cli_sd != -1){
	//jbod_disconnect(); // Disconnecting from server
	// If disconnection fails, Attempt again
	//}	
	return ret;
}
