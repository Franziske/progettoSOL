
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/un.h> /* ind AF_UNIX */
//#include <bits/signum.h>
//#include <asm-generic/signal-defs.h>
#include <stddef.h>

#include "../../lib/ini/ini.h"
#include "../../lib/utils/utils.h"
#include "threadpool.h"
#define UNIX_PATH_MAX 108 /* man 7 unix */
#define SOCKNAME "./mysock"
#define N 26

 //capacita dello storage Mbytes
    int capacity = 0; 
  //numero massimo di files
    int max = 0;


typedef struct {
    sigset_t     *set;           /// set dei segnali da gestire (mascherati)
    int           signal_pipe;   /// descrittore di scrittura di una pipe senza nome
} sigHandler_t;

// funzione eseguita dal signal handler thread
static void *sigHandler(void *arg) {
    sigset_t *set = ((sigHandler_t*)arg)->set;
    int fd_pipe   = ((sigHandler_t*)arg)->signal_pipe;

    while(1) {
        int sig;
        int r = sigwait(set, &sig);
        if (r != 0) {
            errno = r;
            perror("Errore sigwait : ");
            return NULL;
        }

        switch(sig) {
        case SIGINT:
        case SIGQUIT:
        case SIGHUP:{
            write(fd_pipe, &sig, sizeof(int));
              close(fd_pipe);  // notifico il listener thread della ricezione del segnale
            return NULL;
        }
        default:  ; 
        }
    }
    return NULL;	   
}
//aggiunge la richiesta in coda

//rimuovo la richiesta in testa alla coda
int removeRequest(ServerRequest **list, ServerRequest **currReq)
{
   if (*list == NULL)
      return -1;
   *currReq = *list;
   (*currReq)->next = NULL;
   *list = (*list)->next;
   return 0;
}




