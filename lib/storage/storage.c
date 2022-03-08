#include "storage.h"

#include <stdio.h>
#include <string.h>

// extern int capacity;
// extern int max;

int capacity;
int max;

int currMem;
int currNFile;

pthread_mutex_t storageMutex;

File* storageHead;
File* storageTail;

// inizializzo le variabili globali
int storageInit(int c, int m) {
  capacity = c;
  max = m;
  currMem = 0;
  currNFile = 0;
  storageHead = NULL;
  storageTail = NULL;
  return pthread_mutex_init(&storageMutex, NULL);
}

// POSSO RESTITUIRE IL FILE LOCKATO?????????
// restituisce il file lockato

File* findFile(char* nameF) {
  if (storageHead == NULL) return NULL;

  File* aux = storageHead;

  while (aux != NULL) {
    if (strncmp(aux->name, nameF, (ssize_t)strlen(nameF) + 1) == 0) {
      // if (//LOCK(&(aux->mutex)) == 0)
      return aux;
      // else
      return NULL;
    }
    aux = aux->nextFile;
  }

  return NULL;
}

int addFile(File* newFile) {
  int res;
  if (storageHead == NULL) {
    printf("FILE AGGIUNTO IN TESTA\n");
    storageHead = newFile;
    // LOCK(&storageHead->mutex);
    storageTail = storageHead;
    // res = //UNLOCK(&storageHead->mutex);
  } else {
    printf("FILE AGGIUNTO IN CODA\n");
    // LOCK(&storageTail->mutex);
    storageTail->nextFile = newFile;
    storageTail = storageTail->nextFile;
    // res = //UNLOCK(&storageTail->mutex);
  }

  printf("\n ATTUALMENTE:\n testa %s\n coda %s \n", storageHead->name, storageTail->name);
  return 0;
}

void freeFile(File* f) {
  free(f->name);
  free(f->buff);
  //_pthread_mutex_destroy(&(f->lockFile));
  // pthread_cond_destroy(&(f->condFile));
  free(f);
}

// restituisce il puntatore al file e setta prec in modo che punti al suo
// predecessore, prec == NULL se il file è in testa alla lista restituisce NULL
// se il file non è presente nello storage

File* findFileAndPrec(char* nameF, File** prec) {
  if (storageHead == NULL) return NULL;

  File* aux = storageHead;

  if (strncmp(storageHead->name, nameF, strlen(nameF) + 1) == 0) {
    *prec = NULL;
    // if (//LOCK(&storageHead->mutex) == 0)
    return storageHead;
    // else
    //  return NULL;
  }

  *prec = storageHead;
  aux = storageHead->nextFile;

  while (aux != NULL) {
    if (strncmp(aux->name, nameF, strlen(nameF) + 1) == 0) {
      // if (//LOCK(&(aux->mutex)) == 0)
      return aux;
      // else
      return NULL;
    }
    (*prec) = aux;
    aux = aux->nextFile;
  }

  return NULL;
}

int sendFile(File* f, int fd) {
  int res;
  //int nameLen;

  //nameLen = strlen(f->name) + 1;

  //res = writen(fd, &nameLen, sizeof(int));
  //if (res == -1) return 4;
  res = writen(fd, &(f->dim), sizeof(int));
  if (res == -1){
    printf("errore invio dim file %d\n\n",f->dim);
    return -4;
    
  }
  //res = writen(fd, f->name, nameLen);
  //if (res == -1) return -4;
  res = writen(fd, f->buff, f->dim);
  if (res == -1){
    printf("errore invio buff\n\n");
    return -4;
  
  }
  return 0;
}

