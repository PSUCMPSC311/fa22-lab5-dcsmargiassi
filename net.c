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
	/* Instead of executing make command, execute the following: Due to using mdadm.o and cache.o
	# gcc -c -Wall -I. -fpic -g -fbounds-check -Werror tester.c -o tester.o
	# gcc -c -Wall -I. -fpic -g -fbounds-check -Werror util.c -o util.o
	# gcc -c -Wall -I. -fpic -g -fbounds-check -Werror net.c -o net.o
	# gcc -L. -o tester tester.o util.o mdadm.o cache.o net.o jbod.o -lcrypto
	*/
/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n (len) bytes from fd; returns true on success and false on failure. 
It may need to call the system call "read" multiple times to reach the given size len. 
*/
static bool nread(int fd, int len, uint8_t *buf) {
	int bytesread = 0; // Variable to keep track of data already read
	while(bytesread < len){ // Loop to continue reading data until finished
	int n = read(fd, &buf[bytesread], len - bytesread);// ring from buf to fd using read as offset
	if(n < 0|| n > len) { // Failure check for negative n value or read greater than length
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
	if(n < 0 || n > len) { // Failure check for negative n value or write greater than length
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
	uint8_t header[261]; // Creating temp beffer to hold header and block if needed
	int offSet = 0; // Variable to keep track of buffer offset
	if(nread(sd, HEADER_LEN, header) == false){// Reading data into array from server also checking invalid read
	return false;
	}
		/*Location of bytes in header
		# Bytes 1-4 Opcode
		# Byte 5 Info code
		# Bytes 6-261 Data block Payload
		*/
	// Use a series of Memcpy functions to copy data into proper variables from server
	memcpy(op, header + offSet, 4); // Copying the first four byes of OP Code
	offSet += 4; // Incrementing offset by 4
	*op = ntohl(*op);
	memcpy(ret, header + offSet, 1); // Coping info code into ret variable
	if(*ret == 2){
	nread(sd, 256, block); // Since data is present read the block
	//printf("\nnread Byte Value: %hhn is returning true in recvpacket function\n", ret);
	}
	return true;
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
	uint8_t header[261]; // Creating temp buffer to hold header and block if needed
	uint16_t wLength = HEADER_LEN; // Setting length to size of header len
	int offSet = 0; // Variable to keep track of memcpy offset
	int infoCode = 0; // To hold info code
	uint32_t command = op >> 12; // Based on lab 2 shifting by 12 is the location of command opcode
	op = htonl(op); // Process to convert op code into readable info
	memcpy(header, &op, 4); // Memcopy op code into header first 4 bytes
	offSet += 4; // Increase offset by 4
	if(command == 7){ // Checking to see if command is equal to jbod_write_block
	wLength = 261; // Set length to 261
	infoCode = 2; // Set info code to 2
	memcpy(header + 5, block, 256); // If write code copy block into header offset by 5
	}
	//else{
	//wlength = 5; // If not Jbod... set length to 5
	//}
	memcpy(header + offSet, &infoCode, 1); //copy info code into header byte 5
	offSet += 1; // increase offset by 1
	if(nwrite(sd, wLength, header) == true){ // Send to nwrite function to package
	//printf("\nnwrite is returning true in sendpacket function\n"); // Debug statement
	return true;
	}
	else{
	return false;
	}
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
	cli_sd = socket(AF_INET, SOCK_STREAM, 0);// Setting cli_sd variable to socket
	if(cli_sd == -1){ // Checking to see if socket connection successful
	return false; // If failed, return false
	}
	
	struct sockaddr_in caddr; // IPv4 connection
	
	// Creating connection to server
	caddr.sin_family = AF_INET; // Setting structure domain
	caddr.sin_port = htons(port); // Setting structure port 
	
	if(inet_aton((char*)ip, &caddr.sin_addr) == 0){ // Checking for failure in connection
	return false; // If fails return false
	}

	if(connect(cli_sd, (const struct sockaddr *)&caddr, sizeof(caddr)) == 0){ // Connecting socket to server
	return true; // if it is successful, return true
	}	
	else{
	return false; // Return false if it reaches end of function
	}
}
/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
	close(cli_sd); // Closing server connection 
	cli_sd = -1; // Setting client side variable back to -1
}

/* sends the JBOD operation to the server (use the send_packet function) and receives 
(use the recv_packet function) and processes the response. 

The meaning of each parameter is the same as in the original jbod_operation function. 
return: 0 means success, -1 means failure.
*/
int jbod_client_operation(uint32_t op, uint8_t *block) {
	uint8_t ret; // Variable to receive what the server receives from packet
	if(send_packet(cli_sd, op, block) == false){ // Send packet to server
	return -1; // If send packet fails return -1
	}
	if(recv_packet(cli_sd, &op, &ret, block) == false){ // Collect info from server
	return -1; // If collection fails return -1
	}	
	return 0;
}
