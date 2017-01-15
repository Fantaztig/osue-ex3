#ifndef myshared
#define myshared
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h> 
#include <semaphore.h>
#include <errno.h>
//semaphore def
#define CLIENT_READ_SEM "/1226747clrsem"
#define CLIENT_WRITE_SEM "/1226747clwsem"
#define SERVER_SEM "/1226747srwsem"

//protocoll def
#define REGISTER (1)
#define LOGIN (2)
#define WRITE_SECRET (3)
#define READ_SECRET (4)
#define LOGOUT (5)


//shared mem def
#define SHM_NAME "/1226747myshared"
#define PERMISSION (0600)
typedef struct myshmstruct {
	unsigned int state;
	int command;
	int sessId;
	char login[20];
	char pass[20];
	char secret[50];
} MyShm;

#endif