#ifndef STORAGE_H_
#define STORAGE_H_

#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "../utils/utils.h"



typedef struct file_t{
    char*name;
    int dim;
    int lock;                     //fd del client che detiene la lock -1 se non è detenuta da nessuno
    int readCnt;                   // contatore di lettori del file
    pthread_mutex_t  modifyMutex;    // mutua esclusione nell'accesso alle strutture del file
    pthread_mutex_t  cntMutex;         //mutua esclusione sul contatore readCnt
    Client* lockReqs;             // lista dei client che hanno esplicitamente richiesto la lock
    Client* open;           //se il file è aperto contiene i fd di chi lo ha aperto
    void* buff;         //puntatore al buffer in memoria contenente il file
    struct file_t* nextFile;

}File;


int removeFd(int fd);

int storageInit(int c,int m);

int OpenInStorage(char* name, int dim, int flags, int fd);
int CloseInStorage(char* name, int fd);
int WriteInStorage(char* name, int dim, int flags, int fd);
int ReadFromStorage(char* name, int flags, int fd);
int LockInStorage(char* name, int fd);
int UnlockInStorage(char* name, int fd);
int DeleteFromStorage(char* name, int fd);

int freeStorage();

#endif