int sendFileWithName(File* f, int fd) {
  int res;
  int nameLen;
  char* pName;

  
  pName = strrchr(f->name, '/');
  printf("NOME DA INVIARE %s\n", pName);
  nameLen = strlen(pName)+1;


  res = writen(fd, &nameLen, sizeof(int));
  if (res == -1) return 4;
  res = writen(fd, &(f->dim), sizeof(int));
  if (res == -1){
    printf("errore invio dim file %d\n\n",f->dim);
    return -4;
    
  }
  res = writen(fd, pName, nameLen);
  if (res == -1) return -4;
  res = writen(fd, f->buff, f->dim);
  if (res == -1){
    printf("errore invio buff\n\n");
    return -4;
  
  }
  return 0;
}


// restitusco la lista di file vittima per far spazio nello storage a n byte

File* FindVictims(int n, int fd, int* victimsCount) {
  int count = 0;
  File* victims = NULL;
  File* vTail = NULL;

  if (storageHead == NULL) {
    return NULL;
  }

  while (count < n || currNFile == max) {
    if (storageHead == NULL) {
      return NULL;
    }

    //_lock(&storageHead->lockFile);
    storageHead->lock = fd;

    count = count + storageHead->dim;

    // addFile(victims, f);

    if (victims == NULL) {
      victims = storageHead;
      vTail = victims;
      storageHead = storageHead->nextFile;
      vTail->nextFile = NULL;
      
    } else {
      vTail->nextFile = storageHead;
      storageHead = storageHead->nextFile;
      vTail = vTail->nextFile;
      vTail->nextFile = NULL;
    }

    printf("\nNEW VICTIM %s\n", vTail->name);
    // addCient(&storageHead->open),);    //deleted from storage

    // la lock effettuata sulla testa all'inizio del ciclo
    // corrisponde all'unlock sull'ultimo elemento di victims
    //_pthread_mutex_unlock(&(vTail->lockFile));

    (*victimsCount)++;
    if(currNFile == max) break;

    //currMem = currMem - (vTail->dim);
    //currNFile--;
  }

  return victims;
}

// Ho una specifica funzione per ogni possibile operazione da effetuare sullo
// storage

int OpenInStorage(char* name, int dim, int flags, int fd) {
  File* f = findFile(name);

  if(f == NULL) printf("file non trovato\n");

  if (flags < 2) {
    // O_CREATE non è settato
    // controllo che il file sia presente nello storage
    // altrimenti errore
    if (f == NULL) {
      //_pthread_mutex_unlock(&(f->mutex));
      free(name);
      return -1;
    }
  }
  if (flags >= 2) {
    // O_CREATE è settato
    // controlle che il file non esista già
    // altrimenti errore
    if (f != NULL) {
      // UNLOCK(&(f->mutex));
      free(name);
      return -2;
    }
    printf("creo il file \n");

    // creo il nuovo file
    f = malloc(sizeof(File));
    f->name = name;
    f->dim = dim;
    f->lock = -1;
    f->lockReqs = NULL;
    f->open = NULL;
    f->nextFile = NULL;
    f->buff = NULL;

    currNFile++;

    printf("numero corrente di file nello storage: %d\n", currNFile);
    if (pthread_mutex_init(&f->mutex, NULL) != 0) {
      free(name);
      freeFile(f);
      return -4;
    }

     if (addFile(f) != 0) {
    printf("file non aggiunto \n");
    freeFile(f);
    return -4;
  }

  printf("file aggiunto \n");
    // LOCK(&(f->mutex));

    /* ||
    (pthread_cond_init(&(f->condFile), NULL) != 0)){

        freeFile(f);
    }*/
  }

  if (flags % 2 == 1) {
    // O_LOCK settato
    // setto flag lock
    //_pthread_mutex_lock(&(f->mutex));
    if (f->lock == -1 || f->lock == fd) {
      printf("setto il flag lock sul file \n");

      f->lock = fd;
      // UNLOCK(&(f->mutex));

      // if(pthread_mutex_trylock(&(f->lockFile)) == 0){

      // inutilr è già la semantica della lock
      /*printf("un client cerca di eseguire la lock su %s \n", f->name);

      while (f->lock != -1) {

          printf(" aspetta che sia rilasciata la lock \n");
          pthread_cond_wait(&(f->condFile), &(f->lockFile));
      }*/
      // f->lock = fd;

    } else {
      //è stata richiesta la lock ma il file è già lockato restituisco errore
      if (flags == 3) {
        // avevo creato il file ma l'operazione di lock non è andata a buon fine
        // UNLOCK(&(f->mutex));
        freeFile(f);
        free(name);
      }
      return -4;
    }
  }
  // aggiungo fd alla lista di chi ha aperto il file

  Client* newC = malloc(sizeof(Client));
  newC->fdC = fd;
  newC->nextC = NULL;

  // LOCK(&(f->mutex));
  addClient(&(f->open), newC);
  printf("aggiungo fd a chi ha aperto il file \n");
  // UNLOCK(&(f->mutex));

 
  return 0;
}

