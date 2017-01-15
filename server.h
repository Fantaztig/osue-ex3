#ifndef myauthserver
#define myauthserver
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include "myshared.h"
#include<signal.h>
#include <assert.h>
#include <string.h>
#include <time.h>

typedef struct myDbObjectStruct{
		char login[20];
		char pass[20];
		char secret[50];
} myDbObject;

typedef struct myUserIdStruct{
	int userid;
	char login[20];
} session;

struct simpleListNode{
	void *data;
	struct simpleListNode *next;
};
typedef struct simpleListNode* List;

#endif