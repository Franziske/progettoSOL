#include <stdio.h>
#include <string.h>



#include "storage.h"


//extern int capacity;
//extern int max;

int capacity;
int max;

int currMem;
int currNFile;

pthread_mutex_t storageMutex;

File** storageList;

//inizializzo le variabili globali
int storageInit(int c,int m){
    capacity = c;
    max = m;
    currMem = 0;
    currNFile = 0;
    storageList = NULL;
    return pthread_mutex_init(&storageMutex, NULL);
}



File* findFile(char* nameF){

    if((*storageList) == NULL) return NULL;

    File* aux = (*storageList);

    while(aux!= NULL){
        if(strncmp(aux->name, nameF, (ssize_t)strlen(nameF)+1) == 0 ) return aux;
        aux = aux->nextFile;
    }

    return NULL;

}

int addFile(File* newFile){

    pthread_mutex_lock(&(storageMutex));

    if( (*storageList) == NULL){
        (*storageList) = newFile;
       return pthread_mutex_unlock(&(storageMutex));
        
    }
    else{
        File* aux = (*storageList);

        while (aux->nextFile != NULL){
            aux = aux->nextFile;
        }
        aux->nextFile = newFile;
    }

   return pthread_mutex_unlock(&(storageMutex));
}


void freeFile(File*f){
    free(f->name);
    free(f->buff);
    //pthread_mutex_destroy(&(f->lockFile));
    //pthread_cond_destroy(&(f->condFile));
    free(f);
}

//restituisce il puntatore al file e setta prec in modo che punti al suo predecessore, prec == NULL se il file è in testa alla lista
//restituisce NULL se il file non è presente nello storage

File* findFileAndPrec(char* nameF, File** prec){

    if((*storageList) == NULL) return NULL;

    File* aux = (*storageList);

    if(strncmp((*storageList)->name, nameF, (ssize_t)strlen(nameF)+1) == 0){

        (*prec) = NULL;
        return (*storageList);
    }
    (*prec) = (*storageList);
    aux = (*storageList)->nextFile;

    while(aux!= NULL){
        if(strncmp(aux->name, nameF, (ssize_t)strlen(nameF)+1) == 0 ) return aux;
        (*prec) = aux;
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
    File* vTail = NULL;

    if((*storageList) == NULL) return -4;

    while(count < n){

        if((*storageList) == NULL) return -4;

        //_lock(&(*storageList)->lockFile);
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

        //la lock effettuata sulla testa all'inizio del ciclo 
        //corrisponde all'unlock sull'ultimo elemento di victims
       // pthread_mutex_unlock(&(vTail->lockFile));

        (*victimsCount)++;

        
        if(currNFile == max) break;
       
    }
    return victims;

}



//Ho una specifica funzione per ogni possibile operazione da effetuare sullo storage

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
    if(flags >=2){
        //O_CREATE è settato
        //controlle che il file non esista già
        //altrimenti errore
        if(f != NULL){
            return -2;
        }

        //creo il nuovo file
        f = malloc(sizeof(File));
        f->name = name;
        f->dim = dim;
        f->lock = -1;
        f->lockReqs = NULL;
        f->open = NULL;
        f->nextFile = NULL;
        f->buff = NULL;
        if(pthread_mutex_init(&f->mutex, NULL) != 0){
            freeFile(f);
            return -4;
        } 
        
        /* ||
        (pthread_cond_init(&(f->condFile), NULL) != 0)){

            freeFile(f);
        }*/ 
    }

    if(flags % 2 == 1){
        //O_LOCK settato
        //setto flag lock
        if(f->lock == -1 || f->lock == fd){

            f->lock = fd;


        //if(pthread_mutex_trylock(&(f->lockFile)) == 0){


            // inutilr è già la semantica della lock
            /*printf("un client cerca di eseguire la lock su %s \n", f->name);

            while (f->lock != -1) {

                printf(" aspetta che sia rilasciata la lock \n");
                pthread_cond_wait(&(f->condFile), &(f->lockFile));          
            }*/
            //f->lock = fd;

        }
        else{
            //è stata richiesta la lock ma il file è già lockato restituisco errore
            if(flags == 3){
                //avevo creato il file ma l'operazione di lock non è andata a buon fine
               freeFile(f);
            }
            return -4;
        }
    }
    //aggiungo fd alla lista di chi ha aperto il file
    addClient(&(f->open), fd);

    if (addFile(f) != 0){
        freeFile;
        return -4;
    }
    return 0;
    

}

