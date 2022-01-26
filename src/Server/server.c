
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
#include "../../lib/storage/storage.h"
#include "threadpool.h"
#define UNIX_PATH_MAX 108 /* man 7 unix */
#define SOCKNAME "./mysock"
#define N 26

 //capacita dello storage Mbytes
    int capacity = 0; 
  //numero massimo di files
    int max = 0;

//terminazione 
    int termina=0;


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
        printf("pre sigwait");
        int r = sigwait(set, &sig);
            printf("post sigwait");
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
        printf("numero di workers : %d\n", n);
        
    }
    const char *c = ini_get(config, NULL, "storageCapacity");
    if (c) {
        printf("capacity: %s\n", c);

        //converto in intero e assegno a capacity
        capacity = stringToInt(c);
        printf("capacity: %d\n", capacity);

    }

    const char *m = ini_get(config, NULL, "maxFiles");
    if (m) {
        printf("max number of file: %s\n", m);

        //converto in intero e assegno a max
        max = stringToInt(m);
         printf("max number of file: %d\n", max);
    }
   ini_free(config);

    if(storageInit(capacity,max) != 0){

        //termino //TODO
        exit(EXIT_FAILURE);
    }

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

    // ignoro SIGPIPE per evitare di essere terminato da una scrittura
    struct sigaction s;
    memset(&s,0,sizeof(s));    
    s.sa_handler = SIG_IGN;

   err = sigaction(SIGPIPE,&s,NULL);
   CHECKERRSC(err,-1,"Errore sigaction: ");

    /*
    * La pipe viene utilizzata come canale di comunicazione tra il signal handler thread ed il 
    * thread listener 
    */
    int signal_pipe[2];
   err = pipe(signal_pipe);
   CHECKERRSC(err,-1,"Errore pipe: ");

   //pipe utilizzata come comunicazione fra workers e master

   int fds_pipe[2];
   err = pipe(fds_pipe);
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

    printf("listenfd: %d\n", listenfd);
    Threadpool* pool = NULL;

    pool = createThreadPool(n, fds_pipe[1]); 
    // close(fds_pipe[1]);

    CHECKERRE(pool, NULL, "Errore nella creazione del pool di thread");
    printf("creato thread pool\n");
    


    fd_set set, tmpset;
    FD_ZERO(&set);
    FD_ZERO(&tmpset);

    FD_SET(listenfd, &set);        // aggiungo il listener fd al master set
    FD_SET(signal_pipe[0], &set);  // aggiungo il descrittore di lettura della signal_pipe
    FD_SET(fds_pipe[0], &set);   //aggiungo il fd della fds_pipe

    // tengo traccia del file descriptor con id piu' grande
    int fdmax = (listenfd > signal_pipe[0]) ? listenfd : signal_pipe[0];

    fdmax = (fdmax > fds_pipe[0]) ? fdmax : fds_pipe[0];

    //volatile int termina = 0;
    while(!termina) {
        // copio il set nella variabile temporanea per la select
        tmpset = set;

        printf("--->fd pipe %d %d\n", fds_pipe[0],fds_pipe[1]);
        printf("pre select\n");

        printf("fdmax prima della select = %d\n", fdmax);

        
        err = select(fdmax+1, &tmpset, NULL, NULL, NULL);




        printf("%d err \n",err);
        CHECKERRSC(err,-1, "Errore select: ");

        printf("postselect\n");
        //int i=0;

        // cerchiamo di capire da quale fd abbiamo ricevuto una richiesta
        for(int i=0; i <= fdmax; i++) {
        //while(i<=fdmax){
            int isset = FD_ISSET(i, &tmpset);
            
            if (isset) {
                int connfd;
                printf("fd settato = %d \n", i);

                if (i == listenfd) { 
                    // e' una nuova richiesta di connessione 
                    connfd = accept(listenfd, (struct sockaddr*)NULL ,NULL);
                    CHECKERRSC(connfd, -1, "Errore accept");
                    printf("ho ricevuto richiesta di connessione da fd: %d\n", connfd);
                    FD_SET(connfd, &set); 
                    if(connfd > fdmax) fdmax = connfd;

                    /*int* args = malloc(2*sizeof(int));
                    if (!args) {
                    perror("Errore malloc");
                    destroyThreadPool;
                    unlink(SOCKNAME);
                    return -1;
                    }
                    args[0] = connfd;
                    args[1] = (int)&termina;
                    
                    int r =addToQueue(pool, args);
                    if (r==0) continue; // aggiunto con successo
                    fprintf(stderr, "Errore aggiungendo il fd %d alla coda delle richieste del thread pool\n", connfd);
                    
                    free(args);
                    close(connfd);*/
                
                    continue;
                }
                if (i == signal_pipe[0]) {
                    // ricevuto un segnale, esco ed inizio il protocollo di terminazione
                    //controlla il tipo di terminazione
                    int pTerm;
                    read(signal_pipe[0], &pTerm, sizeof(int));
                    termina = pTerm;
                    printf("\nterminando con sig: %d\n", termina);
                    break;
                }

                 if (i == fds_pipe[0]) {

                     printf("ricevuto fd da pipe %d \n", fds_pipe[0]);
                    // ricevuto un sulla pipe un fd sul quale mi devo rimettere in ascolto 
                    int clientBack;
                    err = read(fds_pipe[0], &clientBack, sizeof(int));
                    
                    CHECKERRSC(err,-1,"Errore read da pipe: ");
                    //if(clientBack == 0 );
                    FD_SET(clientBack, &set);
                    if(clientBack > fdmax) fdmax = clientBack;

                    printf("Client con fd %d is back\n", clientBack);

                    continue;
                }


                printf("Ricevuta richiesta fd %d \n", i);
                FD_CLR(i,&set);
                if(fdmax == i){
                    printf("decremento fd max \n");
                    fdmax--;
                }
                /*int* args = malloc(1*sizeof(int));
                    if (!args) {
                    perror("Errore malloc");
                    destroyThreadPool;
                    unlink(SOCKNAME);
                    return -1;
                    }
                    args[0] = i;
                    //args[1] = (int)&termina;*/
                    printf("aggiungendo fd alle richieste %d\n", i);
                    int arg = i;
                    
                    int r =addToQueue(pool, arg);
                    if (r==0){ // aggiunto con successo

                
                    continue;
                    }
                    
                    fprintf(stderr, "Errore aggiungendo il fd %d alla coda delle richieste del thread pool\n", connfd);

                   
                    
                    //free(args);
                    //close(connfd);
            
                //un client ha inviato una richiesta
                /*int* args = malloc(2*sizeof(int));
                if (!args) {
                perror("Errore malloc");
                destroyThreadPool;
                unlink(SOCKNAME);
                return -1;
                }
                args[0] = connfd;
                args[1] = (int)&termina;
                
                int r =addToQueue(pool, args);
                if (r==0) continue; // aggiunto con successo
                fprintf(stderr, "Errore aggiungendo il fd %d alla coda delle richieste del thread pool\n", connfd);
                
                free(args);
                close(connfd);*/
                continue;

            }
            //i++;
        }
    }
    printf("protocollo di terminazione\n");
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