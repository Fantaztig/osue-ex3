#include "client.h"
/**
 * @file client.c
 * @author David Schröder 1226747
 * @brief Client for auth exercise
 * @details Either tries to register or login a client with the server.
 * @date 08.01.2017
 */
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
 * @brief same as strcpy but adds tailing null byte after size-1 chars
 * @param dest string to copy to
 * @param source string to copy from
 * @param size maximum size including null byte
 */
static void mystrcpy(char *dest, char *source, int size);

 /**
 * @brief allocates needed ressources
 */
static void allocate_ressources(void);

 /**
 * @brief tries to free all ressources
 */
static void free_ressources(void);

 /**
 * @brief waits for semaphore and exits gracefully in case of signal
 * @param sem semaphore to wait on
 * @param description semaphore description that is printed in case of error
 */
static void wait_for_sem(sem_t *sem, char *description);

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
 * shared memory for communication with the server
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

static int mode;
static char login[20];
static char pass[20];
static int session;

/**
 * @brief Program entry point
 * @param argc The argument counter
 * @param argv The argument vector
 * @details tries to connect to the server with the given username and password
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
	parse_args(argc,argv);
	if(strlen(argv[optind])>19||strlen(argv[optind+1])>19){
		bailout(EXIT_FAILURE,"Username and password must be max 19 characters!");
	}
	
	mystrcpy(login,argv[optind],20);
	mystrcpy(pass,argv[optind+1],20);

	allocate_ressources();
	
	
	switch(mode){
		case REGISTER:{
			wait_for_sem(c_w_sem,"client write sem");
			memset(shared, 0, sizeof(MyShm));
			shared->state = 0;
			mystrcpy(shared->login,login,20);
			mystrcpy(shared->pass,pass,20);
			shared->command=REGISTER;
			int c = sem_post(s_sem);
			if(c!= 0){
				bailout(EXIT_FAILURE,"server semaphore error");
			}
			wait_for_sem(c_r_sem,"client read sem");
			if(shared->state==0){
				c = sem_post(c_w_sem);
				if(c!= 0){
					bailout(EXIT_FAILURE,"client write semaphore error");
				}
				bailout(EXIT_SUCCESS,"success");
			}else{
				c = sem_post(c_w_sem);
				if(c!= 0){
					bailout(EXIT_FAILURE,"client write semaphore error");
				}
				bailout(EXIT_FAILURE,"registration failed");
			}
		}
		break;
		case LOGIN:{
			wait_for_sem(c_w_sem,"client write sem");
			memset(shared, 0, sizeof(MyShm));
			shared->state = 0;
			mystrcpy(shared->login,login,20);
			mystrcpy(shared->pass,pass,20);
			shared->command=LOGIN;
			int c = sem_post(s_sem);
			if(c!= 0){
				bailout(EXIT_FAILURE,"server semaphore error");
			}
			wait_for_sem(c_r_sem,"client read sem");
			if(shared->state==0){
				session = shared->sessId;
				(void)fprintf(stdout,"logged in with id %d\n",session);
				memset(shared, 0, sizeof(MyShm));
				shared->state = 0;
				
				c = sem_post(c_w_sem);
				if(c!= 0){
					bailout(EXIT_FAILURE,"client write semaphore error");
				}
			}else{
				c = sem_post(c_w_sem);
				if(c!= 0){
					bailout(EXIT_FAILURE,"client write semaphore error");
				}
				bailout(EXIT_FAILURE,"login failed");
			}
		while(!quit){
			(void)fprintf(stdout,"%s\n%s\n%s\n%s\n%s"
			,"Commands:"
			,"  1) write secret"
			,"  2) read secret"
			,"  3) logout"
			,"Please select a command (1-3):");
			int myInt;
			int result = scanf("%d", &myInt);

			if (result == EOF) {
				(void)fprintf(stdout,"%s\n","please enter a number");
			}
			if (result == 0) {
				while (fgetc(stdin) != '\n')
					;
				(void)fprintf(stdout,"%s\n","please enter a number");
			}
			if(result == 1){
				if(fgetc(stdin) != '\n'){
					while (fgetc(stdin) != '\n')
					;
					(void)fprintf(stdout,"%s\n","please enter a number");
				}else{
					switch(myInt){
						case 1:{
						//TODO enter secret
						int accepted = 0;
						char mysecret[50];
						while(!accepted){
							memset(&mysecret[0], 0, sizeof(mysecret));
							(void)fprintf(stdout,"%s","Please enter your new secret:");
							char ch;
							int count = 0;
							while ((ch = fgetc(stdin)) != '\n'){
								if(count <49){
									mysecret[count] = ch;
								}
								count++;
							}
							if(count > 49){
								(void)fprintf(stdout,"%s","Your secret must be maximum 49 characters long!\n");
							}else{
								accepted = 1;
								mysecret[count] = '\0';
							}
						}
						
						wait_for_sem(c_w_sem,"client write sem");
						mystrcpy(shared->login,login,20);
						mystrcpy(shared->secret,mysecret,50);
						shared->command=WRITE_SECRET;
						shared->sessId = session;
						
						c = sem_post(s_sem);
						if(c!= 0){
							bailout(EXIT_FAILURE,"server semaphore error");
						}
						wait_for_sem(c_r_sem,"client read sem");
						if(shared->state==0){
							(void)fprintf(stdout,"successfully wrote secret\n");
							c = sem_post(c_w_sem);
							if(c!= 0){
								bailout(EXIT_FAILURE,"client write semaphore error");
							}
						}else{
							c = sem_post(c_w_sem);
							if(c!= 0){
								bailout(EXIT_FAILURE,"client write semaphore error");
							}
							bailout(EXIT_FAILURE,"");
						}
						}
						break;
						case 2:
						wait_for_sem(c_w_sem,"client write sem");
						mystrcpy(shared->login,login,20);
						shared->command=READ_SECRET;
						shared->sessId = session;
						c = sem_post(s_sem);
						if(c!= 0){
							bailout(EXIT_FAILURE,"server semaphore error");
						}
						wait_for_sem(c_r_sem,"client read sem");
						if(shared->state==0){
							char secret[50];
							mystrcpy(secret,shared->secret,50);
							(void)fprintf(stdout,"Your secret is: %s\n",secret);
							c = sem_post(c_w_sem);
							if(c!= 0){
								bailout(EXIT_FAILURE,"client write semaphore error");
							}
						}else{
							c = sem_post(c_w_sem);
							if(c!= 0){
								bailout(EXIT_FAILURE,"client write semaphore error");
							}
							bailout(EXIT_FAILURE,"Server returned an error");
						}
						break;
						case 3:
						wait_for_sem(c_w_sem,"client write sem");
						mystrcpy(shared->login,login,19);
						shared->command=LOGOUT;
						shared->sessId = session;
						c = sem_post(s_sem);
						if(c!= 0){
							bailout(EXIT_FAILURE,"server semaphore error");
						}
						wait_for_sem(c_r_sem,"client read sem");
						if(shared->state==0){
							c = sem_post(c_w_sem);
							if(c!= 0){
								bailout(EXIT_FAILURE,"client write semaphore error");
							}
							bailout(EXIT_SUCCESS,"logged out");
						}else{
							c = sem_post(c_w_sem);
							if(c!= 0){
								bailout(EXIT_FAILURE,"client write semaphore error");
							}
							bailout(EXIT_FAILURE,"logout failed");
						}
						break;
						default:
						(void)fprintf(stdout,"%s\n","please enter a number from 1-3");
						break;
					}
				}
			}
		}
		}
		break;
		default:
		assert(0);
	}
	
	
	
	
	
	
	
	
}

static void handle_signal(int signo){
  if (signo == SIGINT){
	  quit = 1;
  }
  
  if (signo == SIGTERM){
	  quit = 1;
  }
   
}

static void mystrcpy(char *dest,char *source,int size){
	strncpy(dest,source,size-1);
	dest[size]='\0';
}


static void parse_args(int argc, char **argv){
	myname = argv[0];
	int c;
	int i = 0;
	while ((c = getopt(argc, argv, "lr")) != -1){
		switch(c){
			case 'l':
				if(i == 0){
					i++;
					mode = LOGIN;
				}else{
					bailout(EXIT_FAILURE,"usage auth-client { -r | -l } username password");
				}
				break;
			case 'r':
				if(i == 0){
					i++;
					mode = REGISTER;
				}else{
					bailout(EXIT_FAILURE,"usage auth-client { -r | -l } username password");
				}
				break;
			case '?':
				bailout(EXIT_FAILURE,"usage auth-client { -r | -l } username password");
			default:
				assert(0);
				break;
		}
	}
	if((optind+1>=argc)||i==0){
		bailout(EXIT_FAILURE,"usage auth-client { -r | -l } username password");
	}
	
}

static void allocate_ressources(void){
	//initialize shared memory
	shmfd =shm_open(SHM_NAME, O_RDWR, PERMISSION);
	if(shmfd==-1){
		bailout(EXIT_FAILURE,"couldnt open shared memory");
	}
	if(ftruncate(shmfd, sizeof *shared) == -1){
		bailout(EXIT_FAILURE,"couldnt set the size of shared memory");
	}
	shared = mmap(NULL, sizeof *shared, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
	if(shared == MAP_FAILED){
		bailout(EXIT_FAILURE,"couldnt map shared memory");
	}
	if(shared == NULL){
		bailout(EXIT_FAILURE,"couldnt malloc shm");
	}
	
	//initialize semaphors
	s_sem = sem_open(SERVER_SEM, 0);
	if(s_sem == SEM_FAILED){
		bailout(EXIT_FAILURE,"creating sem1 failed!");
	}
	c_r_sem = sem_open(CLIENT_READ_SEM, 0);
	if(c_r_sem == SEM_FAILED){
		bailout(EXIT_FAILURE,"creating sem2 failed!");
	}
	c_w_sem = sem_open(CLIENT_WRITE_SEM, 0);
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
	if(shared->state==-1){
		(void)sem_post(c_w_sem);
		bailout(EXIT_SUCCESS,"server has shut down, closing");
	}
}

static void bailout(int exitcode, const char *errmsg){
	(void)fprintf(stderr,"%s %s\n",myname,errmsg);
	exit(exitcode);
}

static void free_ressources(void){
	(void)close(shmfd);
	(void)munmap(shared, sizeof *shared);
	(void)sem_close(s_sem);
	(void)sem_close(c_r_sem);
	(void)sem_close(c_w_sem);
	
}