int CloseInStorage(char* name, int fd) {
  // controllo che il file esista

  File* f = findFile(name);
  if (f == NULL) {
    //_pthread_mutex_unlock(&(f->mutex));
    printf("il file non esiste \n");
    return -1;
  }
  // controllo che sia stata effettuata prima una open dal client c
  // LOCK(&(f->mutex));

  printf("chiudo il file \n");
  int res = removeClient(&(f->open), fd);

  if (f->lock == fd) {
    if (f->lockReqs == NULL) {
      f->lock = -1;
      // UNLOCK(&(f->mutex));
      return 0;
    } else {
      f->lock = f->lockReqs->fdC;
      Client* aux = f->lockReqs;
      f->lockReqs = f->lockReqs->nextC;
      free(aux);
      sendResponse(f->lock, 0);
    }
    // UNLOCK(&(f->mutex));
  }

  // UNLOCK(&(f->mutex));

  return res;

  // se il file esiste aperto da fd lo chiudo
  // f->open = -1;
}

// restituisce il numero di file espulsi
int WriteInStorage(char* name, int dim, int flags, int fd) {
  // leggo il file
  void* buff = malloc(dim);

  printf("dim = %d\n", dim);

  printf("leggo il file\n");

  int res = readn(fd, buff, dim);

  printf("file letto  res = %d\n", res);

  if (dim > capacity) {
    free(buff);
    int r = -5;
    sendResponse(fd, r);
    return -5;
  }

  // controllo che sia stata effetuata open con create e lock flags

  File* victims = NULL;
  File* f = findFile(name);
  if (f == NULL) {
    printf("file non trovato\n");
    return -1;
  }
  //_pthread_mutex_lock(&(f->mutex));
  if (f->lock != fd) {
    // UNLOCK(&(f->mutex));
    printf("file trovato ma senza lock da fd\n");

    return -3;
  }
  printf("Tutto bene lock=%d e fd=%d\n", f->lock, fd);
  res =  // UNLOCK(&(f->mutex));
      printf("Res della unlock %d\n", res);
  // il file è esplicitamente loccato da fd tramite il flag

  // controllo che il client abbia aperto il file

  Client* aux = f->open;
  while (aux != NULL) {
    if (aux->fdC == fd) break;
    aux = aux->nextC;
  }
  if (aux == NULL) {
    printf("file trovato ma non aperto da fd\n");
    //_pthread_mutex_unlock(&(f->mutex));
    return -3;
  }
  //_pthread_mutex_unlock(&(f->mutex));

  // controllo se sforo la capacità dello storage in tal caso seleziono le
  // vittime

  int victimsCount = 0;
  printf("pre lock storageMutex\n");
  // LOCK(&(storageMutex));
  printf("post lock storageMutex\n");

  // se sforerei la cxapacità massima o il numero masiimo di file nello storage
  //seleziono i file vittima da rimuovere

  if (currMem + dim > capacity || currNFile == max) {
    victims = FindVictims(currMem + dim - capacity, fd, &victimsCount);
  }
  // UNLOCK(&(storageMutex));

  // invio al client il numero di file vittima
  int err;
  printf("pre write \n");
  err = writen(fd, &victimsCount, sizeof(int));

  printf("file vittima %d\n", victimsCount);
  // CHECKERRE(err, -1, "Errore writen: ");

  if (flags == 1) {
    // invio i file vittima e li cancello dallo storage
    File* aux;
    while (victims != NULL) {
      // gestisci errore send
      aux = victims;
      printf("\n file vittima da inviare : %s \n", victims->name);
      err = sendFileWithName(victims, fd);
      if (err < 0) return err;
      victims = victims->nextFile;
      currMem = currMem - aux->dim;
      currNFile = currNFile - 1;
      freeFile(aux);
    }

    // invio i file vittima al al client

  } else {
    //altrimenti cancello i file vittima senza inviarli
    while (victims != NULL) {
      File* aux = victims;
      victims = victims->nextFile;

      // LOCK(&(storageMutex));
      currMem = currMem - aux->dim;
      currNFile = currNFile - 1;
      // UNLOCK(&(storageMutex));
      freeFile(aux);
    }
  }

  f->dim = dim;
  f->buff = buff;
  currMem = currMem + dim;

  printf("ATTUALMENTE: %d MEMORIA\n%d FILE\n\n",currMem,currNFile);
   printf("DIMENSIONE FILE %d \n", f->dim);
  return 0;

  // controlla res

  // CHECKERRE(res, -1, "readn failed");

  // aggiungo file
}

