#include <stdio.h>
#include <string.h>



#include "storage.h"


extern int capacity;
extern int max;

int currMem = 0;
int currNFile = 0;

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

int sendFile(File*f, int fd){
    int res;
    int nameLen;

    nameLen = strlen(f->name)+1;

    res = writen(fd, nameLen, sizeof(int));
    if(res == -1) return -5;
    res = writen(fd, f->dim, sizeof(int));
    if(res == -1) return -5;
    res = writen(fd, f->name, nameLen);
    if(res == -1) return -5;
    res = writen(fd, f->buff, f->dim);
    if(res == -1) return -5;

    return 0;
        
}


//restitusco la lista di file vittima per far spazio nello storage a n byte

File* FindVictims(int n, int fd, int* victimsCount){
    int count = 0;
    File* victims = NULL;
    File* vTail;

    if((*storageList) == NULL) return -4;

    while(count < n){

        if((*storageList) == NULL) return -4;
        pthread_mutex_lock(&(*storageList)->lockFile);
        (*storageList)->lock = fd;

        count = count + (*storageList)->dim;

        //addFile(victims, f);

        if (victims == NULL){
        victims = (*storageList);
        vTail = victims;
        (*storageList) = (*storageList)->nextFile;
        vTail->nextFile = NULL;
        }
        else{

            vTail->nextFile = (*storageList);
            (*storageList) = (*storageList)->nextFile; 
            vTail = vTail->nextFile;
            vTail->nextFile = NULL;

        }
        //addCient(&(*storageList)->open),);    //deleted from storage
        pthread_mutex_unlock(&(*storageList)->lockFile);

        (*victimsCount)++;
       
    }


}



//Ho una specifica funzione per ogni possibile operazione da effetuare sullo storage

//LA OPEN PUO ESSERE FATTA DA PIU CILIENT FALLA DIVENTARE UA LISTA

int OpenInStorage(char* name, int dim, int flags, int fd){
    
    File* f = findFile(name);
    
    if(flags < 2){

        //O_CREATE non è settato
        //controllo che il file sia presente nello storage
        //altrimenti errore
        
        if(f == NULL){
            return -1;
        }


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
    //aggiungo fd alla lista di chi ha aperto il file
    addClient(f->open, fd);
    return 1;
    

}

int CloseInStorage(char* name, int fd){
    //controllo che il file esista

    File* f = findFile(name);
     if(f == NULL){
            return -1;
    }
    //controllo che sia stata effettuata prima una open dal client c
    return removeClient((&(f->open), fd) != 0);

    //se il file esiste aperto da fd lo chiudo
    //f->open = -1;

}


//restituisce il numero di file espulsi
int WriteInStorage(char*name, int dim, int flags, int fd){

    File* victims = NULL; 
     File* f = findFile(name);
     if(f == NULL){
            return -1;
    }

    if(f->lock != fd) return -3;

    //if(f->open != fd) return -3;
    //controllo che il client abbia aperto il file

    Client* aux = f->open;
    while(aux != NULL){
        if(aux->fdC == fd)break;
        aux = aux->nextC;
    }
    if(aux == NULL) return -3;

    //controllo se sforo la capacità dello storage in tal caso seleziono le vittime

    int victimsCount = 0;

    if(currMem + dim > capacity){

        victims = FindVictims(dim, fd, &victimsCount);
    }

    //invio al client il numero di file vittima 
    int err;
    err = writen(fd, &victimsCount, sizeof(int));
    //CHECKERRE(err, -1, "Errore writen: ");

    if(flags == 1){

        //invio i file vittima e li cancello dal 

        while(victims != NULL){

            Client* aux = victims;

            //gestisci errore send

            sendFile(aux, fd);

            victims = victims->nextFile;
            freeFile(aux);
        }
        
        //invio i file vittima al al client

    }
    else{
        while(victims != NULL){
            Client* aux = victims;
            victims = victims->nextFile;
            freeFile(aux);
        }
    }


    f->buff = malloc(dim);

    int res = readn(fd, f->buff, dim);

    if (res == f->dim) return 0;

    return -4;
    // controlla res

    //CHECKERRE(res, -1, "readn failed");
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
    pthread_cond_signal(&(f->condFile));
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
