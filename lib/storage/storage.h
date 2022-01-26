#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "../utils/utils.h"



typedef struct file_t{
    char*name;
    char*dim;
    int lock;                     //fd del client che detiene la lock -1 se non è detenuta da nessuno
    //pthread_mutex_t  lockFile;    // mutua esclusione nell'accesso al file
    //pthread_cond_t   condFile;
    Client* lockReqs;             // lista dei client che hanno esplicitamente richiesto la lock
    Client* open;           //se il file è aperto contiene i fd di chi lo ha aperto
    void* buff;         //puntatore al buffer in memoria contenente il file
    struct file_t* nextFile;

}File;



int OpenInStorage();
int CloseInStorage();
int WriteInStorage();
int ReadFromStorage();
int LockInStorage();
int DeleteFromStorage();
int UnlockInStorage();