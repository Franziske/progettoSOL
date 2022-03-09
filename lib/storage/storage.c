#include "storage.h"

#include <stdio.h>
#include <string.h>


int capacity;
int max;

int currMem;
int currNFile;


File* storageHead;
File* storageTail;


int lockToModify(File* f){
  LOCKR(&(f->modifyMutex), -1);
  return 0;
}
int unlockToModify(File* f){
  UNLOCKR(&(f->modifyMutex), -1);
  return 0;
}

int lockToRead(File* f){
  LOCKR(&(f->cntMutex), -1);
  f->readCnt ++;
  if(f->readCnt == 1){

    if(pthread_mutex_lock(&(f->modifyMutex)) != 0){
      UNLOCKR(&(f->cntMutex), -1);
      return -1;
    }
  }
  UNLOCKR(&(f->cntMutex), -1);
  return 0;

}
int unlockToRead(File* f){
   LOCKR(&(f->cntMutex), -1);
  f->readCnt --;
  if(f->readCnt == 0){

    if(pthread_mutex_unlock(&(f->modifyMutex)) != 0){
      UNLOCKR(&(f->cntMutex), -1);
      return -1;
    }
  }
  UNLOCKR(&(f->cntMutex), -1);
  return 0;

}

// inizializzo le variabili globali
int storageInit(int c, int m) {
  capacity = c;
  max = m;
  currMem = 0;
  currNFile = 0;
  storageHead = NULL;
  storageTail = NULL;
  return 0;
}

// POSSO RESTITUIRE IL FILE LOCKATO?????????
// restituisce il file con la lock per poterlo leggere

File* findFile(char* nameF) {
  if (storageHead == NULL) return NULL;

  File* aux = storageHead;

  while (aux != NULL) {

    lockToRead(aux);

    if (strncmp(aux->name, nameF, (ssize_t)strlen(nameF) + 1) == 0) {
     
      return aux;
    
    }
    unlockToRead(aux);
    aux = aux->nextFile;
  }

  return NULL;
}

int addFile(File* newFile) {
  int res;
  if (storageHead == NULL) {
    printf("FILE AGGIUNTO IN TESTA\n");
    storageHead = newFile;

    storageTail = storageHead;

  } else {
    printf("FILE AGGIUNTO IN CODA\n");
  ;
    storageTail->nextFile = newFile;
    storageTail = storageTail->nextFile;

  }

  printf("\n ATTUALMENTE:\n testa %s\n coda %s \n", storageHead->name, storageTail->name);
  return 0;
}

void freeFile(File* f) {
  free(f->name);
  free(f->buff);

  free(f);
}

// restituisce il puntatore al file e setta prec in modo che punti al suo
// predecessore, prec == NULL se il file è in testa alla lista, restituisce NULL
// se il file non è presente nello storage

File* findFileAndPrec(char* nameF, File** prec) {
  if (storageHead == NULL) return NULL;

  File* aux = storageHead;

  if (strncmp(storageHead->name, nameF, strlen(nameF) + 1) == 0) {
    *prec = NULL;
  
    return storageHead;

  }

  *prec = storageHead;
  aux = storageHead->nextFile;

  while (aux != NULL) {
    if (strncmp(aux->name, nameF, strlen(nameF) + 1) == 0) {
   
      return aux;
    
    
    }
    (*prec) = aux;
    aux = aux->nextFile;
  }

  return NULL;
}

//invia la dimensione del file f e il buffer con il suo contenuto
int sendFile(File* f, int fd) {
  int res;

  res = writen(fd, &(f->dim), sizeof(int));
  if (res == -1){
    printf("errore invio dim file %d\n\n",f->dim);
    return -4;
    
  }

  res = writen(fd, f->buff, f->dim);
  if (res == -1){
    printf("errore invio buff\n\n");
    return -4;
  
  }
  return 0;
}

