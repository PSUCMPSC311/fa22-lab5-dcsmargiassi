#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "cache.h"
#include "jbod.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int num_queries = 0;
static int num_hits = 0;
	int i = 0; // counter for inserts into cache
	int entries = 0; // Tracking number of cache entries
	int LFU = 0; // Least frequently used variable
int cache_create(int num_entries) {
// Failure Checks
	if(num_entries < 2){ // Checking for a size smaller than 2
	return -1;
	}
	if(num_entries > 4096){ // Checking for a size larger than 4096
	return -1;
	}
	if(cache_size != 0){
	return -1;
	}
	// Creating the Cache
	//cache = calloc(num_entries, sizeof(cache_entry_t));
	cache = malloc(num_entries*sizeof(cache_entry_t));
	
	if(cache == NULL){ // Checking to see if cache Initialized 
	printf("\nCache failed to initialize!\n");
	return -1;
	}
	
	cache_size = num_entries; // Setting cache size to number of entries
	num_hits = 0; // Initalizing number of hits to zero 
	num_queries = 0; // Initializing number of queries to zero
	i = 0; // Resetting I counter for insert function
	for(int l = 0; l < num_entries; l++){// Initializing all valid status to false
	cache[l].valid = false;
	}
	return 1;
}

int cache_destroy(void) {
	if(cache == NULL){
	return -1;
	}
	free(cache); // Freeing cache memory
	cache = NULL; // Setting cache pointer to NULL
	cache_size = 0; // Setting cache size back to zero.
    return 1;
}
int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
	int l = 0;
	bool found = false;
	if(cache_size == 0 || cache_size < 0){ // Checking to see if cache is empty or a negative number
	return -1;
	}
	if(entries == 0){ // Checking to see if any entries into cache
	return -1;
	}
	if(buf == NULL){ // Checking to see if buffer is NULL
	return -1;
	}
	// Loop to see if there is a matching disk and block number
	for(l = 0; l < cache_size; l++){
	if(cache[l].disk_num == disk_num && cache[l].block_num == block_num){
	found = true;
	}
	}
	if(found == false){ // Checking to see if element is in cache
	return -1;
	}
	l = 0; // Setting l back to zero
	while(l < cache_size){
	if((cache[l].disk_num == disk_num) && (cache[l].block_num == block_num) && cache[l].valid == true){
	memcpy(buf, cache[l].block, 256);
	cache[l].num_accesses++; // Increment number of accesses by 1	
	num_queries+= 1; // Increment number of queries by 1
	num_hits++; // Increment number of hits by 1
	l++; // increment counter
	break; // break loop if found
	}
	else{
	l++; // Increment counter
	num_queries+= 1;// Increment number of queries by 1
	}
	}
	return 1;
	}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
	int k = 0;
	while(k < cache_size){
	if((cache[k].disk_num = disk_num) && (cache[k].block_num == block_num) && (cache[k].valid == true)){
	cache[k].num_accesses++; // Increment number of accesses by 1
	memcpy(cache[k].block, buf, 256); // Copy block content into buffer
	break; // Break loop if element found
	}
	else{
	k++;
	}
	}
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
	int j = 1;
	if(cache == NULL){ // Checking to see if Cache is NULL
	return -1; // If Yes, return -1 as not able to insert
	}
	if(buf == NULL){ // Checking to see if buffer is NULL
	return -1;
	}
	if(disk_num > JBOD_NUM_DISKS || disk_num < 0){ // Checking for valid parameters on disk num
	return -1;
	}
	if(block_num > JBOD_NUM_BLOCKS_PER_DISK || block_num < 0){ // Checking for valid parameters on block num
	return -1;
	}
	int k = 0;
	while(k < cache_size){ // Loop to see if element is already in cache
	if(cache[k].disk_num == disk_num && cache[k].block_num == block_num && cache[k].valid == true){ // Comparing block/disk num & checking valid
	return -1;
	}
	else{
	k++; // If not found, increment loop counter
	}
	}
	
	while((cache+i) != NULL && i < cache_size){ // First Cache entries go through this loop
	cache[i].disk_num = disk_num; // Setting cache disk num
	cache[i].block_num = block_num; // Setting cache block num
	cache[i].valid = true; // Setting cache element to valid
	cache[i].num_accesses = 1; // Setting number of accesses to 1
	memcpy(cache[i].block, buf, 256); // Copy buffer into block
	entries++; // Increment entries by 1
	i++; // Increment Counter
	return 1;
	} 
	if(i == cache_size){ // If number of entries = cache_size, Remove the LFU Element
	while(j < cache_size){
	if(cache[LFU].num_accesses > cache[j].num_accesses){ // Loop to find LFU Element
	LFU = j; // If found, set equal to LFU
	j++; // Increment J by 1
	}
	else if(cache[LFU].num_accesses <= cache[j].num_accesses){ // If LFU Not found keep searching
	j++; // Increment J by 1
	}
	}
	cache[LFU].disk_num = disk_num; // Setting cache disk num
	cache[LFU].block_num = block_num; // Setting cache block num
	cache[LFU].valid = true; // Setting cache element to valid
	cache[LFU].num_accesses = 1; // Setting number of accesses to 1
	memcpy(cache[LFU].block, buf, 256); // Copy buffer into block
	}
  return 1;
}

bool cache_enabled(void) {
	return cache != NULL && cache_size > 0;
}

void cache_print_hit_rate(void) {
	fprintf(stderr, "num_hits: %d, num_queries: %d\n", num_hits, num_queries);
	fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}
