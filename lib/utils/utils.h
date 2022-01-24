#ifndef UTILS_H
#define UTILS_H
#include <errno.h>
#include <stdio.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>


//possibili operazioni lette dal client
typedef enum
{
   Write,
   Read,
   Lock,
   Unlock,
   Cancel,
   Close
} Operation;

//possibili operazioni lette dal server

typedef enum
{
    OF,
    CF,
    W,
    R,
    L,
    U,
    CC

} serverOperation;

//struttura utilizzata per costruire liste di stringhe
typedef struct string_t {  
    char* name;
    struct string_t* nextString;
} string;

typedef struct client_t{
    int fdC;
    struct client_t* nextC;
}Client;


//Struttura contenente le info necessarie al server relative ad una richiesta
typedef struct serverRequest_t
{
   serverOperation op;               // quale operazione è stata richiesta
   size_t dim;                 // dimensione file
   size_t nameLenght;          // lunghezza del nome con cui memorizzare il file
   char *fileName;             //nome del file associato alla richiesta
   int client;                 //fd client che l'ha richiesta
   int flags;                
   struct serverRequest_t *next; //puntatore alla richiesta successiva NULL se non ce ne sono
} ServerRequest;


// con le macro ho libertà nei tipi

// se il parametro var ha valore value ottengo l'eerore errString
#define NOTEQUAL(var, value, errString) \
    if(var == value){      \
         \
        perror(errString); \
        exit(errno);       \
    }


/** Evita letture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la lettura da fd leggo EOF
 *   \retval size se termina con successo
 */
static inline int readn(long fd, void* buf, size_t size) {
    size_t left = size;
    int r;
    char* bufptr = (char*)buf;
    while (left > 0) {
        if ((r = read((int)fd, bufptr, left)) == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return 0;  // EOF
        left -= r;
        bufptr += r;
    }
    return size;
}

/** Evita scritture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la scrittura la write ritorna 0
 *   \retval  1   se la scrittura termina con successo
 */
static inline int writen(long fd, void* buf, size_t size) {
    size_t left = size;
    int r;
    char* bufptr = (char*)buf;
    while (left > 0) {
        if ((r = write((int)fd, bufptr, left)) == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return 0;
        left -= r;
        bufptr += r;
    }
    return 1;
}

Client* addRequest(Client **list, Client *newReq);


// funzioni utilizzate per gestire liste di stringhe
void freeStringList(string** list);
void addToList(string** list, string* f);
int removeHead(string** list);


int listFileInDir(char *dirName, int n, string **list);
int stringToInt(const char* s);

// con le macro ho libertà nei tipi

// se il parametro var ha valore value ottengo l'errore errString
#define CHECKERRSC(var, value, errString) \
    if(var == value){      \
         \
        perror(errString); \
        exit(errno);       \
    }


#define CHECKERRE(var,value,errString)\
     if(var == value){      \
         \
        fprintf(stderr, errString); \
        exit(EXIT_FAILURE);      \
    }

#define CHECKERRNE(var,value,errString)\
     if(var != value){      \
         \
        fprintf(stderr, errString); \
        exit(EXIT_FAILURE);      \
    }
#define CHECKRES(var,value)\
     if(var != value){      \
         \
        printf(stdout, "Richiesta andata a buon fine \n"); \
        exit(EXIT_FAILURE);      \
    }
// dal professore
/*
#define SYSCALL_EXIT(name, r, sc, str, ...)	\
    if ((r=sc) == -1) {				\
	perror(#name);				\
	int errno_copy = errno;			\
	print_error(str, __VA_ARGS__);		\
	exit(errno_copy);			\
    }

#define SYSCALL_PRINT(name, r, sc, str, ...)	\
    if ((r=sc) == -1) {				\
	perror(#name);				\
	int errno_copy = errno;			\
	print_error(str, __VA_ARGS__);		\
	errno = errno_copy;			\
    }

#define SYSCALL_RETURN(name, r, sc, str, ...)	\
    if ((r=sc) == -1) {				\
	perror(#name);				\
	int errno_copy = errno;			\
	print_error(str, __VA_ARGS__);		\
	errno = errno_copy;			\
	return r;                               \
    }

#define CHECK_EQ_EXIT(name, X, val, str, ...)	\
    if ((X)==val) {				\
        perror(#name);				\
	int errno_copy = errno;			\
	print_error(str, __VA_ARGS__);		\
	exit(errno_copy);			\
    }

#define CHECK_NEQ_EXIT(name, X, val, str, ...)	\
    if ((X)!=val) {				\
        perror(#name);				\
	int errno_copy = errno;			\
	print_error(str, __VA_ARGS__);		\
	exit(errno_copy);			\
    }

//controlla pthread mutex lock

#define LOCK(l)      if (pthread_mutex_lock(l)!=0)        { \
    fprintf(stderr, "ERRORE FATALE lock\n");		    \
    pthread_exit((void*)EXIT_FAILURE);			    \
  }   
#define LOCK_RETURN(l, r)  if (pthread_mutex_lock(l)!=0)        {	\
    fprintf(stderr, "ERRORE FATALE lock\n");				\
    return r;								\
  }   

#define UNLOCK(l)    if (pthread_mutex_unlock(l)!=0)      {	    \
    fprintf(stderr, "ERRORE FATALE unlock\n");			    \
    pthread_exit((void*)EXIT_FAILURE);				    \
  }
#define UNLOCK_RETURN(l,r)    if (pthread_mutex_unlock(l)!=0)      {	\
    fprintf(stderr, "ERRORE FATALE unlock\n");				\
    return r;								\
  }
#define WAIT(c,l)    if (pthread_cond_wait(c,l)!=0)       {	    \
    fprintf(stderr, "ERRORE FATALE wait\n");			    \
    pthread_exit((void*)EXIT_FAILURE);				    \
  }
  */
 #endif