//invia la lunghezza del nome del file f, la dimensione del file f, il suo nome e il buffer con il suo contenuto
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


    storageHead->lock = fd;

    count = count + storageHead->dim;

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


    (*victimsCount)++;
    if(currNFile == max) break;

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
    
      free(name);
      return -1;
    }
  }
  if (flags >= 2) {
    // O_CREATE è settato
    // controlle che il file non esista già
    // altrimenti errore
    if (f != NULL) {
     
      free(name);
      return -2;
    }
    printf("creo il file \n");

    // creo il nuovo file
    f = malloc(sizeof(File));
    f->name = name;
    f->dim = dim;
    f->lock = -1;
    f->readCnt = 0;
    f->lockReqs = NULL;
    f->open = NULL;
    f->nextFile = NULL;
    f->buff = NULL;

    currNFile++;

    printf("numero corrente di file nello storage: %d\n", currNFile);

    if (pthread_mutex_init(&f->cntMutex, NULL) != 0) {
      free(name);
      freeFile(f);
      return -4;
    }
     if (pthread_mutex_init(&f->modifyMutex, NULL) != 0) {
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
  
  }

  if (flags % 2 == 1) {
    // O_LOCK settato
    // setto flag lock

    if (f->lock == -1 || f->lock == fd) {
      printf("setto il flag lock sul file \n");

      f->lock = fd;

    } else {
      //è stata richiesta la lock ma il file è già lockato restituisco errore
      if (flags == 3) {
        // avevo creato il file ma l'operazione di lock non è andata a buon fine
   
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


  addClient(&(f->open), newC);
  printf("aggiungo fd a chi ha aperto il file \n");

  return 0;
}

int CloseInStorage(char* name, int fd) {
  // controllo che il file esista

  File* f = findFile(name);
  if (f == NULL) {

    printf("il file non esiste \n");
    return -1;
  }
  // controllo che sia stata effettuata prima una open dal client c


  printf("chiudo il file \n");
  int res = removeClient(&(f->open), fd);

  if (f->lock == fd) {
    if (f->lockReqs == NULL) {
      f->lock = -1;
  
      return 0;
    } else {
      f->lock = f->lockReqs->fdC;
      Client* aux = f->lockReqs;
      f->lockReqs = f->lockReqs->nextC;
      free(aux);
      sendResponse(f->lock, 0);
    }
  
  }
  return res;

  // se il file esiste aperto da fd lo chiudo

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

  if (f->lock != fd) {

    printf("file trovato ma senza lock da fd\n");

    return -3;
  }
  printf("Tutto bene lock=%d e fd=%d\n", f->lock, fd);

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
   
    return -3;
  }
 

  // controllo se sforo la capacità dello storage in tal caso seleziono le
  // vittime

  int victimsCount = 0;


  // se sforassi la capacità massima o il numero masiimo di file nello storage
  //seleziono i file vittima da rimuovere

  if (currMem + dim > capacity || currNFile == max) {
    victims = FindVictims(currMem + dim - capacity, fd, &victimsCount);
  }


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

   
      currMem = currMem - aux->dim;
      currNFile = currNFile - 1;
     
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

//legge files dallo storage:
//se flags = -1 significa che non ho specificato n (numero di file dal leggere)
//altrimenti flags corrisponde a n, se flags = 0 è richiesta la lettura di tutti i file 

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
 
      return -1;
    }

    // il file è esplicitamente lockato da fd
    if (f->lock == fd){
     
      int res = sendFile(f, fd);
  
      return res;
    }

 
    if (f->lock == -1) {
      //informo il cliente che riceverà un file
      //sendResponse(fd, 1);
      int res = sendFile(f, fd);

      return res;
    }

    // invalid operation file locked
    return -3;
  }
  // altrimenti il valore di flags è il numero che viene richiesto di file da
  // leggere
  int actualN;  // numero di file realmente inviati al client



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
   
    if (aux->lock == fd) {
      int res = sendFileWithName(aux, fd);
      aux = aux->nextFile;
    
      // return res;
      continue;
    }

    if (aux->lock == -1) {
      int res = sendFileWithName(aux, fd);
      aux = aux->nextFile;
      continue;

      
      // return res;
    }
      // TODO scorrere aux 
    // invalid operation file locked
    return -3;
  }
  return 0;
}

//setta il flag di lock da parte del client fd del file identificato da name 
int LockInStorage(char* name, int fd) {
  // controllo il flag di lock o lo setto o mi metto in coda

  File* f = findFile(name);
  if (f == NULL) {
   
    return -1;
  }
  // provo ad acquisire la lock


  if (f->lock == fd) {

    return 0;
  }
  if (f->lock == -1) {
    f->lock = fd;

    printf("file %s lockato da client %d\n", f->name,fd);
    
    return 0;
  }

  Client* newC = malloc(sizeof(Client));
  newC->fdC = fd;
  newC->nextC = NULL;

  addClient(&(f->lockReqs), newC);


  return 1;


}

int UnlockInStorage(char* name, int fd) {
  // controllo che il file esista

  File* f = findFile(name);
  if (f == NULL) {
  
    return -1;
  }
  // controllo che sia stata effettuata prima una lock dal client c

  if (f->lock != fd) {
  
    return -3;
  }
  // se il file esiste lockato da fd
  // controllo la richiesta di lock in coda

  if (f->lockReqs == NULL) {
    f->lock = -1;

    return 0;
  }

  f->lock = f->lockReqs->fdC;
  Client* aux = f->lockReqs;
  f->lockReqs = f->lockReqs->nextC;
  free(aux);


  sendResponse(f->lock, 0);

  return 0;
}

int DeleteFromStorage(char* name, int fd) {
  File* prec = NULL;

 
  File* f = findFileAndPrec(name, &prec);
  if (f == NULL) {
   
    return -1;
  }



  if (f->lock != fd ) {

    return -3;
  }
  if (prec == NULL) {
    storageHead = f->nextFile;
  
  } else {
    prec->nextFile = f->nextFile;
  
    //  controllo anche la lock sul precedente?
  }
 
  currMem = currMem - f->dim;
  currNFile = currNFile - 1;


  printf("ATTUALMENTE: %d MEMORIA\n%d FILE\n\n",currMem,currNFile);

  //se ho richieste pendenti di lock sul file che sto eleminando
  //riferisco ai client che il file non esiste più

  Client* aux = f->lockReqs;
  while (aux != NULL){
    sendResponse(aux->fdC, -1);
  }
  

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