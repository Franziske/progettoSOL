
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <dirent.h>
#include <sys/un.h> /* ind AF_UNIX */
#include <time.h>
#include <math.h>
#include <inttypes.h>
#include <getopt.h>
#include <stdbool.h>

#include "../../lib/utils/utils.h"
#include "../../lib/api/api.h"

#define UNIX_PATH_MAX 108 /* man 7 unix */
#define N 10


void help(){
    printf("comando help\n");
}

typedef struct Request{
    Operation op;       // quale operazione è stata richiesta
    size_t n;         // rappresenta la "n" nell'API ovvero il numero di file da leggere o scrivere
    char* dirFrom;        //cartella dalla quale prendere i file da inviare al server
    char* dirTo;      //cartella nel quale salvare i file ricevuti dal server 
    string* files;   //lista di file sui quali eseguire l'operazione                                             
    size_t filesNumber;
    struct Request* next;  //operazione successiva, vale NULL se non ce ne sono
} Request;

size_t getList(const char* str, string** list){
    char* aux = malloc(strlen(str)*sizeof(char)+1);
    strcpy(aux,str);
    char* subString;
    string* tail = *list;

    if(tail !=NULL)
        while(tail->nextString != NULL) tail = tail->nextString;

    size_t subStrNumber = 0;
     while ((subString = strtok_r(aux, ",", &aux))!= NULL) {
         printf("size of string %ld\n", strlen(subString));
        printf("stringa letta : %s\n ", subString);
       string* f = malloc(sizeof(string));
       printf("size of string %ld\n\n", strlen(subString));
       f->name = malloc(strlen(subString)*sizeof(char)+1);
       strcpy(f->name,subString);
       f->nextString = NULL;

       if(tail == NULL){
           *list = f;
           tail = f;
       }
       else{
           tail->nextString = f;
           tail = f;
       }
    
       subStrNumber ++;
     }

    return subStrNumber;

}
//controllo che l'ultima richiesta aggiunta in coda sia una read/write
//restituisce NULL se falso, altrimenti il puntatore all'ultimo elemento

Request* checkLastOp(Request* list, Operation op){
    if(list == NULL) return NULL;
    Request* aux = list;
    while(aux->next != NULL) aux = aux->next;
    if(aux->op != op) return NULL;

    return aux;
}


void addRequest(Request** list, Request* newReq){

    printf("aggiungendo la richiesta\n");

    if( (*list) == NULL){
        *list = newReq;
    }
    else{
        Request* aux = (*list);

        while (aux->next != NULL){
            aux = aux->next;
        }
        aux->next = newReq;
    }

}


void freeRequests(Request** list){
    Request*aux;
    while(*list != NULL){
        aux = *list;
        *list = aux->next;
        if(aux->dirFrom != NULL)free(aux->dirFrom);
        if(aux->dirTo != NULL)free(aux->dirTo);
        if(aux->files != NULL)freeStringList(&(aux->files));
        free(aux);
    }
}

