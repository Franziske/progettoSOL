#include <stdio.h>
#include <string.h>



#include "storage.h"


extern int capacity;
extern int max;

File** storageList = NULL;



File* findFile(char* nameF){

    if((*storageList) == NULL) return NULL;

    File* aux = (*storageList);

    while(aux!= NULL){
        if(strncmp(aux->name, nameF, (ssize_t)strlen(nameF)+1) == 0 ) return aux;
        aux = aux->nextFile;
    }

    return NULL;

}


void freeFile(File*f){
    free(f->name);
    free(f->buff);
    pthread_mutex_destroy(&(f->lockFile));
    pthread_cond_destroy(&(f->condFile));
    free(f);
}

//restituisce il puntatore al file e setta prec in modo che punti al suo predecessore

File* findFileAndPrec(char* nameF, File* prec){

    if((*storageList) == NULL) return NULL;

    File* aux = (*storageList);

    if(strncmp((*storageList)->name, nameF, (ssize_t)strlen(nameF)+1) == 0){

        prec = NULL;
        return (*storageList);
    }
    prec = (*storageList);
    File* aux = (*storageList)->nextFile;

    while(aux!= NULL){
        if(strncmp(aux->name, nameF, (ssize_t)strlen(nameF)+1) == 0 ) return aux;
        prec = aux;
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
        f->buff = NULL;
        if ((pthread_mutex_init(&(f->lockFile), NULL) != 0) ||
        (pthread_cond_init(&(f->condFile), NULL) != 0)){

            free(f->name);
            free(f);
            return -4;
        }

        //creo il file e ritorno la struttura file
    }

    if(flags % 2 == 1){
        //O_LOCK settato
        //setto flag lock
        if(pthread_mutex_lock(&(f->lockFile)) == 0){

            printf("un client cerca di eseguire la lock su %s \n", f->name);

            while (f->lock != -1) {

                printf(" aspetta che sia rilasciata la lock \n");
                pthread_cond_wait(&(f->condFile), &(f->lockFile));          
            }
            f->lock = fd;

        }
        else return -4;
    }
    //setto il flag open
    f->open = fd;
    return 1;
    

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

     File* f = findFile(name);
     if(f == NULL){
            return -1;
    }
    //controllo che sia stata effetuata open con create e lock flags

    //aggiungo file

}
int ReadFromStorage(char*name, int fd){
    //controllo che il file esista

     File* f = findFile(name);
     if(f == NULL){
            return -1;
    }

    //invio la dim
    //invio il file

}
int LockInStorage(char*name, int fd){
    //controllo il flag di lock o lo setto o mi metto in coda

     File* f = findFile(name);
     if(f == NULL){
            return -1;
    }
    //provo ad acquisire la lock

    if(f->lock == fd) return 0;
   
    if(pthread_mutex_lock(&(f->lockFile)) == 0){

        printf("un client cerca di eseguire la lock su %s \n", f->name);

        while (f->lock != -1) {

            printf(" aspetta che sia rilasciata la lock \n");
            pthread_cond_wait(&(f->condFile), &(f->lockFile));          
        }
        f->lock = fd;
        return 0;

    }
    else return -4;
    
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
    if (pthread_mutex_unlock(&(f->lockFile)) == 0){
    f->lock = -1;
    return 0;
    }
    else return -4;

}

int DeleteFromStorage(char* name, int fd){

    File* prec = NULL;
    File* f = findFileAndPrec(name, prec);
    if(f == NULL){
        return -1;
    }

    if(f->lock != fd) return -3;
    if(prec == NULL){
        (*storageList) = f->nextFile;
        
    }
    else{
        prec->nextFile = f->nextFile;
        //controllo anche la lock sul precedente?
    }

    freeFile(f);
    return 0;


}
