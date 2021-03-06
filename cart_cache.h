#ifndef CART_FRAME_CACHE_INCLUDED
#define CART_FRAME_CACHE_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File           : cart_frame_cache.h
//  Description    : This is the header file for the implementation of the
//                   frame cache for the cartridge memory system driver.
//
//  Author         : Patrick McDaniel
//  Last Modified  : Sun Oct 16 07:59:59 EDT 2016
//

// Includes
#include <cart_controller.h>

// Defines
#define DEFAULT_CART_FRAME_CACHE_SIZE 1024  // Default size for cache

///
//My function declarations
int update_cache(CartridgeIndex cart, CartFrameIndex frm, void *buf);
	//Update the frame in the cache if it is found

void * delete_cart_cache(CartridgeIndex cart, CartFrameIndex blk);
	//Delete a frame from the cache

int get_cache_size(void);
	//Get the cache size

int get_cache_num_occupied(void);
	//Get the number of frames used in the cache

CartFrameIndex get_lowest_time_frame(void);
	//Get the eviction frame

CartFrameIndex get_lowest_time_cart(void);
	//Get the eviction cart

// Cache Interfaces

int set_cart_cache_size(uint32_t max_frames);
	// Set the size of the cache (must be called before init)

int init_cart_cache(void);
	// Initialize the cache 

int close_cart_cache(void);
	// Clear all of the contents of the cache, cleanup

int put_cart_cache(CartridgeIndex cart, CartFrameIndex frm, void *frame);
	// Put an object into the object cache, evicting other items as necessary

void * get_cart_cache(CartridgeIndex dsk, CartFrameIndex blk);
	// Get an object from the cache (and return it)

//
// Unit test

int cartCacheUnitTest(void);
	// Run a UNIT test checking the cache implementation

#endif