int main(int argc, char* const* argv){

   FILE *ifp;
   void *buffer;
   //int justRead = 0;
   int bytesRead = 0;
   int msec = 500;
   struct timespec spec;
    int opt;
    //flags per vedre se ho ricevuto -p,-f,-h
    bool print = 0;      //devo stampare le info relative ad una richiesta?
    bool socketInzialized = 0;       //ho già inzializzato la socket?
    bool helpRequested = 0;      //ho già ricevuto il comando help?
    size_t timeout = 0;        //intervallo fra richieste //err, timeout = 0, msec = 10;
  
  //controllo che ci siano opzioni altrimenti termino fallendo
     if (argc == 1) {
        //usage(argv[0]);
        fprintf(stderr,"numero non valido di opzioni\n");
        return EXIT_FAILURE;
    }

    // se ho argomenti a sufficienza
    static char* serverSocket = NULL;
    Request* reqs = NULL;       //lista in cui memorizzare le richieste

    Request* aux;

    // utilizzo getopt per fare il parsing come mostrato in man 3 getopt
    while ((opt = getopt(argc, argv, ":hpf:t:w:W:r:R::l:u:c:d:D:p")) != -1) {
        switch (opt) {
            case 'h':
                help();
                freeRequests(&reqs);
                //freeRequests(operations);
                return EXIT_SUCCESS;
            case 'p':
                // optional
                CHECKERRE(print, 1, "Richiesta mal formata:-p duplicato");
                print = 1;
                printf("stampe abilitate\n");
                break;
            case 'f':
                // required
                CHECKERRE(socketInzialized, 1, "Richiesta mal formata:-f duplicato");
                serverSocket = optarg;
                printf("server socket inizializzata: %s\n", serverSocket);
                socketInzialized = 1;
                break;
            case 't':{
                int t;
                t = atoi(optarg);
                if(t < 0){
                    fprintf(stderr, "Richiesta mal formata timeout negativo");
                    return EXIT_FAILURE;
                }
                timeout = (size_t) t;
                printf("timeout setted to: %ld millisec\n", timeout);
            }
                break;
            case 'w': {
                
                Request* req = malloc(sizeof(Request));
                
                char* args = optarg;
                req->op = Write;
                req->files = NULL;
                req->dirFrom = strtok_r(args, ",", &args);
                // controllo se è stato specificato un valore per n
                
                char* parsedN = strtok_r(args, ",", &args);
                if(parsedN!= NULL){
                    req->n = (size_t) atoi(parsedN);
                }
                else 
                  req->n = 0;
               req->dirTo = NULL;
               req->next = NULL;
               //aggiornao il numero di file e la list con i file
               req->filesNumber = listFileInDir(req->dirFrom,req->n,&(req->files));
               addRequest(&reqs, req);
               break;  
            }
            case 'W': {
                
                Request* req = malloc(sizeof(Request));
                req->op = Write;
                req->dirFrom = NULL;
                req->dirTo = NULL;
                req->files = NULL;
                //aggiorno il numero di file e la lista con i files
                req->filesNumber = getList(optarg, &(req->files)); 
                req->n = 0;
                req->next = NULL;
                addRequest(&reqs, req);
                break;
            }
            case 'R': {
                
                Request* req = malloc(sizeof(Request));

                printf("caso R\n");
                req->op = Read;
                req->files = NULL;
                req->filesNumber = 0;
                req->dirFrom = NULL;
                req->n = 0;
                // se presente faccio il parsing di n
              
                char* args = optarg;
                char*parsedN = strtok_r(args, ",", &args);
                if (parsedN != NULL && parsedN[0] == '='){
                req->n = atoi(parsedN + 1);
                }

                
                else{
                    fprintf(stderr, "richiestra read malformata ");
                    exit(EXIT_FAILURE);
                }

                /*strtok_r(optarg, ",", &optarg);
                if (optarg != NULL)
                     req->n =  atoi(optarg);*/

                    //req->n = strtok_r(optarg, ",", &optarg);
                req->dirTo = NULL;
                req->next = NULL;

                printf("n parsed = %d\n", req->n);
                addRequest(&reqs, req);
                break;
            }
            case 'r': {
                
                Request* req = malloc(sizeof(Request));
                req->op = Read;
                req->dirTo = NULL;
                req->dirFrom = NULL;
                req->files = NULL;
                //aggiorno il numero di file e la lista con i files
                req->filesNumber = getList(optarg, &(req->files)); 
                req->next = NULL;
                req->n = 0;
                addRequest(&reqs, req);
                break;
            }
            case 'l': {
                
                Request* req = malloc(sizeof(Request));
                req->op = Lock;
                req->dirTo = NULL;
                req->dirFrom = NULL;
                req->files = NULL;
                //aggiorno il numero di file e la lista con i files
                req->filesNumber = getList(optarg, &(req->files)); 
                req->next = NULL;
                req->n = 0;
                addRequest(&reqs, req);
                break;
            }
            case 'u': {
                
                Request* req = malloc(sizeof(Request));
                req->op = Unlock;
                req->dirTo = NULL;
                req->dirFrom = NULL;
                req->files = NULL;
                //aggiorno il numero di file e la lista con i files
                req->filesNumber = getList(optarg, &(req->files)); 
                req->next = NULL;
                req->n = 0;
                addRequest(&reqs, req);
                break;
            }
            case 'c': {
                
                Request* req = malloc(sizeof(Request));
                req->op = Cancel;
                req->dirTo = NULL;
                req->dirFrom = NULL;
                req->files = NULL;
                //aggiorno il numero di file e la lista con i files
                req->filesNumber = getList(optarg, &(req->files)); 
                req->next = NULL;
                req->n = 0;
                addRequest(&reqs, req);
                break;
            }
            case 'd':
                //controllo che -d sia preceduto da una richiesta di lettura
                aux = checkLastOp(reqs,Read);
                CHECKERRE(aux ,NULL,
                         "Richiesta mal formata: -d non preceduta da una lettura");
                aux->dirTo = optarg; ///STRINGCOPY?????

                break;
            case 'D':
                //controllo che -D sia preceduto da una richiesta di scrittura
                 aux = checkLastOp(reqs,Write);
                CHECKERRE(aux ,NULL,
                         "Richiesta mal formata: -D non preceduta da una scrittura");
                aux->dirTo = optarg; ///STRINGCOPY?????
                
                break;
            
            case ':':
                fprintf(stderr, "L'opzione richiede un argomento\n");
                //printf("Argument %s needs a value\n", argv[optind - 1]);
                break;
            case '?':
                fprintf(stderr, "l'opzione non è riconosciuta\n");
                //printf("Unknown option: %c\n", optopt);
                break;
            
                //printf("Default: %c %s\n", opt, optarg);
        }
    }
   //controllo che mi sia stat specificata una socket

    CHECKERRE(socketInzialized, 0, "socket non inizializzato");

    // se ho richieste da eseguire che necessitano della connessione con il server:
    //apro la connesione con il server
    aux = reqs;

    while(aux!=NULL){
        printf("richiesta: %d\n", aux->op);
        //printf("richiesta su : %s\n", aux->files->name);

        aux = aux->next;
        
    }
   

    if (reqs != NULL) { 
        // apro la connessione e prendo le richieste nella coda
        clock_gettime(CLOCK_REALTIME, &spec);
      spec.tv_sec  = spec.tv_sec + 10;   //tenterò la connessione per 10 secondi

      printf("due time: %ld nanoseconds \n",spec.tv_nsec);
      printf("due time: %ld seconds\n\n",spec.tv_sec);
      openConnection( serverSocket, msec, spec);
   //CONTROLLA ERRNO -1
        
    //SWITCH SUL TIPO DI OPERAZIONE

    //togli la richiesta dalla lista 

    int result;
    string*aux_p;
    
    while (reqs != NULL) {

        switch (reqs->op) {

            case Write:{

                //posso avere n invece che il nome del file
               // CHECKERRE(reqs->files,NULL,"Richiesta writeFile malformata\n");
                //CHECKERRE(reqs->files->name,NULL,"Richiesta writeFile malformata\n");
            
                result = 0;
                for (size_t i = 0; i < reqs->filesNumber; i++){  
                    aux_p = reqs->files;
                    int flags = 3;
                    result = 0;
                    result = openFile(aux_p->name, flags);
                    CHECKERRSC(result, -1, "Errore openFile: ");
                    //usleep(timeout *1000);
                    result = 0;
                    result = writeFile(aux_p->name, reqs->dirTo);
                    CHECKERRSC(result, -1, "Errore writeFile: ");
                    result = 0;
                    result = closeFile(aux_p->name);
                    CHECKERRSC(result, -1, "Errore closeFile: ");
                    aux_p = aux_p->nextString;
                }
            
                /*if(reqs->dirFrom != NULL){
                    // caso W


                }
                else{
                    //caso w
                }*/

            break;
            }

            case Read:{

                printf("Richiesta read\n");

                //CHECKERRE(reqs->files,NULL,"Richiesta readFile malformata");
                //CHECKERRE(reqs->files->name,NULL,"Richiesta readFile malformata");
                if(reqs->files == NULL && reqs->n <0){
                     fprintf(stderr, "richiestra read malformata ");
                    exit(EXIT_FAILURE);
                }
                if(reqs->files != NULL){
                    for (size_t i = 0; i < reqs->filesNumber; i++){  
                        result = 0;
                        aux_p = reqs->files;
                        int flags = 3;
                        void* buffer = NULL;
                        size_t dim;
                        result = openFile(aux_p->name, flags);
                        CHECKERRSC(result, -1, "Errore openFile: ");
                            result = 0;
                        result = readFile(aux_p->name, buffer, &dim);
                        CHECKERRSC(result, -1, "Errore readFile: ");
                            result = 0;
                        result = closeFile(aux_p->name);
                        CHECKERRSC(result, -1, "Errore closeFile: ");
                        aux_p= aux_p->nextString;
                    }
                }
                else{
                    if(reqs->n > 0){
                        printf("richiesta lettura n files\n");
                        readNFiles(reqs->n, reqs->dirTo);
                         CHECKERRSC(result, -1, "Errore readNFile: ");
                    }
                }

            break;
            }
            case Lock:{
                result = 0;
                CHECKERRE(reqs->files,NULL,"Richiesta lockFile malformata");
                CHECKERRE(reqs->files->name,NULL,"Richiesta lockFile malformata");
                result = lockFile(reqs->files->name);
                CHECKERRSC(result, -1, "Errore lockFile: ");
            break;
            }
            case Unlock:{
                result = 0;
                CHECKERRE(reqs->files,NULL,"Richiesta unlockFile malformata");
                CHECKERRE(reqs->files->name,NULL,"Richiesta unlockFile malformata");
                result = unlockFile(reqs->files->name);
                CHECKERRSC(result, -1, "Errore unlockFile: ");
            break;
            }
            case Cancel:{
                CHECKERRE(reqs->files,NULL,"Richiesta removeFile malformata");
                CHECKERRE(reqs->files->name,NULL,"Richiesta removeFile malformata");
                
                result = 0;
            for (size_t i = 0; i < reqs->filesNumber; i++){  
                aux_p = reqs->files;
                int flags = 3;
                result = 0;
                result = lockFile(aux_p->name);
                CHECKERRSC(result, -1, "Errore lockFile: ");
                result = 0;
                result = removeFile(reqs->files->name);
                CHECKERRSC(result, -1, "Errore removeFile: ");
                
                aux_p= aux_p->nextString;
            }
            break;
            }

            default:{
                fprintf(stderr, "richiesta mal formata");
                freeRequests(&reqs);
                return EXIT_FAILURE;
            }
           

        }
        //rimuovo la richiesta dalla coda
        Request* aux = reqs;
        reqs = reqs->next;
        free(aux);
    }
    freeRequests(&reqs);

    

    }

    printf("Terminando Client....");
    int res = closeConnection(serverSocket);
    CHECKERRSC(res, -1, "Errore closeConnection: ")
   

   /*clock_gettime(CLOCK_REALTIME, &spec);
    spec.tv_sec  = spec.tv_sec + 10;

    printf("due time: %ld nanoseconds \n",spec.tv_nsec);
   printf("due time: %ld seconds\n\n",spec.tv_sec);

   openConnection(SOCKNAME,msec,spec);

   //printf("Risultato openConnection fd skt = %d \n", fd_skt);

   /// terza versione
   // lettura da file
   char* fileName = malloc(45);
   strcpy(fileName,"/home/franc/Documents/ClientServer/prova.txt");

   bytesRead = getFile(&buffer, fileName);
   printf("----> bytes read %d \n", bytesRead);
   string* fileList = NULL; // = malloc(sizeof(string));
   int n = 0;
   char* dirName = malloc(27);
   strcpy(dirName,"/home/franc/Documents/dire");
   int filesRead = listFileInDir(dirName,n , &fileList);

   printf("file letti : %d  = \n", filesRead);

   string*aux = fileList;
   while (aux!= NULL){
   printf("nella lista di file ho :%s \n", aux->name);
   aux = aux->nextString;
   }
   printf("stampa finita\n");

   int flags = -1;
   int resSend = sendRequest(Lock, bytesRead, fileName, flags);

   printf("risultato send : %d\n", resSend);

   //printf("invio %s\n", (char*)buffer);
  // writen(fd_skt, buffer, bytesRead);
   sendFile(buffer,bytesRead);
   free(buffer);

  void *buffer1 = malloc(26);


  readn(3, buffer1, 26);

   FILE *file = fopen("back.txt", "w");
   CHECKERRSC(file, NULL, "fopen failed");

   fwrite(buffer1, 1, 26, file);

   fclose(file);

   free(buffer1);
   free(fileName);
  
   freeStringList(&fileList);



   // testo get list

   string* list = NULL;
   char* param = malloc(28);

   strcpy(param,"/ciao/1,/ciao/due,/ciao/tre");

   getList(param,&list);

   string* tmp = list;
   while(tmp!=NULL){
      printf("file letto : %s \n", tmp->name );
      tmp= tmp->nextString;
   }

   free(param);

   freeStringList(&list);*/

  // close(fd_skt);

   /*long            ns; // Milliseconds
   long          s;  // Seconds
   struct timespec spec;

   long            currns; // Milliseconds
   long          currs;  // Seconds
   struct timespec currSpec;

   //prendi il tempo subito dopo il primo tentativo
    clock_gettime(CLOCK_REALTIME, &spec);
   printf("due time: %ld seconds\n\n",spec.tv_sec);
    spec.tv_sec  = spec.tv_sec + 5;
    ns = spec.tv_nsec; // Convert nanoseconds to milliseconds

    printf("due time: %ld nanoseconds \n",ns);
   printf("due time: %ld seconds\n\n",spec.tv_sec);
   int msec = 500;

   //OpenConnection

    clock_gettime(CLOCK_REALTIME, &currSpec);

    while(currSpec.tv_sec < s){
         printf("Current time: %ld nanoseconds \n",currSpec.tv_nsec);
         printf("Current time: %ld seconds \n\n",currSpec.tv_sec);
         usleep((useconds_t)msec * (useconds_t)1000);
         clock_gettime(CLOCK_REALTIME, &currSpec);
         }

   printf("uscita dal while Current time: %ld nanoseconds \n",currSpec.tv_nsec);
   printf("uscita dal while il tempo Current time: %ld seconds\n",currSpec.tv_sec);
       */  

}