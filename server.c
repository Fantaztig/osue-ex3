/**
 * @file server.c
 * @author David Schröder 1226747
 * @brief Server for auth exercise
 * @details Initializes a database and waits for clients to connect via shm
 * @date 08.01.2017
 */
 #include "server.h"
  /**
 * @brief Parse command line options
 * @param argc The argument counter
 * @param argv The argument vector
 */
static void parse_args(int argc, char **argv);

 /**
 * @brief exit with proper ressource freeing
 * @param exitcode the exitcode to return
 * @param errmsg message to print before exiting
 */
static void bailout(int exitcode, const char *errmsg);

 /**
 * @brief allocates needed ressources
 */
static void allocate_ressources(void);

 /**
 * @brief tries to free all ressources
 */
static void free_ressources(void);

 /**
 * @brief inserts a data given via pointer into the list
 * @param list the list to insert into
 * @param data the object to insert into the list
 */
static void insert(List list, void *data);

 /**
 * @brief frees all list elements memory
 * @param list the list to clear
 */
static void emptyList(List list);

 /**
 * @brief dumps the db into csv
 */
static void dumpdb(void);

 /**
 * @brief same as strcpy but adds tailing null byte after size-1 chars
 * @param dest string to copy to
 * @param source string to copy from
 * @param size maximum size including null byte
 */
static void mystrcpy(char *dest, char *source, int size);

 /**
 * @brief waits for semaphore and exits gracefully in case of signal
 * @param sem semaphore to wait on
 * @param description semaphore description that is printed in case of error
 */
static void wait_for_sem(sem_t *sem, char *description);

 /**
 * @brief searches the database for a myDbObject with the given username
 * @param list list of db entries
 * @param username username to search for
 */
static myDbObject *search_for(List list, char *username);

 /**
 * @brief searches the database for a session object with the given sessionid
 * @param list list of sessions
 * @param sessionid sessionid to look for 
 */
static session *get_session(List list, int sessionid);

 /**
 * @brief drops a session from the db
 * @param list list of sessions
 * @param sessionid sessionid to look for
 */
static void drop_session(List list, int sessionid);

 /**
 * @brief handles the given signal
 * @param signo number of the signal to handle
 */
static void handle_signal(int signo);
 /*
 * the programs name
 */
static char *myname;

 /*
 * shared memory for communication with the clients
 */
static MyShm *shared;
static int shmfd;
volatile sig_atomic_t quit = 0;

 /*
 * semaphore for synchronization
 */
static sem_t *s_sem;
static sem_t *c_r_sem;
static sem_t *c_w_sem;

static List db;

static List users;

/**
 * @brief Program entry point
 * @param argc The argument counter
 * @param argv The argument vector
 * @details initializes the database and waits for clients to connect
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error or false parameters
 */