int ReadFromStorage(char* name, int flags, int fd) {
  // controllo che il file esista

  printf("numero corrente di file nello storage %d\n",currNFile);
  File* aux1 = storageHead;
  while(aux1!= NULL){
    printf("File %s\n", aux1->name);
    aux1 = aux1->nextFile;
  }

  if (flags == -1) {
    File* f = findFile(name);
    if (f == NULL) {
      //_pthread_mutex_unlock(&(f->mutex));
      return -1;
    }

    // il file è esplicitamente lockato da fd
    if (f->lock == fd){
      //informo il cliente che riceverà un file
      //sendResponse(fd, 1);
      int res = sendFile(f, fd);
      // UNLOCK(&(f->mutex));
      return res;
    }

    //_pthread_mutex_lock(&(f->mutex));
    if (f->lock == -1) {
      //informo il cliente che riceverà un file
      //sendResponse(fd, 1);
      int res = sendFile(f, fd);

      // UNLOCK(&(f->mutex));
      return res;
    }

    // invalid operation file locked
    return -3;
  }
  // altrimenti il valore di flags è il numero che viene richiesto di file da
  // leggere
  int actualN;  // numero di file realmente inviati al client

  //_pthread_mutex_lock(&(storageMutex));

  //se non ci sono file da leggere nello storage
  if (currNFile == 0) {
    int r = 0;
    sendResponse(fd, r);
    return -1;
  }

  if (flags == 0 || flags > currNFile)
    actualN = currNFile;

  else {
    actualN = flags;
  }

  printf("\t sto per inviare %d file", actualN);


   File* aux = storageHead;
   printf("storage head name %s\n", aux->name);

  sendResponse(fd, actualN);
 

  printf("storage head name %s\n", aux->name);

  for (int i = 0; i < actualN; i++) {
    //_pthread_mutex_lock(&(aux->mutex));
    if (aux->lock == fd) {
      int res = sendFileWithName(aux, fd);
      aux = aux->nextFile;
      //_pthread_mutex_unlock(&(aux->mutex));
      // return res;
      continue;
    }

    //_pthread_mutex_lock(&(f->mutex));
    if (aux->lock == -1) {
      int res = sendFileWithName(aux, fd);
      aux = aux->nextFile;
      continue;

      // UNLOCK(&(aux->mutex));
      // return res;
    }
      // TODO scorrere aux 
    // invalid operation file locked
    return -3;
  }
  return 0;
}
int LockInStorage(char* name, int fd) {
  // controllo il flag di lock o lo setto o mi metto in coda

  File* f = findFile(name);
  if (f == NULL) {
    // UNLOCK(&(f->mutex));
    return -1;
  }
  // provo ad acquisire la lock
  //_pthread_mutex_lock(&(f->mutex));

  if (f->lock == fd) {
    // UNLOCK(&(f->mutex));
    return 0;
  }
  if (f->lock == -1) {
    f->lock = fd;

    printf("file %s lockato da client %d\n", f->name,fd);
    // UNLOCK(&(f->mutex));
    return 0;
  }

  Client* newC = malloc(sizeof(Client));
  newC->fdC = fd;
  newC->nextC = NULL;

  addClient(&(f->lockReqs), newC);
  // UNLOCK(&(f->mutex));

  return 1;

  // int res = pthread_mutex_trylock(&(f->lockFile));

  /*if(res == 0){

      printf("un client cerca di eseguire la lock su %s \n", f->name);

      while (f->lock != -1) {

          printf(" aspetta che sia rilasciata la lock \n");
          pthread_cond_wait(&(f->condFile), &(f->lockFile));
      }
      f->lock = fd;
      return 0;
  }*/

  // la lock non è andata a buon fine perchè già acquisita da un altro client
  // if(res == EBUSY){

  //}

  // else return -4;
}

