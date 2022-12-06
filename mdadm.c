#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache.h"
#include "jbod.h"
#include "mdadm.h"
#include "net.h"

int is_mounted = 0; // Mounted Variable Declaration
int is_written = 0; // Written Variable Declaration
int writePermission = 0; // Write permission Variable Declaration

// Declaring an unsigned 32 bit int to properly hold and organize bits
uint32_t operation(int blockSize, int diskNum, jbod_cmd_t command){ // Operation Function
// Initializing blockID variable and placing all bits in order based on bit position
 uint32_t blockID = ((command << 12)|(diskNum << 8)|(blockSize));
return blockID; // Return blockID back formatted.
}

int mdadm_mount(void) { //Return 1 for success return -1 for failure (ALL Functions)
	if (is_mounted){ // Checking to see if mounting was successful
	return -1; // If false, return -1
	}
	uint32_t op = operation(0, 0, JBOD_MOUNT);
	jbod_operation(op, NULL); // Calling device driver function
	if(jbod_client_operation(op, NULL) != 0){ // New Driver Function Checking for failure
	return -1;
	}
	is_mounted = 1;	
	return 1; // If True, return 1
	}

int mdadm_unmount(void) {
	if (!is_mounted){ // Checking to see if unmounted
	return -1; // If unmounted return -1
	}
	uint32_t op = operation(0, 0, JBOD_UNMOUNT); // Calling operation function
	jbod_operation(op, NULL); // Calling device driver function
	if(jbod_client_operation(op, NULL) != 0){ // New Driver Function Checking for failure
	return -1;
	}
	is_mounted = 0;	// Setting is_mounted variable back to 0
	return 1; // If true, return 1
 	}

int mdadm_write_permission(void){ // Setting write permission to 1
 	 writePermission = 1;		// variable to enable writing
 	 return 0;
	}