int CloseInStorage(char* name, int fd){
    //controllo che il file esista

    File* f = findFile(name);
     if(f == NULL){
            return -1;
    }
    //controllo che sia stata effettuata prima una open dal client c
    pthread_mutex_lock(&(f->mutex));
    int res = removeClient((&(f->open), fd) != 0);
    pthread_mutex_unlock(&(f->mutex));

    return res;

    //se il file esiste aperto da fd lo chiudo
    //f->open = -1;

}


//restituisce il numero di file espulsi
int WriteInStorage(char*name, int dim, int flags, int fd){

     //controllo che sia stata effetuata open con create e lock flags

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

    if(currMem + dim > capacity || currNFile == max){

        victims = FindVictims(currMem + dim - capacity , fd, &victimsCount);
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

            if(err = sendFile(aux, fd) < 1) return err;

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
   

    //aggiungo file

}
int ReadFromStorage(char*name, int fd){
    //controllo che il file esista

     File* f = findFile(name);
     if(f == NULL){
            return -1;
    }

    if(f->lock == -1 || f->lock == fd)return sendFile(f, fd);


    //invalid operation file locked
    return  -3;


}
int LockInStorage(char*name, int fd){
    //controllo il flag di lock o lo setto o mi metto in coda

     File* f = findFile(name);
     if(f == NULL){
            return -1;
    }
    //provo ad acquisire la lock
    pthread_mutex_lock(&(f->mutex));

    if(f->lock == fd){
        pthread_mutex_unlock(&(f->mutex));
        return 0;
    }
    if(f->lock == -1){ 
        
        f->lock = fd;
        pthread_mutex_unlock(&(f->mutex));
        return 0;
    }
    addClient(&(f->lockReqs), fd);
    pthread_mutex_unlock(&(f->mutex));

    return 1;

    //int res = pthread_mutex_trylock(&(f->lockFile));
   
    /*if(res == 0){

        /*printf("un client cerca di eseguire la lock su %s \n", f->name);

        while (f->lock != -1) {

            printf(" aspetta che sia rilasciata la lock \n");
            pthread_cond_wait(&(f->condFile), &(f->lockFile));          
        }
        f->lock = fd;
        return 0;
    }*/

    // la lock non è andata a buon fine perchè già acquisita da un altro client
    //if(res == EBUSY){
        
    //}

   // else return -4;
    
}

  


int UnlockInStorage(char*name, int fd){

     //controllo che il file esista

     File* f = findFile(name);
     if(f == NULL){
            return -1;
    }
    //controllo che sia stata effettuata prima una lock dal client c
    pthread_mutex_lock(&(f->mutex));
    if(f->lock != fd){
        pthread_mutex_unlock(&(f->mutex));
        return -3;
    }
    //se il file esiste lockato da fd 
    //controllo la richiesta di lock in coda 

    
    if(f->lockReqs == NULL){
        f->lock = -1;
         pthread_mutex_unlock(&(f->mutex));
        return 0;
    }
   

    f->lock = f->lockReqs->fdC;
    Client* aux = f->lockReqs;
    f->lockReqs = f->lockReqs->nextC;
    free(aux);
    pthread_mutex_unlock(&(f->mutex));

    sendResponse(f->lock, 0);

}

int DeleteFromStorage(char* name, int fd){

    File* prec = NULL;

    pthread_mutex_lock(&(storageMutex));
    File* f = findFileAndPrec(name, &prec);
    if(f == NULL){
        return -1;
    }

   //pthread_mutex_lock(&(f->mutex));

    if(f->lock != fd){
         pthread_mutex_unlock(&(storageMutex));
        //pthread_mutex_unlock(&(f->mutex));
        return -3;
    }
    if(prec == NULL){
        (*storageList) = f->nextFile;
         pthread_mutex_unlock(&(storageMutex));
        
    }
    else{
        prec->nextFile = f->nextFile;
         pthread_mutex_lock(&(storageMutex));
        //controllo anche la lock sul precedente?
    }

    freeFile(f);
    return 0;


}
