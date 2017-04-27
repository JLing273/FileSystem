////////////////////////////////////////////////////////////////////////////////
//
//  File           : cart_cache.c
//  Description    : This is the implementation of the cache for the CART
//                   driver.
//
//  Author         : [** Jason Ling **]
//  Last Modified  : [** November 25, 2016**]
//

// Includes
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Project includes
#include <cmpsc311_log.h>
#include <cart_controller.h>
#include <cart_cache.h>

// Defines
typedef int bool;
#define true 1
#define false 0

//cache frames
struct CacheFrames {
	int time;
	CartFrameIndex frame_num;
	CartFrameIndex cart_num;
	char framebuf[1024];
	bool mark; //used for unit test only
};

//main cache structure
typedef struct Cache {
	int size;
	int num_occupied;
	bool open;
	struct CacheFrames frames[DEFAULT_CART_FRAME_CACHE_SIZE];
} Cache;

//global time variable for LRU eviction
uint64_t global_time = 0;
//declare cache structure and initialze size
struct Cache cache_structure = {DEFAULT_CART_FRAME_CACHE_SIZE};

//
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : create_cache_opcode
// Description  : Used to get the return value of the opcode
//
// Inputs       : 64_bit opcode
// Outputs      : 0 if successful

CartXferRegister create_cache_opcode(int64_t ky1, int64_t ky2, int64_t rt1, int64_t ct1, int64_t fm1) {
	CartXferRegister opcode = 0;
	CartXferRegister temp_ky1 = (ky1 & 0xff) << 56;
	CartXferRegister temp_ky2 = (ky2 & 0xff) << 48;
	CartXferRegister temp_rt1 = (rt1 & 0xf) << 47;
	CartXferRegister temp_ct1 = (ct1 & 0xffff) << 31;
	CartXferRegister temp_fm1 = (fm1 & 0xffff) << 15; 

	opcode = temp_ky1 | temp_ky2 | temp_rt1 | temp_ct1 | temp_fm1;

	return opcode;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : extract_cache_opcode
// Description  : Used to extract the opcode
//
// Inputs       : 64_bit opcode
// Outputs      : 0 if successful

int extract_cache_opcode(CartXferRegister resp, CartXferRegister *ky1, CartXferRegister *ky2, CartXferRegister *rt1, 
						CartXferRegister *ct1, CartXferRegister *fm1) {
	CartXferRegister extract_ky1 = (*ky1 & 0xff);
	CartXferRegister extract_ky2 = (*ky2 & 0xff);
	CartXferRegister extract_rt1 = (*rt1 & 0xff);
	CartXferRegister extract_ct1 = (*ct1 & 0xff);
	CartXferRegister extract_fm1 = (*fm1 & 0xff);

	//0 out everything besides ky1
	extract_ky1 >>= 56;
	extract_ky1 <<= 56;
	*ky1 = extract_ky1;

	//0 out everything besides ky2
	extract_ky2 <<= 8;
	extract_ky2 >>= 56;
	*ky2 = extract_ky2;

	//0 out everything besides rt1
	extract_rt1 <<= 16;
	extract_rt1 >>= 63;
	*rt1 = extract_rt1;

	//0 out everything besides ct1
	extract_ct1 <<= 17;
	extract_ct1 >>= 48;
	*ct1 = extract_ct1;

	//0 out everything besides fm1
	extract_fm1 <<= 33;
	extract_fm1 >>= 48;
	*fm1 = extract_fm1;

	return(0);

}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_cache_size
// Description  : Getter function to retrieve cache size
//
// Inputs       : none
// Outputs      : cache_structure.size

int get_cache_size(void) {
	
	return cache_structure.size;
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_cache_num_occupied
// Description  : Getter function to retrieve number of frames in the cache that is in use
//
// Inputs       : none
// Outputs      : cache_structure.num_occupied

int get_cache_num_occupied(void) {
	
	return cache_structure.num_occupied;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_lowest_time_frame
// Description  : Getter function to get the frame number of the frame that was last used
//
// Inputs       : none
// Outputs      : frames[index_of_frame].frame_num

CartFrameIndex get_lowest_time_frame(void) {
	//set variables
	int min_time = (int) INFINITY;
	int index_of_frame = 0;

	//find the frame with the smallest time
	for(int i = 0; i < cache_structure.size; i++) {
		if(cache_structure.frames[i].time < min_time) {
			min_time = cache_structure.frames[i].time;
			index_of_frame = i;
		}
	}

	return (cache_structure.frames[index_of_frame].frame_num);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_lowest_time_cart
// Description  : Getter function to get the cart number of the frame that was last used
//
// Inputs       : none
// Outputs      : frames[index_of_frame].cart_num

CartFrameIndex get_lowest_time_cart(void) {
	//set variables
	int min_time = (int) INFINITY;
	int index_of_frame = 0;

	//find the frame with the smallest time
	for(int i = 0; i < cache_structure.size; i++) {
		if(cache_structure.frames[i].time < min_time) {
			min_time = cache_structure.frames[i].time;
			index_of_frame = i;
		}
	}

	return (cache_structure.frames[index_of_frame].cart_num);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : update_cache
// Description  : Used to update the buffer and time of a frame that is already in the cache
//
// Inputs       : cart - cart number of the frame to cache
//				  frm - frame number of the frame to cache
//				  buf - buffer of the frame to cache
// Outputs      : 0 if successful

int update_cache(CartridgeIndex cart, CartFrameIndex frm, void *buf) {

	//find where the frame is within the cache
	for(int i = 0; i < cache_structure.size; i++) {
		if(cache_structure.frames[i].cart_num == cart && cache_structure.frames[i].frame_num == frm) {
			//update the time variable of the frame to cache
			cache_structure.frames[i].time = global_time;
			//update the buffer of the frame to cache
			strncpy(cache_structure.frames[i].framebuf, buf, 1024);
			//increment time
			global_time++;
			break;
		}
	}

	return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : set_cart_cache_size
// Description  : Set the size of the cache (must be called before init)
//
// Inputs       : max_frames - the maximum number of items your cache can hold
// Outputs      : 0 if successful, -1 if failure

int set_cart_cache_size(uint32_t max_frames) {

	//intialize cache size
	cache_structure.size = max_frames;

	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : init_cart_cache
// Description  : Initialize the cache and note maximum frames
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int init_cart_cache(void) {

	//turn on the cache
	cache_structure.open = true;
	//intialize the number of frames currently in cache to 0
	cache_structure.num_occupied = 0;

	//intialize the cache
	for(int i = 0; i < cache_structure.size; i++) {
		cache_structure.frames[i].time = -1;
		cache_structure.frames[i].cart_num = -1;
		cache_structure.frames[i].frame_num = -1;
		strncpy(cache_structure.frames[i].framebuf, " ", 1);
	}

	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : close_cart_cache
// Description  : Clear all of the contents of the cache, cleanup
//
// Inputs       : none
// Outputs      : o if successful, -1 if failure

int close_cart_cache(void) {

	//intialze the cache
	for(int i = 0; i < DEFAULT_CART_FRAME_CACHE_SIZE; i++) {
		cache_structure.frames[i].time = -1;
		cache_structure.frames[i].cart_num = -1;
		cache_structure.frames[i].frame_num = -1;
		strncpy(cache_structure.frames[i].framebuf, " ", 1);
	}

	//close the cache structure
	cache_structure.open = false;

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : put_cart_cache
// Description  : Put an object into the frame cache
//
// Inputs       : cart - the cartridge number of the frame to cache
//                frm - the frame number of the frame to cache
//                buf - the buffer to insert into the cache
// Outputs      : 0 if successful, -1 if failure

int put_cart_cache(CartridgeIndex cart, CartFrameIndex frm, void *buf)  {

	//put frame into buffer
	for(int i = 0; i < cache_structure.size; i++) {
		//find a cache frame that is not used
		if(cache_structure.frames[i].time == -1) {
			strncpy(cache_structure.frames[i].framebuf, (char *) buf, 1024);
			cache_structure.frames[i].time = global_time;
			cache_structure.frames[i].cart_num = cart;
			cache_structure.frames[i].frame_num = frm;
			cache_structure.num_occupied++;
			global_time++;
			break;
		}
	}
	
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_cart_cache
// Description  : Get an frame from the cache (and return it)
//
// Inputs       : cart - the cartridge number of the cartridge to find
//                frm - the  number of the frame to find
// Outputs      : pointer to cached frame or NULL if not found

void * get_cart_cache(CartridgeIndex cart, CartFrameIndex frm) {
	//set variables
	int index_of_frame = 0;
	bool found = false;

	//see if frame is in cache
	for(int i = 0; i < cache_structure.size; i++) {
		if(cache_structure.frames[i].cart_num == cart && cache_structure.frames[i].frame_num == frm) {
			index_of_frame = i;
			found = true;
			break;
		}
	}

	//if the frame is found, return its buffer
	if(found == true) {
		cache_structure.frames[index_of_frame].time = global_time;
		global_time++;
		return &(cache_structure.frames[index_of_frame].framebuf);
	}
	//if the frame is not found, return NULL
	else {
		global_time++;
		return (NULL);
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : delete_cart_cache
// Description  : Remove a frame from the cache (and return it)
//
// Inputs       : cart - the cart number of the frame to remove from cache
//                blk - the frame number of the frame to remove from cache
// Outputs      : pointed buffer inserted into the object

void * delete_cart_cache(CartridgeIndex cart, CartFrameIndex blk) {
	//used to find the index of the frame we want to delete
	int index_of_frame = 0;

	//find the frame we are deleting
	for(int i = 0; i < cache_structure.size; i++) {
		if(cache_structure.frames[i].cart_num == cart && cache_structure.frames[i].frame_num == blk) {
			index_of_frame = i;
			cache_structure.frames[i].frame_num = -1;
			cache_structure.frames[i].cart_num = -1;
			cache_structure.frames[i].time = -1;
			cache_structure.num_occupied--;
			break;
		}
	}

	return &(cache_structure.frames[index_of_frame].framebuf);
}

//
// Unit test

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cartCacheUnitTest
// Description  : Run a UNIT test checking the cache implementation
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int cartCacheUnitTest(void) {
	//set variables
	int num_of_cache_frames = 80;
	int cache_size = 40;
	int get_or_put = 0;
	CartFrameIndex temp_cart = 0;
	CartFrameIndex temp_frame = 0;

	//reset timer
	global_time = 0;

	//set of n(num_of_cache_frames) cache items
	struct CacheFrames unit_cache_frames[num_of_cache_frames];

	//cache frame pointer
	struct CacheFrames *framePtr = NULL;

	//check pointer for read
	char *readPtr = NULL;

	//set size
	set_cart_cache_size(cache_size);

	//initialize cache
	init_cart_cache();

	//give each unit_frame a different cart number and frame number
	for(int i = 0; i < num_of_cache_frames; i++) {
		unit_cache_frames[i].frame_num = (CartFrameIndex) (rand() % 1024);
		unit_cache_frames[i].cart_num = i;
		unit_cache_frames[i].mark = false;
	}

	//10,000 random unit tests
	for(int i = 0; i < 10000; i++) {
		global_time++;
		//reinitialize pointers after every iteration
		framePtr = NULL;
		readPtr = NULL;

		//pick read or insert from cache: 0 - read, 1 - insert
		get_or_put = (int) rand() % 2;

		//pick random cache item
		framePtr = &(unit_cache_frames[i % num_of_cache_frames]);

		//we will read the cache
		if(get_or_put == 0) {
			//test get_cart_cache and used to check if the frame is in the cache
			readPtr = get_cart_cache(framePtr->cart_num, framePtr->frame_num);

			//fail if mark == true and readPtr == NULL
			if(readPtr == NULL && framePtr->mark == true) {
				printf("Fail on the first if statement \n");
				return(-1);
			}
			//fail if mark == false and readPtr != NULL 
			if(readPtr != NULL && framePtr->mark == false) {
				printf("Fail on the second if statement \n");
				return(-1);
			}
		}
		//we will insert into the cache
		else {
			//used as a check to see if frame already exists in cache
			readPtr = get_cart_cache(framePtr->cart_num, framePtr->frame_num);

			//the frame is not in the cache and cache is full
			if (readPtr == NULL && cache_structure.size == cache_structure.num_occupied) {
				temp_cart = get_lowest_time_cart();
				temp_frame = get_lowest_time_frame();

				//evict frame
				delete_cart_cache(temp_cart, temp_frame);

				//find the frame in test caches and mark not in cache
				for(int i = 0; i < num_of_cache_frames; i++) {
					if(unit_cache_frames[i].cart_num == temp_cart && unit_cache_frames[i].frame_num == temp_frame) {
						unit_cache_frames[i].mark = false;
						break;
					}
				}
				//insert new frame
				put_cart_cache(framePtr->cart_num, framePtr->frame_num, "Cant use null bruh");
				//mark frame is in cache
				framePtr->mark = true;
			}
			//if the frame is not in the cache and the cache is not full, just insert and mark
			else if(readPtr == NULL && cache_structure.size > cache_structure.num_occupied) {
				put_cart_cache(framePtr->cart_num, framePtr->frame_num, "Jason Ling");
				framePtr->mark = true;
			}
			//if the frame is in the cache, update time
			else if(readPtr != NULL) {
				update_cache(framePtr->cart_num, framePtr->frame_num, "A billion hours on this assignment smh");
				framePtr->mark = true;
			}
			//fail if insertion failed
			else {
				return (-1);
			}
		}
	}

	framePtr = NULL;
	free(framePtr);

	readPtr = NULL;
	free(readPtr);
	// Return successfully
	logMessage(LOG_OUTPUT_LEVEL, "Cache unit test completed successfully.");
	return(0);
}