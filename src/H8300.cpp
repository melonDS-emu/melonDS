/*
	This file is copyright Barret Klics and maybe melonDS team I guess?
   */


#include "H8300.h"
#include <string.h>
#include "types.h"
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>



H8300::H8300(){
	printf("H8/300 has been created");
    	//debugSerialPort();
}

H8300::~H8300(){


	close(fd);

	printf("H8/300 has been killed");
}

void H8300::speak(){
	printf("H8 CHIP IS SPEAKING");
}

int H8300::debugSerialPort(){
	int option = 0;
	while(option != 9){
		printf("\n\nWelcome to the mini H8/300 Debugger. Please select an option.\n");
		printf("1. Read\n");
		printf("2. Write\n");
		printf("3. FD status\n");
		printf("4. Configure Serial Port\n");
		printf("5. Close serial port\n");
		printf("8. Exit without closing port\n");
		printf("9. Exit and close serial port\n");
		scanf("%d", &option);
		if (option ==1){
			printf("You have 5 seconds to send me some data!!!\n");
			sleep(5);

			memset(buf, 0, sizeof(buf));
			printf("reading");

			int len;
			len = read(fd, buf, sizeof(buf));

			if (len == -1){
				perror("Failed to read port");
			}
			printf("Rxed %d bytes\n", len);

			for (int i = 0; i < len; i++){
				printf("0x%02x ", (unsigned char)buf[i]);
			}
			printf("\n");
		}

		else if (option == 2){
			printf("writing not implemented\n");
		}
		else if (option == 3){

			printf("FD status: %d\n", fd);
		}

		else if (option == 4){
			configureSerialPort();
		}
		else if (option == 5){
			close(fd);
			printf("hopefully closed fd\n");
		}

		else if (option == 8){
			break;
		}

		else if (option == 9){
			close(fd);
			printf("Exiting.\n");
			break;
		}
	}
	return 0;
}


//This should be called dynamically??
int H8300::configureSerialPort(){


	memset(buf, 0, sizeof(buf));
	//This should be configures in options
	const char *portname = "/dev/ttyUSB0";

	//sudo chmod 666 /dev/ttyUSB0
	fd = open(portname, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd == -1){
		printf("H8/300 failed to open the serial port\n");
		perror("Err:");
		return 1;
	}

	struct termios options;
	tcgetattr(fd, &options);

	//Set walker params. We can potentially make this different if we want to talk to walker EMUS


	//baud rate
	cfsetispeed(&options, B115200);
	cfsetospeed(&options, B115200);


	//no parity, 1 stop bit, clear char size mask, 8 data bits, no hardware flow ctrl,
	// enable reciever, ignore modem control lines
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	options.c_cflag &= ~CRTSCTS;
	options.c_cflag |= CREAD | CLOCAL;


//	options.c_cc[VTIME] = 1;
//	options.c_cc[VMIN] = 0;


	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &options);

	if (tcgetattr(fd, &options) == -1){
		printf("H8/300 failed to configure the serial port\n");
		close(fd);
		return 1;
	}
	return 0;
}


//Working

int H8300::readSerialPort(){
	char tempBuf[0xB8];
	int len = 1;  //read(fd, tempBuf, sizeof(tempBuf));
	u8 pointer = 0;
	while (len > 0){

		usleep(4000);
		len = read(fd, tempBuf, sizeof(tempBuf));

		if (len < 0) break;

		for(int i = 0; i < len; i++){

			buf[pointer + i] = tempBuf[i];

		}
		pointer = pointer + len;
	}


	tcflush(fd, TCIFLUSH);
	recvLen = pointer;

	if (recvLen == 0) return 0;


	printf("\nrecieved a packet of len: %d\n", recvLen);

	for (int i = 0; i < recvLen; i++){
		printf("0x%02x ", (u8)buf[i] ^ 0xaa);
	}
	printf("\n");


	return recvLen;
}

/*
int H8300::readSerialPort(){


	struct timeval tv;
	gettimeofday(&tv, NULL);
	long lastRxTime = tv.tv_usec;


	char tempBuf[0xB8];
	int len = read(fd, tempBuf, sizeof(tempBuf));
	u8 pointer = 0;

	if (len > 0){
		printf("\nConstructing a packet: %d + ", len);

		lastRxTime = tv.tv_usec;
		for(int i = 0; i < len; i++){
			buf[pointer + i] = tempBuf[i];
		}
		pointer = pointer + len;



		//We are now recieving a packet
		long cond = tv.tv_usec - lastRxTime;
		while(cond < 4000){
			//printf("last checkd cond: %ld\n", cond);
			cond = tv.tv_usec - lastRxTime;
			gettimeofday(&tv, NULL);

			len = read(fd, tempBuf, sizeof(tempBuf));




			if (len < 0){ continue;}

			else{
				printf("%d + ", len);
				lastRxTime = tv.tv_usec;
				for(int i = 0; i < len; i++){
					buf[pointer + i] = tempBuf[i];
				}
				pointer = pointer + len;
			}

		}

//		printf("STOPPED\n");
//		getchar();
	}


	recvLen = pointer;

	if (recvLen == 0) return 0;

	printf("\nrecieved a packet of len: %d\n", recvLen);

//	for (int i = 0; i < recvLen; i++){
//		printf("0x%02x ", (u8)buf[i] ^ 0xaa);
//	}
	printf("\n");
	return recvLen;
}
*/
int H8300::sendSerialPort(){




//	printf("\n Sending %d Bytes: %d\n", sendLen);
	/*
	for (int i = 0; i < sendLen; i++){

		printf("0x%02x ", (u8)sendBuf[i] ^ 0xaa);

	}
	printf("\n");*/
	u8 written = write(fd, sendBuf, sendLen);

	//printf("wrote %d bytes\n", written);

//	getchar();


	return 0;
}













//A botched implementation
u8 H8300::handleSPI(u8& IRCmd, u8& val, u32&pos, bool& last){

   // readSerialPort();





//    printf("IRCmd: %d		val:%02x   pos: %ld   last: %d\n", IRCmd, val, pos, last);
    switch (IRCmd)
    {
    case 0x00: // pass-through
	printf("CRITICAL ERROR IN H8300 emu, should not have recieved 0x00 command!");
        return 0xFF;


    case 0x01:

	if (pos == 0){
		printf("beginning new spi seq\n");
		return 0x00;
	}
	if (pos == 1){
		recvLen = readSerialPort();

//		printf("	pos 1: found %d bytes\n", recvLen);
		return recvLen;
	}
	else{
		u8 data = (unsigned char)buf[pos-2];
	//	printf("	pos: %02x, last: %d, dgettimeofday(&tv, NULL);ata: 0x%02x \n", pos, last, data);
		return data;
	}
	return 0x00;

    case 0x02:
	//printf("                        press any key to continue.");
	//getchar();

	if (pos == 0){

	//	memset(buf, 0, sizeof(buf)); //we can clear our read buffer because we are sending now. (make easier)
	//	recvLen = 0;

	}
	else{
		sendBuf[pos-1] = val; //Load the spi packet into the buffer;
	}

	if (last == 1){
		sendLen = pos;
		sendSerialPort();
	}
	return 0x00;
    case 0x08: // ID
	if (fd <0){
		printf("Configuring port during IR init sequence\n");
		configureSerialPort();

		tcflush(fd, TCIFLUSH);
	}

        return 0xAA;
    }

//    printf("Unhandled: %d		val:%d   pos: %ld   last: %d\n", IRCmd, val, pos, last);
    return 0x00;
}
