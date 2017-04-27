////////////////////////////////////////////////////////////////////////////////
//
//  File           : cart_driver.c
//  Description    : This is the implementation of the standardized IO functions
//                   for used to access the CART storage system.
//
//  Author         : [**Jason Ling**]
//  Last Modified  : [**October 23, 2016**]
//

// Includes
#include <stdlib.h>
#include <cmpsc311_log.h>
#include <string.h>

// Project Includes
#include <cart_driver.h>
#include <cart_controller.h>
#include <cart_cache.h>
#include <cart_network.h>
//
// Implementation

typedef int bool;
#define true 1
#define false 0

//Data structure that will have information about the cart and frame
struct Table {
	int16_t handle;
	uint32_t bytesUsed; //number of bytes used in frame
	CartXferRegister frame_num;
	CartXferRegister cart_num;
	struct Table *nextFrame;
};

//Data structure that will have information of the files
struct FileStructure{
	char fileName[1024];
	bool open; //check if file has been opened
	bool filled; //used to add file into empty space of data structure
	int16_t handle;
	uint32_t length;
	uint32_t location;
	struct Table *tablePtr;
};

//Our main data structure
struct CartStructure {
	bool cart_is_on;
	struct Table infoTable[CART_MAX_CARTRIDGES][CART_CARTRIDGE_SIZE];
	struct FileStructure fileTable[CART_MAX_TOTAL_FILES]; //need to initialize all handles to -1
};

//Global structure
struct CartStructure mainStructure;

////////////////////////////////////////////////////////////////////////////////
//
// Function     : create_cart_opcode
// Description  : Used to get the return value of the opcode
//
// Inputs       : 64_bit opcode
// Outputs      : 0 if successful

