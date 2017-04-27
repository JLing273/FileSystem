////////////////////////////////////////////////////////////////////////////////
//
//  File          : cart_client.c
//  Description   : This is the client side of the CART communication protocol.
//
//   Author       : Jason Ling
//  Last Modified : Friday, December 9
//

// Include Files
#include <stdio.h>

// Project Include Files
#include <cart_network.h>
#include <cmpsc311_util.h>
#include <cmpsc311_log.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

//
//  Global data
int client_socket = -1;
int                cart_network_shutdown = 0;   // Flag indicating shutdown
unsigned char     *cart_network_address = NULL; // Address of CART server
unsigned short     cart_network_port = 0;       // Port of CART serve
unsigned long      CartControllerLLevel = LOG_INFO_LEVEL; // Controller log level (global)
unsigned long      CartDriverLLevel = 0;     // Driver log level (global)
unsigned long      CartSimulatorLLevel = 0;  // Driver log level (global)

//
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : extract_reg
// Description  : Shift the register 56 bits to the right to get ky1
//
// Inputs       : reg - the request reqisters for the command
// Outputs      : decision - 8 bit ky1


int8_t extract_reg(CartXferRegister reg) {
	int8_t decision = 0;
	decision = reg >> 56;

	return decision;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : client_cart_bus_request
// Description  : This the client operation that sends a request to the CART
//                server process.   It will:
//
//                1) if INIT make a connection to the server
//                2) send any request to the server, returning results
//                3) if CLOSE, will close the connection
//
// Inputs       : reg - the request reqisters for the command
//                buf - the block to be read/written from (READ/WRITE)
// Outputs      : the response structure encoded as needed

CartXferRegister client_cart_bus_request(CartXferRegister reg, void *buf) {
	char *ip = CART_DEFAULT_IP;
	struct sockaddr_in caddr;
	uint64_t value = 0;
	int8_t opcode;

	//if socket handle == -1, there is no connection
	if(client_socket == -1) {

		caddr.sin_family = AF_INET;
		caddr.sin_port = htons(CART_DEFAULT_PORT);

		//setup address
		if(inet_aton(ip, &(caddr.sin_addr)) == 0) {
			printf("Setting up an address caused an error");
			return (-1);
		}
		//create a socket
		client_socket = socket(PF_INET, SOCK_STREAM, 0);

		if(client_socket == -1) {
			printf("Creating a socket caused an error\n");
			return (-1);
		}
		//connect socket and address
		if(connect(client_socket, (const struct sockaddr *) &caddr, sizeof(caddr)) == -1) {
			printf("Connecting a socket caused an error\n");
		}
	}

	//get the ky1
	opcode = extract_reg(reg);
	//convert value to network format
	value = htonll64(reg);

	switch(opcode) {
		//if reading
		case CART_OP_RDFRME:
			//write to the network
			if(write(client_socket, &value, sizeof(value)) != sizeof(value)) {
				printf("Error writing to the network");
				return (-1);
			}

			//read to the network
			if(read(client_socket, &value, sizeof(value)) != sizeof(value)) {
				printf("Error reading from the network");
			}

			//convert to host format
			value = ntohll64(value);

			//read the buffer
			if(read(client_socket, buf, 1024) != 1024) {
				printf("Did not read 1024 bytes\n");
			}

			break;
		//if writing
		case CART_OP_WRFRME:
			//write to the network
			if(write(client_socket, &value, sizeof(value)) != sizeof(value)) {
				printf("Error writing network data in write condition");
			}

			//write the buffer to the network
			if(write(client_socket, buf, 1024) != 1024) {
				printf("Error 2nd writing network data in write condition\n");
			}	

			//read to the buffer
			if(read(client_socket, &value, sizeof(value)) != sizeof(value)) {
				printf("Error reading network data in write condition\n");
			}

			//convert reg to host format
			value = ntohll64(value);

			break;
		//if powering off
		case CART_OP_POWOFF:
			//write to the network
			if(write(client_socket, &value, sizeof(value)) != sizeof(value)) {
				printf("Error writing in power off\n");
			}

			//read to the network
			if(read(client_socket, &value, sizeof(value)) != sizeof(value)) {
				printf("Error reading in power off");
			}

			//convert reg to host format
			value = ntohll64(value);

			//close the client socket
			close(client_socket);

			//change socket handle
			client_socket = -1;
			break;
		//if initializing cart
		case CART_OP_INITMS:
			//writing to the network
			if(write(client_socket, &value, sizeof(value)) != sizeof(value)) {
				printf("Error in writing init case\n");
			}

			//read the network
			if(read(client_socket, &value, sizeof(value)) != sizeof(value)) {
				printf("Error in reading init case");
			}

			//convert reg to host format
			value = ntohll64(value);
			break;

		//if zeroing out the cart
		case CART_OP_BZERO:
			//write to the network
			if(write(client_socket, &value, sizeof(value)) != sizeof(value)) {
				printf("Error in writing bzero case\n");
			}

			//read the network
			if(read(client_socket, &value, sizeof(value)) != sizeof(value)) {
			 	printf("Error in reading bzero case");
			}
			//convert reg to host format
			value = ntohll64(value);

			break;
		//default statement
		default:
			//write to the network
			if(write(client_socket, &value, sizeof(value)) != sizeof(value)) {
				printf("Error in writing default case\n");
			}

			//read from the network
			if(read(client_socket, &value, sizeof(value)) != sizeof(value)) {
				printf("Error in reading default case");
			}

			//convert to host format
			value = ntohll64(value);

			break;

	}

	//return register
	return value;
}
