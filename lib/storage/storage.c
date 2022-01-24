#include <stdio.h>
#include <string.h>



#include "storage.h"


extern int capacity;
extern int max;

File* storageList = NULL;



File* findFile(char* nameF){

    if(storageList == NULL) return NULL;

    File* aux = storageList;

    while(aux!= NULL){
        if(strncmp(aux->name, nameF, (ssize_t)strlen(nameF)+1) == 0 ) return aux;
        aux = aux->nextFile;
    }

    return NULL;

}


//Ho una specifica funzione per ogni possibile operazione da effetuare sullo storage


int OpenInStorage(char* name, int dim, int flags, int fd){
    
    File* f = findFile(name);
    
    if(flags == 0){

        //O_LOCK e O_CREATE non sono settati
        //controllo che il file sia presente nello storage
        //altrimenti errore

        //ritorno la struttura file 
        
        if(f == NULL){
            return -1;
        }
        //f= malloc(sizeof(File));


    }
    if(flags > 2){
        //O_CREATE è settato
        //controlle che il file non esista già
        //altrimenti errore
        if(f != NULL){
            return -2;
        }
        f->name = name;
        f->dim = dim;
        f->lock = -1;
        f->nextFile = NULL;
        f->lockQueue = NULL;
        f->buff = NULL;

        //creo il file e ritorno la struttura file
    }

    if(flags % 2 == 1){
        //O_LOCK settato
        //setto flag lock

        f->lock = fd;
    }
    //setto il flag open
     f->open = fd;

}

int CloseInStorage(char* name, int fd){
    //controllo che il file esista

    File* f = findFile(name);
     if(f == NULL){
            return -1;
    }
    //controllo che sia stata effettuata prima una open dal client c
    if(f->open != fd) return -3;

    //se il file esiste aperto da fd lo chiudo
    f->open = -1;

}
int WriteInStorage(char*name, int dim, int fd){
    //controllo che sia stata effetuata open con create e lock flags

    //aggiungo file

}
int ReadFromStorage(char*name, int fd){
    //controllo che il file esista

    //invio la dim
    //invio il file

}
int LockInStorage(char*name, int fd){
    //controllo il flag di lock o lo setto o mi metto in coda

     File* f = findFile(name);
     if(f == NULL){
            return -1;
    }
    //controllo se la lock è libera
    if(f->lock == -1){ 
        f->lock = fd;
        return 0;
    }

    Client* c = malloc(sizeof(Client));
    c->fdC = fd;
    c->nextC = NULL;

    addRequest(c, f->lockQueue);

  

}
int UnlockInStorage(char*name, int fd){

     //controllo che il file esista

     File* f = findFile(name);
     if(f == NULL){
            return -1;
    }
    //controllo che sia stata effettuata prima una lock dal client c
    if(f->lock != fd) return -3;

    //se il file esiste lockato da fd tolgo la lock
    f->lock = -1;

}