CartXferRegister create_cart_opcode(int64_t ky1, int64_t ky2, int64_t rt1, int64_t ct1, int64_t fm1) {
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
// Function     : extract_cart_opcode
// Description  : Used to extract the opcode
//
// Inputs       : 64_bit opcode
// Outputs      : 0 if successful

int extract_cart_opcode(CartXferRegister resp, CartXferRegister *ky1, CartXferRegister *ky2, CartXferRegister *rt1, 
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
// Function     : cart_poweron
// Description  : Startup up the CART interface, initialize filesystem
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t cart_poweron(void) {
 	//initialize cart
 	init_cart_cache();

 	//check if the cart is on already
 	if(mainStructure.cart_is_on == true) {
 		return (-1);
 	}
 	//if it is not on, power it on already
 	else {
 		//initialize data structure
 		for(int i = 0; i < CART_MAX_TOTAL_FILES; i++) {
 			strncpy(mainStructure.fileTable[i].fileName," ", CART_MAX_PATH_LENGTH);
 			mainStructure.fileTable[i].open = false;
 			mainStructure.fileTable[i].filled = false;
 			mainStructure.fileTable[i].handle = -1;
 			mainStructure.fileTable[i].length = 0;
 			mainStructure.fileTable[i].tablePtr = NULL;
 			mainStructure.fileTable[i].location = 0;
 		}
 		//initialize the file structure
 		for(int i = 0; i < CART_MAX_CARTRIDGES; i++) {
 			for(int j = 0; j <CART_CARTRIDGE_SIZE; j++) {
 				mainStructure.infoTable[i][j].handle = -1;
 				mainStructure.infoTable[i][j].bytesUsed = 0; 
 				mainStructure.infoTable[i][j].cart_num = i;
 				mainStructure.infoTable[i][j].frame_num = j;
 				mainStructure.infoTable[i][j].nextFrame = NULL;
 			}
 		}

 		//turn on the cart structure
 		mainStructure.cart_is_on = true;

 		//power on the cart
		//poweron_opcode = create_cart_opcode(CART_OP_INITMS, 0, 0, 0, 0);
		client_cart_bus_request(create_cart_opcode(CART_OP_INITMS, 0, 0, 0, 0), NULL);

		//load all the carts and zero it as we load
		for(int i = 0; i < CART_MAX_CARTRIDGES; i++) {
			client_cart_bus_request(create_cart_opcode(CART_OP_LDCART, 0, 0, i, 0), NULL);
			client_cart_bus_request(create_cart_opcode(CART_OP_BZERO, 0, 0, 0 ,0), NULL);
		}
	}

	// Return successfully
	return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_poweroff
// Description  : Shut down the CART interface, close all files
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t cart_poweroff(void) {

	//close the cache
	close_cart_cache();

	//fail if the cart is not open
	if(mainStructure.cart_is_on == false) {
		return (-1);
	}
	//power off the cart
	else {
		//power off the cart
		CartXferRegister poweroff_opcode = create_cart_opcode(CART_OP_POWOFF, 0, 0, 0 ,0);
		client_cart_bus_request(poweroff_opcode, NULL);

		//close the cart structure
		mainStructure.cart_is_on = false;

		//clear the cart table
		for(int i = 0; i < CART_MAX_TOTAL_FILES; i++) {
 			strncpy(mainStructure.fileTable[i].fileName," ", CART_MAX_PATH_LENGTH) ;
 			mainStructure.fileTable[i].open = false;
 			mainStructure.fileTable[i].filled = false;
 			mainStructure.fileTable[i].handle = -1;
 			mainStructure.fileTable[i].length = 0;
 			mainStructure.fileTable[i].tablePtr = NULL;
 			mainStructure.fileTable[i].location = 0;
 		}

 		//clear the file table
 		for(int i = 0; i < CART_MAX_CARTRIDGES; i++) {
 			for(int j = 0; j <CART_CARTRIDGE_SIZE; j++) {
 				mainStructure.infoTable[i][j].handle = -1;
 				mainStructure.infoTable[i][j].bytesUsed = 0; 
 				mainStructure.infoTable[i][j].cart_num = i;
 				mainStructure.infoTable[i][j].frame_num = j;
 				mainStructure.infoTable[i][j].nextFrame = NULL;
 			}
 		}

	}

	// Return successfully
	return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_open
// Description  : This function opens the file and returns a file handle
//
// Inputs       : path - filename of the file to open
// Outputs      : file handle if successful, -1 if failure

int16_t cart_open(char *path) {

	bool exists = false;
	int16_t counter = 0;
	char *name = path;
	int length_of_path = strlen(path);

	for(int i = 0; i < CART_MAX_TOTAL_FILES; i++) {
		//check if the file exists and is open already
		if(strncmp(mainStructure.fileTable[i].fileName, name, length_of_path) == 0 && mainStructure.fileTable[i].open == true) {
			exists = true;
			return (-1);
		}
		//check if the file exists and not opened
		if(strncmp(mainStructure.fileTable[i].fileName, name, length_of_path) == 0 && mainStructure.fileTable[i].open == false) {
			mainStructure.fileTable[i].open = true;
			mainStructure.fileTable[i].location = 0;
			exists = true;
			return (i);
		}
	}

	//if file doesn't exist, then create it in an empty spot
	if (exists == false) {
		while(exists == false) {
			if(mainStructure.fileTable[counter].filled == false) {
				mainStructure.fileTable[counter].handle = counter;
				strncpy(mainStructure.fileTable[counter].fileName, name, length_of_path);
				mainStructure.fileTable[counter].open = true;
				mainStructure.fileTable[counter].length = 0;
				mainStructure.fileTable[counter].filled = true;
				mainStructure.fileTable[counter].location = 0;
				//mainStructure.fileTable[counter].tablePtr = malloc(sizeof(struct Table)*1024);
				exists = true;
				break;
			}
			counter++;
		}
	}

	//return handle
	return (counter);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_close
// Description  : This function closes the file
//
// Inputs       : fd - the file descriptor
// Outputs      : 0 if successful, -1 if failure

int16_t cart_close(int16_t fd) {
	bool handle_is_valid = false;
	int index_of_file = 0;

	//check if file handle is valid
	for(int i = 0; i < CART_MAX_TOTAL_FILES; i++) {
		//handle exists so break loop
		if(mainStructure.fileTable[i].handle == fd) {
			handle_is_valid = true;
			index_of_file = i;
			break;
		}
	}
	//fail if handle is not valid
	if (handle_is_valid == false) {
		return(-1);
	}
	//fail if file not open
	else if (mainStructure.fileTable[index_of_file].open == false) {
		return(-1);
	}
	//close the file
	else {
		mainStructure.fileTable[index_of_file].open = false;
	}

	// Return successfully
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_read
// Description  : Reads "count" bytes from the file handle "fh" into the 
//                buffer "buf"
//
// Inputs       : fd - filename of the file to read from
//                buf - pointer to buffer to read into
//                count - number of bytes to read
// Outputs      : bytes read if successful, -1 if failure

int32_t cart_read(int16_t fd, void *buf, int32_t count) {

	bool handle_is_valid = false;
	int index_of_file = 0;

	//check if file handle is valid
	for(int i = 0; i < CART_MAX_TOTAL_FILES; i++) {
		//handle exists so break loop
		if(mainStructure.fileTable[i].handle == fd) {
			handle_is_valid = true;
			index_of_file = i;
			break;
		}
	}
	//if handle is not valid, fail
	if(handle_is_valid == false) {
		return (-1);
	}

	//opcodes used for reading
	CartXferRegister ky1 = 0;
	CartXferRegister ky2 = 0;
	CartXferRegister rt1 = 0;
	CartXferRegister ct1 = 0;
	CartXferRegister fm1 = 0;
	CartXferRegister action_code = 0;

	//define variables
	int counter = 0;
	int32_t start_read_frame = (mainStructure.fileTable[index_of_file].location)/ 1024; //what frame i start to read from
	int32_t start_read_bit = (mainStructure.fileTable[index_of_file].location % 1024); //where i wanna start reading in frame
	int32_t bytes_left_to_read = count; //keep track of how many bytes to read
	int32_t bytes_reading_now = 0; //keep track of the amount of bytes to read per iteration
	int start_buf_read_bit = 0; //keep track of what is currently read from the buffer
	char tempbuf[1024]; //temporary buffer for calculations
	char *cachebuf = NULL; //buffer used to check cache
	struct Table *tempPtr = mainStructure.fileTable[index_of_file].tablePtr; //pointer used to point to first frame of the file
	int bytes_read_already = 0; //keep track of the amount of bytes read already

	//if reading past end of the file, read until the end of the file
	if(count + mainStructure.fileTable[index_of_file].location > mainStructure.fileTable[index_of_file].length) {
		bytes_left_to_read = mainStructure.fileTable[index_of_file].length - mainStructure.fileTable[index_of_file].location;
		count = bytes_left_to_read;
	}

	//point to frame that is being read
	while(counter != start_read_frame) {
		tempPtr = tempPtr->nextFrame;
		counter++;
	}

	//begin reading
	while(bytes_left_to_read > 0) {
		//check how many bytes left to read
		if (start_read_bit + bytes_left_to_read > 1024) {
			bytes_reading_now = (1024 - start_read_bit);
		}
		else {
			bytes_reading_now = bytes_left_to_read;
		}

		//used to check if the frame is already in the cache
		cachebuf = get_cart_cache(tempPtr->cart_num, tempPtr->frame_num);

		//if the frame is in the cache, just read it into the buffer
		if(cachebuf != NULL) {
			memcpy(&((char *)buf)[start_buf_read_bit], &cachebuf[start_read_bit], bytes_reading_now);
		}
		//if the frame is not in the cache, go into memory
		else {
			//load the cart
			ky1 = CART_OP_LDCART;
			ct1 = tempPtr->cart_num;
			action_code = create_cart_opcode(ky1, ky2, rt1, ct1, fm1);
			client_cart_bus_request(action_code, NULL);
			extract_cart_opcode(action_code, &ky1, &ky2, &rt1, &ct1, &fm1);

			//read frame into temp buffer
			ky1 = CART_OP_RDFRME;
			fm1 = tempPtr->frame_num;
			action_code = create_cart_opcode(ky1, ky2, rt1, ct1, fm1);
			client_cart_bus_request(action_code, tempbuf); // buf = frame
			extract_cart_opcode(action_code, &ky1, &ky2, &rt1, &ct1, &fm1);

			//read into buf
			memcpy(&((char *)buf)[start_buf_read_bit], &tempbuf[start_read_bit], bytes_reading_now);
		}
		
		//update variables 
		start_buf_read_bit += bytes_reading_now;
		mainStructure.fileTable[index_of_file].location += bytes_reading_now;
		bytes_read_already = start_read_bit + bytes_reading_now;
		bytes_left_to_read -= bytes_reading_now;
		start_read_bit = 0;

		//if I reading more than one frame, go to the next frame
		if(bytes_read_already == 1024 && bytes_left_to_read > 0) {
			tempPtr = tempPtr->nextFrame;
		}
	}

	// Return successfully
	return (count);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_write
// Description  : Writes "count" bytes to the file handle "fh" from the 
//                buffer  "buf"
//
// Inputs       : fd - filename of the file to write to
//                buf - pointer to buffer to write from
//                count - number of bytes to write
// Outputs      : bytes written if successful, -1 if failure

int32_t cart_write(int16_t fd, void *buf, int32_t count) {
	//CHECK IF I CAN WRITE BRO BUT LEMME GET THE INDEX OF MY FILE THO
	bool handle_is_valid = false;
	bool done = false;
	int index_of_file = 0;

	for(int i = 0; i < CART_MAX_TOTAL_FILES; i++) {
		//handle exists so break loop
		if(mainStructure.fileTable[i].handle == fd) {
			handle_is_valid = true;
			index_of_file = i;
			break;
		}
	}
	//if handle is not valid, fail
	if(handle_is_valid == false) {
		return (-1);
	}
	//check if length is 0, then point to a frame
	if(mainStructure.fileTable[index_of_file].tablePtr == NULL) {
		for(CartFrameIndex i = 0; i < CART_MAX_CARTRIDGES; i++) {
			for(CartridgeIndex j = 0; j < CART_CARTRIDGE_SIZE; j++) {
				if(mainStructure.infoTable[i][j].handle == -1) {
					mainStructure.fileTable[index_of_file].tablePtr = &mainStructure.infoTable[i][j];
					mainStructure.infoTable[i][j].handle = fd;
					done = true;
					break;
				}
			}
			//break when found free frame
			if(done == true) {
				break;
			}
		}
	}

	//OPCODES
	CartXferRegister ky1 = 0;
	CartXferRegister ky2 = 0;
	CartXferRegister rt1 = 0;
	CartXferRegister ct1 = 0;
	CartXferRegister fm1 = 0;
	CartXferRegister action_code = 0;

	//START THE REAL WRITING STUFF
	int start_write_frame = (mainStructure.fileTable[index_of_file].location)/ 1024; //what frame i start to write in
	int start_write_bit = (mainStructure.fileTable[index_of_file].location % 1024); //where i wanna start writing in frame
	int32_t bytes_left_to_write = count; //keep track of bytes left to write
	int bytes_writing_now = 0; //amount of bytes writing per iteration
	int buf_starting_point = 0;	//starting of memcpy for the buffer that is getting passed
	char tempbuf[1024]; //temporary buffer for memcpy
	struct Table *tempPtr = mainStructure.fileTable[index_of_file].tablePtr; //pointer to first frame of the file
	int32_t counter = 0; //counter used for traversal to start_write_frame
	char *cachebuf = NULL; //buffer used to check the cache
	bool done2 = false; //used for linking to a new carts
	//temporary variables for eviction
	int temp_low_cart = 0;
	int temp_low_frame = 0;
	//traverse to frame to start writing now ima use ptr to write stuff
	while(counter != start_write_frame) {
		tempPtr = tempPtr->nextFrame;
		counter++;
	}

	//begin writing
	while(bytes_left_to_write > 0) {

		//figure out how many bytes im going to write per iteration
		if (start_write_bit + bytes_left_to_write > 1024) {
			bytes_writing_now = (1024 - start_write_bit);
		}
		else if(start_write_bit + bytes_left_to_write == 1024) {
			bytes_writing_now = bytes_left_to_write;
		}	
		else {
			bytes_writing_now = bytes_left_to_write;
		}

		//check if that frame is in the cache already
		cachebuf = get_cart_cache(tempPtr->cart_num, tempPtr->frame_num);

		//condition if the frame is not in the cache
		if(cachebuf == NULL) {
			//cache is full and the frame is not there
			if(get_cache_size() == get_cache_num_occupied()) {	

				//get the cart number and frame number to evict
				temp_low_cart = get_lowest_time_cart();
				temp_low_frame = get_lowest_time_frame();

				//get the frame that will be evicted from the cache
				cachebuf = delete_cart_cache(temp_low_cart, temp_low_frame);

				//put the new frame into a buffer
				//load the cart
				ky1 = CART_OP_LDCART;
				ct1 = tempPtr->cart_num;
				action_code = create_cart_opcode(ky1, ky2, rt1, ct1, fm1);
				client_cart_bus_request(action_code, NULL);
				extract_cart_opcode(action_code, &ky1, &ky2, &rt1, &ct1, &fm1);

				//read the frame
				ky1 = CART_OP_RDFRME;
				fm1 = tempPtr->frame_num;
				action_code = create_cart_opcode(ky1, ky2, rt1, ct1, fm1);
				client_cart_bus_request(action_code, tempbuf);
				extract_cart_opcode(action_code, &ky1, &ky2, &rt1, &ct1, &fm1);

				memcpy(&tempbuf[start_write_bit], &((char *)buf)[buf_starting_point], bytes_writing_now);

				//write to the frame
				ky1 = CART_OP_WRFRME;
				fm1 = tempPtr->frame_num;
				action_code = create_cart_opcode(ky1, ky2, rt1, ct1, fm1);
				client_cart_bus_request(action_code, tempbuf);
				extract_cart_opcode(action_code, &ky1, &ky2, &rt1, &ct1, &fm1);

				//insert the new frame into the cache
				put_cart_cache(tempPtr->cart_num, tempPtr->frame_num, tempbuf);
			}
			//cache is not full and frame is not there
			else {
				//insert new frame into the cache
				memcpy(&tempbuf[start_write_bit], &((char *)buf)[buf_starting_point], bytes_writing_now);

				//load the cart
				ky1 = CART_OP_LDCART;
				ct1 = tempPtr->cart_num;
				action_code = create_cart_opcode(ky1, ky2, rt1, ct1, fm1);
				client_cart_bus_request(action_code, NULL);
				extract_cart_opcode(action_code, &ky1, &ky2, &rt1, &ct1, &fm1);

				//write to the frame
				ky1 = CART_OP_WRFRME;
				fm1 = tempPtr->frame_num;
				action_code = create_cart_opcode(ky1, ky2, rt1, ct1, fm1);
				client_cart_bus_request(action_code, tempbuf);
				extract_cart_opcode(action_code, &ky1, &ky2, &rt1, &ct1, &fm1);

				//insert into the cache
				put_cart_cache(tempPtr->cart_num, tempPtr->frame_num, tempbuf);

			}
		}
		//frame is already in the cache
		else {
			//write into the buffer
			memcpy(&cachebuf[start_write_bit], &((char *)buf)[buf_starting_point], bytes_writing_now);

			//load the cart
			ky1 = CART_OP_LDCART;
			ct1 = tempPtr->cart_num;
			action_code = create_cart_opcode(ky1, ky2, rt1, ct1, fm1);
			client_cart_bus_request(action_code, NULL);
			extract_cart_opcode(action_code, &ky1, &ky2, &rt1, &ct1, &fm1);

			//write to the frame
			ky1 = CART_OP_WRFRME;
			fm1 = tempPtr->frame_num;
			action_code = create_cart_opcode(ky1, ky2, rt1, ct1, fm1);
			client_cart_bus_request(action_code, cachebuf);
			extract_cart_opcode(action_code, &ky1, &ky2, &rt1, &ct1, &fm1);

			//update the time and buffer of the frame inside the cache
			update_cache(tempPtr->cart_num, tempPtr->frame_num, cachebuf);
		}

		//update location
		mainStructure.fileTable[index_of_file].location += bytes_writing_now;
		//reduce the bytes left to write
		bytes_left_to_write -= bytes_writing_now;

		//update length if necessary
		if (mainStructure.fileTable[index_of_file].location > mainStructure.fileTable[index_of_file].length) {
			mainStructure.fileTable[index_of_file].length = mainStructure.fileTable[index_of_file].location;
		}

		//update variables
		tempPtr->bytesUsed = start_write_bit + bytes_writing_now;
		buf_starting_point += bytes_writing_now;
		start_write_bit = 0;

		//if bytes used = 1024 and next frame is null
		if (tempPtr->bytesUsed == 1024 && tempPtr->nextFrame == NULL) {
			//find free frame and link
			for(CartridgeIndex i = tempPtr->cart_num; i < CART_MAX_CARTRIDGES; i++) {
				for(CartFrameIndex j = 0; j < CART_CARTRIDGE_SIZE; j++) {
					if (mainStructure.infoTable[i][j].handle == -1) {
						tempPtr->nextFrame = &mainStructure.infoTable[i][j];
						mainStructure.infoTable[i][j].handle = tempPtr->handle; //change handle
						tempPtr = tempPtr->nextFrame; //move pointer
						done2 = true;
						break;
					}
				}
				//break if found frame already
				if(done2 == true) {
					break;
				}
			}
		} 
		else if (tempPtr->bytesUsed == 1024 && tempPtr->nextFrame != NULL){
			//point to next frame
			tempPtr = tempPtr->nextFrame;
		}
	}

	// Return successfully
	return (count);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_seek
// Description  : Seek to specific point in the file
//
// Inputs       : fd - filename of the file to write to
//                loc - offfset of file in relation to beginning of file
// Outputs      : 0 if successful, -1 if failure

int32_t cart_seek(int16_t fd, uint32_t loc) {
	bool handle_is_valid = false;
	int index_of_file = 0;

	for(int i = 0; i < CART_MAX_TOTAL_FILES; i++) {
		if(mainStructure.fileTable[i].handle == fd) {
			index_of_file = i;
			handle_is_valid = true;
			break;
		}
	}
	//check if handle is valid
	if (handle_is_valid == false) {
		return (-1);
	}
	//check if trying to seek greater than length of file
	else if (loc > mainStructure.fileTable[index_of_file].length) {
		return (-1);
	}
	//check if file is open
	else if (mainStructure.fileTable[index_of_file].open == false) {
		return (-1);
	}
	//change location pointer
	else {
		mainStructure.fileTable[index_of_file].location = loc;
	}
	
	// Return successfully
	return (0);
}