int mdadm_revoke_write_permission(void){ // Setting write permission to 0
	writePermission = 0;			// Variable to disable writing
	return 0;
	}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
	// Failure Checks
	if(!is_mounted){ // Checking to see if mounted
	return -1; // If not mounted return -1 for failure
	}
	
	if(len > 1024){//Checking read length // 2048 Old #
	return -1; // If length greater than 2048, return -1 for failure
	}
	
	if(addr + len  > 1048576){ // Checking Disk size to not exceed max size
	return -1; // If size is greater than 1 MB return -1 for failure
	} 
	
	if(buf == NULL && len != 0){ // Checking for null pointer with read length
	return -1; // If null and length > 0 return -1 for failure
	}
	// Variable Declarations
	int currAddr = addr; // Starting point to track address
	int blksRead = 0; // Tracking blocks read
	int bufOffSet = 0; // Tracking buffer offset
	while (currAddr < addr + len){ // Loop to continue until all bytes have been read
	int diskNum = currAddr / 65536; //JBOD_DISK_SIZE; // Calculating Disk Number based on curr addr
	int blkNum = (currAddr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE; // Calculating Block Number
	int blkOffSet = currAddr  % 256; // Calculating Block Offset
	uint8_t tempBuffer[JBOD_BLOCK_SIZE]; // Temporary buffer to hold information
	if(tempBuffer == NULL){ // Failure Check
	printf("TempBuffer (Read) failed to initialize"); // If tempbuffer fails to initialize
	return -1; // Then, Return -1
	}

	// Cache Lookup
	if(cache_enabled() == true && cache_lookup(diskNum, blkNum, tempBuffer) == 1){
	printf("\nRead Cache Enabled!\n");
	//int bytesLft = (addr + len) - currAddr; // Seeing how many bytes left to read
	
	// Function to see how many bytes to read per iteration
	int bytesLft = (addr + len) - currAddr; // Seeing how many bytes left to read
	if(bytesLft > 256){
	while(bytesLft > 256){ // if bytes left > 256 subtract 256 until 
	bytesLft -= 256;
	}
	bytesLft = 256 - blkOffSet; // Take 256 and subtract the block offset
	}
	if(blkOffSet + bytesLft > 256){
	bytesLft = 256 - blkOffSet;
	}

	
	memcpy(buf + bufOffSet, tempBuffer + blkOffSet, bytesLft); // Memcopy fucntion
	currAddr += JBOD_BLOCK_SIZE - blkOffSet; // Increment current address by block size
	blksRead += 1; // Updating blocks read by 1
	bufOffSet += JBOD_BLOCK_SIZE - blkOffSet; // Updating buffer offset by amount of bytes read
	} 
	else{
	
		
	// JBOD operation functions to get disk, block, read info place into temp buffer
	jbod_operation(operation(0, diskNum, JBOD_SEEK_TO_DISK), NULL); // Operation TO SEEK DISK
	jbod_operation(operation(blkNum, 0, JBOD_SEEK_TO_BLOCK), NULL);//Operation to seek block
	int ops = jbod_operation(operation(0, 0, JBOD_READ_BLOCK), tempBuffer);// Operation to read block
	jbod_client_operation(ops, tempBuffer); // New Client operation ***********************************************
	
		/*// Debug Print statements
		printf("\nRead Function");
		printf("\ndisk num %d\n", diskNum);
		printf("blkNum %d\n", blkNum);
		printf("blkOffSet %d\n", blkOffSet);
		*/
		
	int bytesLft = (addr + len) - currAddr; // Seeing how many bytes left to read
	
	memcpy(buf + bufOffSet, tempBuffer + blkOffSet, bytesLft); // Memcopy fucntion
	currAddr += JBOD_BLOCK_SIZE - blkOffSet; // Increment current address by block size
	blksRead += 1; // Updating blocks read by 1
	bufOffSet += JBOD_BLOCK_SIZE - blkOffSet; // Updating buffer offset by amount of bytes read
	
	//cache_insert(diskNum, blkNum, tempBuffer); NOT NEEDED ??
	}
	}
	return len; // Returning read length parameter
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
	// Write permission functions
	int mdadm_write_permission(){
	jbod_operation(operation(0, 0, JBOD_WRITE_PERMISSION), NULL);
	return 0;
	}
	
	int mdadm_revoke_write_permission(){
	jbod_operation(operation(0, 0, JBOD_REVOKE_WRITE_PERMISSION), NULL);
	return -1;
	}
	
	// Failure Checks
	if(!is_mounted){ // Checking to see if mounted
	return -1; // If not mounted return -1 for failure
	}
	
	if(len > 1024){//Checking read length // 2048 Old #
	return -1; // If length greater than 2048, return -1 for failure
	}
	
	if(addr + len  > 1048576){ // Checking Disk size to not exceed max size
	return -1; // If size is greater than 1 MB return -1 for failure
	} 
	
	if(buf == NULL && len != 0){ // Checking for null pointer with read length
	return -1; // If null and length > 0 return -1 for failure
	}
	
	// Variable Declarations
	int currAddr = addr; // Starting point to track address
	int blksWrote = 0; // Tracking blocks wrote
	int bufOffSet = 0; // Tracking buffer offset
	mdadm_write_permission(); // Calling function to give write permissions.
	if(writePermission == 1){ // Checking for writing permissions
	while (currAddr < addr + len){ // Loop to continue until all bytes have been wrote and confirming write permission
	int diskNum = currAddr / JBOD_DISK_SIZE;  // Calculating Disk Number based on curr addr
	int blkNum = (currAddr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE; // Calculating Block Number
	int blkOffSet = currAddr  % JBOD_BLOCK_SIZE; // Calculating Block Offset
	uint8_t tempBuffer[JBOD_BLOCK_SIZE]; // Temporary buffer to hold information 
	
	// Function to see how many bytes to read per iteration
	int bytesLft = (addr + len) - currAddr; // Seeing how many bytes left to read
	if(bytesLft > 256){
	while(bytesLft > 256){ // if bytes left > 256 subtract 256 until 
	bytesLft -= 256;
	}
	bytesLft = 256 - blkOffSet; // Take 256 and subtract the block offset
	}
	if(blkOffSet + bytesLft > 256){
	bytesLft = 256 - blkOffSet;
	}
	
	if(cache_enabled() == true && cache_lookup(diskNum, blkNum, tempBuffer) == 1){
		memcpy(tempBuffer + blkOffSet, buf + bufOffSet, bytesLft);
		jbod_operation(operation(0, diskNum, JBOD_SEEK_TO_DISK), NULL); // Operation TO SEEK DISK
		jbod_operation(operation(blkNum, 0, JBOD_SEEK_TO_BLOCK), NULL); // Operation to seek block
		int ops = jbod_operation(operation(0, 0, JBOD_WRITE_BLOCK), tempBuffer); // Operation to Write block
		jbod_client_operation(ops, tempBuffer);// New Client Operation ***********************
		currAddr += JBOD_BLOCK_SIZE - blkOffSet; // Increment current address by block size
		blksWrote += 1; // Increment blks read by 1
		bufOffSet += JBOD_BLOCK_SIZE - blkOffSet; // Updating buffer offset by amount of bytes read
		cache_update(diskNum, blkNum, tempBuffer);
	}
	else{
	//JBOD operations to seek disk/block read block and place in TempReadBuffer
	jbod_operation(operation(0, diskNum, JBOD_SEEK_TO_DISK), NULL); // Operation TO SEEK DISK
	jbod_operation(operation(blkNum, 0, JBOD_SEEK_TO_BLOCK), NULL);//Operation to seek block
	int ops = jbod_operation(operation(0, 0, JBOD_READ_BLOCK), tempBuffer);// Operation to Write block
	jbod_client_operation(ops, tempBuffer);// New Client Operation ***********************
	memcpy(tempBuffer + blkOffSet, buf + bufOffSet, bytesLft);
	jbod_operation(operation(0, diskNum, JBOD_SEEK_TO_DISK), NULL); // Operation TO SEEK DISK
	jbod_operation(operation(blkNum, 0, JBOD_SEEK_TO_BLOCK), NULL); // Operation to seek block
	jbod_operation(operation(0, 0, JBOD_WRITE_BLOCK), tempBuffer); // Operation to Write block
//Delete?	//jbod_client_operation(ops, tempBuffer);// New Client Operation ***********************
	
/*	
		// Debug Print statements
		printf("\nWrite Function");
		printf("\ndisk num %d\n", diskNum);
		printf("blkNum %d\n", blkNum);
		printf("blkOffSet %d\n", blkOffSet);
		printf("Current Address: %d\n", currAddr);
		printf("buffer Offset: %d\n", bufOffSet);
*/		
	currAddr += JBOD_BLOCK_SIZE - blkOffSet; // Increment current address by block size
	blksWrote += 1; // Increment blks read by 1
	bufOffSet += JBOD_BLOCK_SIZE - blkOffSet; // Updating buffer offset by amount of bytes read
	cache_insert(diskNum, blkNum, tempBuffer);
	}
	}	
}
else{
printf("No write Permissions");
return -1;
}
mdadm_revoke_write_permission();
return len;
}