int UnlockInStorage(char* name, int fd) {
  // controllo che il file esista

  File* f = findFile(name);
  if (f == NULL) {
    // UNLOCK(&(f->mutex));
    return -1;
  }
  // controllo che sia stata effettuata prima una lock dal client c
  //_pthread_mutex_lock(&(f->mutex));
  if (f->lock != fd) {
    // UNLOCK(&(f->mutex));
    return -3;
  }
  // se il file esiste lockato da fd
  // controllo la richiesta di lock in coda

  if (f->lockReqs == NULL) {
    f->lock = -1;
    // UNLOCK(&(f->mutex));
    return 0;
  }

  f->lock = f->lockReqs->fdC;
  Client* aux = f->lockReqs;
  f->lockReqs = f->lockReqs->nextC;
  free(aux);
  // UNLOCK(&(f->mutex));

  sendResponse(f->lock, 0);

  return 0;
}

int DeleteFromStorage(char* name, int fd) {
  File* prec = NULL;

  //_pthread_mutex_lock(&(storageMutex));
  File* f = findFileAndPrec(name, &prec);
  if (f == NULL) {
    // UNLOCK(&(f->mutex));
    return -1;
  }

  //_pthread_mutex_lock(&(f->mutex));

  /*if (f->lock != fd ) {
    //_pthread_mutex_unlock(&(storageMutex));
    // UNLOCK(&(f->mutex));
    return -3;
  }*/
  if (prec == NULL) {
    storageHead = f->nextFile;
    //_pthread_mutex_unlock(&(storageMutex));
    // UNLOCK(&(f->mutex));

  } else {
    prec->nextFile = f->nextFile;
    // UNLOCK(&(f->mutex));
    //_pthread_mutex_lock(&(storageMutex));
    //  controllo anche la lock sul precedente?
  }
  // LOCK(&(storageMutex));
  currMem = currMem - f->dim;
  currNFile = currNFile - 1;
  // UNLOCK(&(storageMutex));

  printf("ATTUALMENTE: %d MEMORIA\n%d FILE\n\n",currMem,currNFile);

  freeFile(f);
  return 0;
}

int freeStorage() {
  printf("sto deallocando storage\n");
  while (storageHead != NULL) {
    printf("file %s\n", storageHead->name);
    printf("eliminando un file\n");
    File* aux = storageHead;
    storageHead = storageHead->nextFile;
    freeFile(aux);
  }
  return 0;
}