int main(int argc, char const *argv[])
{

 
    //numero di Workers
    int n;

   int err;

    //controllo la validità degli argomenti
    if(argc != 2){
      fprintf(stderr, "Numero di argomenti non valido\n");
      exit(EXIT_FAILURE);
    }
    const char* configurationFileName = argv[1];

    //prendo il file di configurazione
    ini_t *config = ini_load(configurationFileName);

    const char *workersN = ini_get(config, NULL, "workersNumber");
    if (workersN) {
        printf("numero di workers : %s\n", workersN);

        //converto in intero e assegno a N
        n = stringToInt(workersN);
        
    }
    const char *c = ini_get(config, NULL, "storageCapacity");
    if (c) {
        printf("capacity: %s\n", c);

        //converto in intero e assegno a capacity
        capacity = stringToInt(c);

    }

    const char *m = ini_get(config, NULL, "maxFiles");
    if (m) {
        printf("max number of file: %s\n", m);

        //converto in intero e assegno a max
        max = stringToInt(m);
    }
   ini_free(config);
   printf("inizializzato\n");

////////////////////////////////////////////////////////////
    sigset_t mask;
    err = sigemptyset(&mask);
    CHECKERRSC(err,-1,"Errore sigemptyset: ");
    err = sigaddset(&mask, SIGINT); 
    CHECKERRSC(err,-1,"Errore sigaddset: ");
    err = sigaddset(&mask, SIGQUIT); 
    CHECKERRSC(err,-1,"Errore sigaddset: ");
    err = sigaddset(&mask, SIGTERM); 
    CHECKERRSC(err,-1,"Errore sigaddset: ");

    err = pthread_sigmask(SIG_BLOCK, &mask,NULL);
   CHECKERRNE(err,0,"Errore pthread_sigmask: ");

    // ignoro SIGPIPE per evitare di essere terminato da una scrittura su un socket
    struct sigaction s;
    memset(&s,0,sizeof(s));    
    s.sa_handler=SIG_IGN;

   err = sigaction(SIGPIPE,&s,NULL);
   CHECKERRSC(err,-1,"Errore sigaction: ");

    /*
     * La pipe viene utilizzata come canale di comunicazione tra il signal handler thread ed il 
     * thread lisener per notificare la terminazione. 
     * Una alternativa è quella di utilizzare la chiamata di sistema 
     * 'signalfd' ma non e' POSIX.
     *
     */
    int signal_pipe[2];
   err = pipe(signal_pipe);
   CHECKERRSC(err,-1,"Errore pipe: ");
   
    
    pthread_t sighandler_thread;
    sigHandler_t handlerArgs = { &mask, signal_pipe[1] };
   
    err = pthread_create(&sighandler_thread, NULL, sigHandler, &handlerArgs);
    CHECKERRNE(err,0,"Errore nella creazione del signal handler: ")
    

   int listenfd;
   listenfd= socket(AF_UNIX, SOCK_STREAM, 0);
   CHECKERRSC(listenfd, -1, "Errore socket: ");

   printf("listenfd: %d\n", listenfd);

   struct sockaddr_un sa;
   memset(&sa, '0', sizeof(sa));
   strncpy(sa.sun_path, SOCKNAME, strlen(SOCKNAME)+1);
   sa.sun_family = AF_UNIX;

   err = bind(listenfd, (struct sockaddr*)&sa,sizeof(sa));
   CHECKERRSC(err, -1, "Errore bind: ");

   err = listen(listenfd, SOMAXCONN);
   CHECKERRSC(err, -1, "Errore listen: ");


 Threadpool* pool = NULL;

    pool = createThreadPool(n); 

    CHECKERRE(pool, NULL, "Errore nella creazione del pool di thread");
    
    fd_set set, tmpset;
    FD_ZERO(&set);
    FD_ZERO(&tmpset);

    FD_SET(listenfd, &set);        // aggiungo il listener fd al master set
    FD_SET(signal_pipe[0], &set);  // aggiungo il descrittore di lettura della signal_pipe
    
    // tengo traccia del file descriptor con id piu' grande
    int fdmax = (listenfd > signal_pipe[0]) ? listenfd : signal_pipe[0];

    volatile int termina = 0;
    while(!termina) {
	// copio il set nella variabile temporanea per la select
	tmpset = set;
	err = select(fdmax+1, &tmpset, NULL, NULL, NULL);
    CHECKERRSC(err,-1, "Errore select: ");

	// cerchiamo di capire da quale fd abbiamo ricevuto una richiesta
	for(int i=0; i <= fdmax; i++) {
	    if (FD_ISSET(i, &tmpset)) {
		int connfd;

		if (i == listenfd) { // e' una nuova richiesta di connessione 
		    connfd = accept(listenfd, (struct sockaddr*)NULL ,NULL);
          CHECKERRSC(connfd, -1, "Errore accept");

		    /*int* args = malloc(2*sizeof(int));
		    if (!args) {
		      perror("FATAL ERROR 'malloc'");
		      goto _exit;
		    }
		    args[0] = connfd;
		    args[1] = (int)&termina;
		    
		    int r =addToQueue(pool, args);
		    if (r==0) continue; // aggiunto con successo
		    if (r<0) { // errore interno
			fprintf(stderr, "FATAL ERROR, adding to the thread pool\n");
		    } else { // coda dei pendenti piena
			fprintf(stderr, "SERVER TOO BUSY\n");
		    }
		    free(args);*/
		    close(connfd);
		    continue;
		}
		if (i == signal_pipe[0]) {
		    // ricevuto un segnale, esco ed inizio il protocollo di terminazione
            //controlla il tipo di terminazione
            int pTerm;
            read(signal_pipe[0], &pTerm, sizeof(int));
            termina = pTerm;
		    printf("terminando con sig: %d\n", termina);
		    break;
		}
	    }
	}
    }
    
    destroyThreadPool(pool, termina);  // notifico che i thread dovranno uscire

    // aspetto la terminazione de signal handler thread
    pthread_join(sighandler_thread, NULL);

    unlink(SOCKNAME);    
    return 0;   










   /*void *buffer = malloc(N);
   CHECKERRNE(buffer, NULL, "Errore malloc: ");
   

   int fd_skt, fd_c, err;
   struct sockaddr_un sa;
   strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
   sa.sun_family = AF_UNIX;

   //int justRead;
   // int bytesRead = 0;

   fd_skt = socket(AF_UNIX, SOCK_STREAM, 0);
   CHECKERRSC(fd_skt, -1, "Errore socket: ");
   printf("fd skt: %d\n", fd_skt);
   err = bind(fd_skt, (struct sockaddr *)&sa, sizeof(sa));
   CHECKERRSC(err, -1, "Errore bind: ");
   err = listen(fd_skt, SOMAXCONN);
   CHECKERRSC(err, -1, "Errore listen: ");
   fd_c = accept(fd_skt, NULL, 0);
   CHECKERRSC(err, -1, "Errore accept: ");

   printf("conness accettata\n");

   /// prima si bloccava qua prova a vedere senza printf

   //inizializzo la coda delle richieste
   ServerRequest *reqs = NULL;

   ServerRequest *req = malloc(sizeof(ServerRequest));

   //leggo la richiesta
   readRequest(fd_c, req);

   //controllo che la richiesta non sia di chiusura

   if (req->op == Close)
   {
      //rimuovi file descriptor del client e chiude la connessione
   }
   else{

      //aggiungo la richiesta alla coda
      addRequest(&reqs, req);

      printf("req: %d\n", req->op);
      printf("req: %d\n", req->dim);
      printf("req: %d\n", req->flags);
      printf("req: %d\n", req->nameLenght);
      printf("req: %s\n", req->fileName);

      // che verrà processata poi dai thread worker
   }
   err = readn(fd_c, buffer, N);
   CHECKERRE(err, -1, "readn failed");

   writen(fd_c, buffer, N);

   freeRequests(&reqs);


   close(fd_skt);
   free(buffer);
   //free(name);

   return 0;*/
}