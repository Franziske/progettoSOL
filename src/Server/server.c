
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

        printf("\n RICEVUTO SEGNALE %d \n", sig);

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
    err = sigaddset(&mask, SIGHUP); 
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

    int initialFdmax = fdmax;

    while(termina <2) {
        // copio il set nella variabile temporanea per la select
        tmpset = set;

        printf("fdmax prima della select = %d\n", fdmax);
       
        
        err = select(fdmax+1, &tmpset, NULL, NULL, NULL);
        CHECKERRSC(err,-1, "Errore select: ");

        printf("postselect\n");
        //int i=0;

        // cerchiamo di capire da quale fd abbiamo ricevuto una richiesta
        for(int i=0; i <= fdmax; i++) {
        
            if(termina == 2 || termina == 3) break;

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
                    int sig;
                    read(signal_pipe[0], &sig, sizeof(int));
                    termina = sig;
                    
                    // non permetto nuove connessioni
                    FD_CLR(listenfd,&set);
                    close(listenfd);
                 

                    if(sig == 2 || sig == 3){
                        //chiudo le connessioni che non hanno richieste in sospeso
                        for(int j=0; j <= fdmax; j++){

                            if(FD_ISSET(j,&set)){
                                if(j != fds_pipe[0]){
                                    FD_CLR(j,&set);
                                    close(j);
                                }           
                            }
                        }
                    }
                    FD_CLR(signal_pipe[0],&set);
                    printf("\nterminando con sig: %d\n", termina);
                    terminationProtocol(pool, sig);

                    printf("chiamato protocollo di terminazione\n");
                    
                   // break;
                   continue;
                }

                if (i == fds_pipe[0]) {

                     printf("ricevuto fd da pipe %d \n", fds_pipe[0]);
                    // ricevuto sulla pipe un fd sul quale mi devo rimettere in ascolto 
                    int clientBack;
                    err = read(fds_pipe[0], &clientBack, sizeof(int));
                    
                    CHECKERRSC(err,-1,"Errore read da pipe: ");
                   

                    //se non devo terminare il server immediatamente reinserisco fd
                    if(termina != 2 && termina != 3 && clientBack > 0){
                        FD_SET(clientBack, &set);

                        if(clientBack > fdmax) fdmax = clientBack;

                    printf("Client con fd %d is back\n", clientBack);
                    continue;
                    }
                    
                    if(clientBack == -1){

                        printf("terminate le richieste in sospeso\n");
                        printf("fd max = %d, fd max iniz = %d termina = %d\n", fdmax, initialFdmax, termina);
                        //sono terminate le richieste che erano in sospeso 
                        //se ho chiusura immediata breaK
                        if(termina != 1)break;

                        //se tutti i client devono aver chiuso la connessione controllo i fd
                        if(fdmax <= initialFdmax){
                            termina = 2;
                            terminationProtocol(pool,2);
                            break;
                        }
                        //non ho più nessun client ne' fra le richieste in sospeso
                                            //ne' fra le connessioni in attesa di richieste

                        //non ho più richieste in sospeso ma altre connessioni non chiuse con dei client
                        continue;
                    }
                    //se devo terminare il server subito chiudo la connessione
                    close(clientBack);
                    continue;

                    
                }


                printf("Ricevuta richiesta fd %d \n", i);
                FD_CLR(i,&set);
                if(fdmax == i){
                    int newMax = 0;
                    for(int j=0; j<fdmax;j++){
                        if(FD_ISSET(j,&set) && j>newMax) newMax = j;
                        
                    }
                    printf("aggiorno fd max \n");
                    fdmax = newMax;
                }
               
                printf("aggiungendo fd alle richieste %d\n", i);
                int arg = i;
                
                int r =addToQueue(pool, arg);
                if (r==0){ // aggiunto con successo
                continue;
                }
                    
                fprintf(stderr, "Errore aggiungendo il fd %d alla coda delle richieste del thread pool\n", connfd);
                    continue;



            }
          
        }
    }
    
    printf("protocollo di terminazione\n");

    
    destroyThreadPool(pool);  // notifico che i thread dovranno uscire

    // aspetto la terminazione de signal handler thread
    pthread_join(sighandler_thread, NULL);

    unlink(SOCKNAME);    
    return 0;   


   
}