int main(int argc, char **argv){
	struct sigaction s;
	s. sa_handler = handle_signal ;
	s. sa_flags = 0 ;
	if(sigemptyset (&s. sa_mask )==-1){
		bailout(EXIT_FAILURE,"error on sigaction initialization");
	}
	if(sigaddset (&s. sa_mask , SIGALRM )==-1){
		bailout(EXIT_FAILURE,"error on sigaction initialization");
	}
	if(sigaction (SIGINT , &s, NULL )==-1){
		bailout(EXIT_FAILURE,"error on sigaction initialization");
	}
	if(sigaction (SIGTERM , &s, NULL )==-1){
		bailout(EXIT_FAILURE,"error on sigaction initialization");
	}
	if(atexit(free_ressources)!=0){
		bailout(EXIT_FAILURE,"couldnt set atexit");
	}
	allocate_ressources();
	parse_args(argc,argv);
	
	
	shared->state = 0;
	srand(time(NULL));
	wait_for_sem(s_sem,"server sem");
	while(!quit){
		switch(shared->command){
			case REGISTER:
				if(search_for(db,shared->login)!=NULL){
					memset(shared, 0, sizeof(MyShm));
					shared->state = 1;
				}else{
					myDbObject *new;
					if((new = (myDbObject*) malloc(sizeof(myDbObject)))==NULL){
						bailout(EXIT_FAILURE,"malloc failed");
					}
					mystrcpy(new->login,shared->login,20);
					mystrcpy(new->pass,shared->pass,20);
					new->secret[0]='\0';
					insert(db,new);
					memset(shared, 0, sizeof(MyShm));
					shared->state = 0;
					(void)fprintf(stdout,"registered:%s\n",new->login);
				}
				
				int c = sem_post(c_r_sem);
				if(c!= 0){
					bailout(EXIT_FAILURE,"client read semaphore error");
				}
			break;
			case LOGIN:{
				myDbObject *userObj = (myDbObject*) search_for(db,shared->login);
				if(userObj != NULL){
					if(strcmp(shared->pass,userObj->pass)==0){
						session *new;
						if((new = (session*) malloc(sizeof(session)))==NULL){
						bailout(EXIT_FAILURE,"malloc failed");
						}
						mystrcpy(new->login,shared->login,20);
						new->userid = rand();
						while(get_session(users,new->userid)){
							new->userid = rand();
						}
						insert(users,new);
						memset(shared, 0, sizeof(MyShm));
						shared->sessId = new->userid;
						shared->state = 0;
						(void)fprintf(stdout,"logged in:%s with session id:%d\n",new->login,new->userid);
					}else{
						memset(shared, 0, sizeof(MyShm));
						shared->state = 1;
					}
				}else{
					memset(shared, 0, sizeof(MyShm));
					shared->state = 1;
				}
				
				int c = sem_post(c_r_sem);
				if(c!= 0){
					bailout(EXIT_FAILURE,"client write semaphore error");
				}
			}
			break;
			case WRITE_SECRET:{
				session *sess = (session*) get_session(users,shared->sessId);
				if(strcmp(sess->login,shared->login)==0){
					myDbObject *userObj = (myDbObject*) search_for(db,shared->login);
					mystrcpy(userObj->secret,shared->secret,50);
					memset(shared, 0, sizeof(MyShm));
					shared->state = 0;
					(void)fprintf(stdout,"user: %s wrote secret:%s\n",userObj->login,userObj->secret);
				}else{
					memset(shared, 0, sizeof(MyShm));
					shared->state = 1;
				}
				int c = sem_post(c_r_sem);
				if(c!= 0){
					bailout(EXIT_FAILURE,"client write semaphore error");
				}
			}
			break;
			case READ_SECRET:{
				session *sess = (session*) get_session(users,shared->sessId);
				if(strcmp(sess->login,shared->login)==0){
					myDbObject *userObj = (myDbObject*) search_for(db,shared->login);
					memset(shared, 0, sizeof(MyShm));
					mystrcpy(shared->secret,userObj->secret,50);
					shared->state = 0;
					(void)fprintf(stdout,"user: %s read secret:%s\n",userObj->login,userObj->secret);
				}else{
					memset(shared, 0, sizeof(MyShm));
					shared->state = 1;
				}
				int c = sem_post(c_r_sem);
				if(c!= 0){
					bailout(EXIT_FAILURE,"client write semaphore error");
				}
			}
			break;
			case LOGOUT:{
				session *sess = (session*) get_session(users,shared->sessId);
				if(strcmp(sess->login,shared->login)==0){
					drop_session(users,sess->userid);
					memset(shared, 0, sizeof(MyShm));
					shared->state = 0;
					(void)fprintf(stdout,"logged out user: %s session id:%d\n",sess->login,sess->userid);
				}else{
					(void)fprintf(stdout,"didnt log out user: %s expected login: %s\n",shared->login,sess->login);
					memset(shared, 0, sizeof(MyShm));
					shared->state = 1;
				}
				int c = sem_post(c_r_sem);
				if(c!= 0){
					bailout(EXIT_FAILURE,"client write semaphore error");
				}
			}
			break;
		}
		wait_for_sem(s_sem,"server sem");
	}
	if(quit){
		bailout(EXIT_FAILURE,"server closing due to signal");
	}
	bailout(EXIT_FAILURE,"returned?");
	
	

}

static void mystrcpy(char *dest,char *source,int size){
	strncpy(dest,source,size-1);
	dest[size]='\0';
}

static void handle_signal(int signo){
  if (signo == SIGINT){
	  quit = 1;
  }
  
  if (signo == SIGTERM){
	  quit = 1;
  }
   
}

static void allocate_ressources(void){
	//initialize db list
	db = (List) malloc(sizeof(List));
	if(db == NULL){
		bailout(EXIT_FAILURE,"couldnt malloc db");
	}
	
	//initialize session list
	users = (List) malloc(sizeof(List));
	if(db == NULL){
		bailout(EXIT_FAILURE,"couldnt malloc db");
	}
	
	//initialize shared memory
	shmfd =shm_open(SHM_NAME, O_RDWR | O_CREAT, PERMISSION);
	if(shmfd==-1){
		bailout(EXIT_FAILURE,"couldnt open or create shared memory");
	}
	if(ftruncate(shmfd, sizeof *shared) == -1){
		bailout(EXIT_FAILURE,"couldnt set the size of shared memory");
	}
	shared = mmap(NULL, sizeof *shared, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
	if(shared == MAP_FAILED){
		bailout(EXIT_FAILURE,"couldnt map shared memory");
	}
	
	//initialize semaphors
	s_sem = sem_open(SERVER_SEM, O_CREAT | O_EXCL, PERMISSION, 0);
	if(s_sem == SEM_FAILED){
		bailout(EXIT_FAILURE,"creating sem1 failed!");
	}
	c_r_sem = sem_open(CLIENT_READ_SEM, O_CREAT | O_EXCL, PERMISSION, 0);
	if(c_r_sem == SEM_FAILED){
		bailout(EXIT_FAILURE,"creating sem2 failed!");
	}
	c_w_sem = sem_open(CLIENT_WRITE_SEM, O_CREAT | O_EXCL, PERMISSION, 1);
	if(c_w_sem == SEM_FAILED){
		bailout(EXIT_FAILURE,"creating sem3 failed!");
	}
	
	
}


static void wait_for_sem(sem_t *sem, char *description){
	while((sem_wait(sem))==-1){
			if(errno == EINTR){
				if(quit){
					bailout(EXIT_SUCCESS,"terminated due to signal");
				}
			}else{
				bailout(EXIT_FAILURE,description);
			}
	}
}

static myDbObject *search_for(List list, char *username) {
    while (list != NULL) {
		if(list->data != NULL){
        if (strcmp(username,((myDbObject*)list->data)->login)==0)
            return (myDbObject*)list->data;
		}
        list = list->next;
    }
    return NULL;
}

static session *get_session(List list, int sessionid) {
    while (list != NULL) {
		if(list->data != NULL){
        if (((session*)list->data)->userid==sessionid)
            return (session*)list->data;
		}
        list = list->next;
    }
    return NULL;
}

static void drop_session(List list,int sessionid){
	List last = list;
	list = list->next; 
	while (list != NULL) {
		if(list->data != NULL){
			if (((session*)list->data)->userid==sessionid){
				last->next=list->next;
				free(list);
				return;
			}
		}
		last = list;
        list = list->next;
    }
}

static void insert(List list, void *data){
	List new_node = (List) malloc(sizeof(List));
	new_node->data = data;
	new_node->next = list->next;
	list->next     = new_node;
}

static void emptyList(List list){
	List next;
	while(list->next != NULL){
		next = list->next;
		if(list->data != NULL){
			free(list->data);
		}
		if(list != NULL){
			free(list);
		}
		list = next;
	}
	if(list->data != NULL){
		free(list->data);
	}
	if(list != NULL){
		free(list);
	}	
	
}

static void dumpdb(void){
	FILE *dbfile;
	if(!(dbfile = fopen("auth-server.db.csv","w"))){
		(void)fprintf(stderr,"Couldnt create file %s\n","auth-server.db.csv");
	}
	List list = db;
	while(list != NULL){
		if(list->data!=NULL){
			myDbObject *obj = ((myDbObject*)list->data);
			if(fprintf(dbfile,"%s;%s;%s\n",obj->login,obj->pass,obj->secret)<0){
				(void)fprintf(stderr,"failed writing line into db file with errno %d",errno);
				return;
			}
		}
		list = list->next;
	}
	if(fclose(dbfile)==EOF){
		(void)fprintf(stderr,"failed to close db file with errno %d",errno);
	}
}

static void parse_args(int argc, char **argv){
	myname = argv[0];
	int c;
	while ((c = getopt(argc, argv, "l:")) != -1){
		switch(c){
			case 'l':{
				FILE *dbfile;
				if(!(dbfile = fopen(optarg,"r"))){
					(void)fprintf(stderr,"Couldnt open file %s\n",optarg);
					bailout(EXIT_FAILURE,"usage auth-server [-l database]");
				}else{
					char buff[50];
					const char delim[2] = ";";
					char *token;
					myDbObject* obj;
					while (fgets(buff,50,dbfile)!=NULL){
						obj = (myDbObject*) malloc(sizeof(myDbObject));
						if(obj==NULL){
							bailout(EXIT_FAILURE,"malloc failed");
						}
						token = strtok(buff,delim);
						if(token == NULL){
							bailout(EXIT_FAILURE,"database file corrupted");
						}
						if(strlen(token)>19){
							bailout(EXIT_FAILURE,"database file corrupted");
						}
						mystrcpy(obj->login,token,20);
						
						token = strtok(NULL,delim);
						if(token == NULL){
							bailout(EXIT_FAILURE,"database file corrupted");
						}
						if(strlen(token)>19){
							bailout(EXIT_FAILURE,"database file corrupted");
						}
						mystrcpy(obj->pass,token,20);
						
						token = strtok(NULL,delim);
						if(token == NULL){
							bailout(EXIT_FAILURE,"database file corrupted");
						}
						if(strlen(token)>49){
							bailout(EXIT_FAILURE,"database file corrupted");
						}
						if(token[strlen(token)-1]=='\n'){
							token[strlen(token)-1]='\0';
						}
						mystrcpy(obj->secret,token,50);
						token = strtok(NULL,delim);
						if(token != NULL){
							bailout(EXIT_FAILURE,"database file corrupted");
						}
						insert(db,obj);
					}
					if(fclose(dbfile)==EOF){
						bailout(EXIT_FAILURE,"error closing db file");
					}
				}
				}
				break;
				
			case '?':
				bailout(EXIT_FAILURE,"usage auth-server [-l database]");
			default:
				assert(0);
				break;
		}
	}
	
}

static void bailout(int exitcode, const char *errmsg){
	(void)fprintf(stderr,"%s %s\n",myname,errmsg);
	exit(exitcode);
}

static void free_ressources(void){
	if(shared!=NULL){
		shared->state = -1;
	}
	if(c_r_sem !=NULL){
		(void)sem_post(c_r_sem);
	}
	
	(void)close(shmfd);
	(void)munmap(shared, sizeof *shared);
	(void)sem_close(s_sem);
	(void)sem_close(c_r_sem);
	(void)sem_close(c_w_sem);
	(void)shm_unlink(SHM_NAME);	
	(void)sem_unlink(CLIENT_READ_SEM);
	(void)sem_unlink(CLIENT_WRITE_SEM);
	(void)sem_unlink(SERVER_SEM);
	dumpdb();
	emptyList(db);
	emptyList(users);
	
}