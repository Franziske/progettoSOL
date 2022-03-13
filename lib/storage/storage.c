#include "storage.h"

#include <stdio.h>
#include <string.h>


int capacity;   //capacità massima dello storage
int max;      //numero massimo di file memorizzabili nello storage

int currMem;
int currNFile;


File* storageHead;
File* storageTail;

pthread_mutex_t storageMutex;     // mutua esclusione sui parametri dello storage


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
  return pthread_mutex_init(&(storageMutex),NULL);
}

// POSSO RESTITUIRE IL FILE LOCKATO?????????
// restituisce il file con la lock per poterlo leggere

File* findFile(char* nameF) {

  //LOCKR(&(storageMutex),NULL);

  if (storageHead == NULL){
    //UNLOCKR(&(storageMutex),NULL);
     return NULL;
  }

   if (strncmp(storageHead->name, nameF, strlen(nameF) + 1) == 0) {
    return storageHead;

  }

  File* aux = storageHead->nextFile;

  while (aux != NULL) {

    //lockToRead(aux);

    if (strncmp(aux->name, nameF, (ssize_t)strlen(nameF) + 1) == 0) {
     
     if(aux != storageTail)UNLOCKR(&(storageMutex), -1);
      return aux;
    
    }
    //unlockToRead(aux);
    aux = aux->nextFile;
  }
  //UNLOCKR(&(storageMutex),NULL);
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
  
    storageTail->nextFile = newFile;
    storageTail = storageTail->nextFile;

  }

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

   //LOCKR(&(storageMutex), -1);

  if (storageHead == NULL) {
    //UNLOCKR(&(storageMutex),NULL);
     return NULL;
  }

  if (strncmp(storageHead->name, nameF, strlen(nameF) + 1) == 0) {
    *prec = NULL;
  
    return storageHead;

  }

  if(storageHead->nextFile == NULL){
    //UNLOCKR(&(storageMutex), -1);
    return NULL;
  }

  if (strncmp(storageHead->nextFile->name, nameF, strlen(nameF) + 1) == 0) {
    *prec = storageHead;
  
    return storageHead->nextFile;

  }

  *prec = storageHead->nextFile;
  File* aux = (*prec)->nextFile;  


  while (aux != NULL) {


    if (strncmp(aux->name, nameF, strlen(nameF) + 1) == 0) {
      //if(aux != storageTail)UNLOCKR(&(storageMutex), -1);
      return aux;
    
    
    }
    (*prec) = aux;
    aux = aux->nextFile;
  }
  //if((*prec) == storageTail) UNLOCKR(&(storageMutex), -1);
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
    currMem = currMem - vTail->dim;
    currNFile = currNFile - 1;
    
    //if(currNFile == max) break;

  }

  return victims;
}

// Ho una specifica funzione per ogni possibile operazione da effetuare sullo
// storage

int OpenInStorage(char* name, int dim, int flags, int fd) {
  LOCKR(&(storageMutex), -3);
  File* f = findFile(name);
  if(f != NULL) lockToModify(f);
  if(f != storageHead && f != storageTail)UNLOCKR(&(storageMutex), -3);
  

  if(f == NULL) printf("file non trovato\n");

  if (flags < 2) {
    // O_CREATE non è settato
    // controllo che il file sia presente nello storage
    // altrimenti errore
    if (f == NULL) {
      if(f == storageHead || f == storageTail)UNLOCKR(&(storageMutex), -3);
      free(name);
      return -1;
    }
  }
  if (flags >= 2) {
    // O_CREATE è settato
    // controlle che il file non esista già
    // altrimenti errore
    if (f != NULL) {
      unlockToModify(f);
     if(f == storageHead || f == storageTail)UNLOCKR(&(storageMutex), -3);
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

    if(f == storageHead || f == storageTail)LOCKR(&(storageMutex), -3); 
    if (addFile(f) != 0) {
      UNLOCKR(&(storageMutex), -3); 
      printf("file non aggiunto \n");
      freeFile(f);
      return -4;
  }
  UNLOCKR(&(storageMutex), -3);
  printf("file aggiunto \n");
  
  }

  if (flags % 2 == 1) {
    // O_LOCK settato
    // setto flag lock

    if(f == storageHead || f == storageTail) LOCKR(&(storageMutex),-3);

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
      if(f != storageHead && f != storageTail)UNLOCKR(&(storageMutex), -3);
      return -4;
    }
  }
  // aggiungo fd alla lista di chi ha aperto il file

  Client* newC = malloc(sizeof(Client));
  newC->fdC = fd;
  newC->nextC = NULL;


  addClient(&(f->open), newC);
  printf("aggiungo fd a chi ha aperto il file \n");

  if(f == storageHead || f == storageTail)UNLOCKR(&(storageMutex), -3);
  return 0;
}

int CloseInStorage(char* name, int fd) {
  // controllo che il file esista

  LOCKR(&(storageMutex), -3);
  File* f = findFile(name);

  if (f == NULL) {
    UNLOCKR(&(storageMutex), -3);
    return -1;
    }
    lockToModify(f);

  if(f != storageHead && f != storageTail)UNLOCKR(&(storageMutex), -3);
  
  
  // controllo che sia stata effettuata prima una open dal client c


  printf("chiudo il file \n");
  int res = removeClient(&(f->open), fd);

  if (f->lock == fd) {
    if (f->lockReqs == NULL) {
      f->lock = -1;
      unlockToModify(f);
      if(f == storageHead || f == storageTail)UNLOCKR(&(storageMutex), -3);
      return res;
    } else {
      f->lock = f->lockReqs->fdC;
      Client* aux = f->lockReqs;
      f->lockReqs = f->lockReqs->nextC;
      free(aux);
      sendResponse(f->lock, 0);
      unlockToModify(f);
    }
  
  }
  if(f == storageHead || f == storageTail)UNLOCKR(&(storageMutex), -3);
  return res;

}

// restituisce il numero di file espulsi
int WriteInStorage(char* name, int dim, int flags, int fd) {
  // leggo il file
  void* buff = malloc(dim);

  printf("dim = %d\n", dim);

  printf("leggo il file\n");

  int res = readn(fd, buff, dim);

  printf("file letto  res = %d\n", res);

  //se la dimensione del file è maggiore della capacità dello storage
  if (dim > capacity) {
    free(buff);
    int r = -5;
    sendResponse(fd, r);
    return -5;
  }

  // controllo che sia stata effetuata open con create e lock flags

  File* victims = NULL;

  LOCKR(&(storageMutex), -3);
  File* f = findFile(name);
    if (f == NULL) {
    printf("file non trovato\n");
    UNLOCKR(&(storageMutex), -3);
    return -1;
  }
  lockToModify(f);
  if(f != storageHead && f != storageTail)UNLOCKR(&(storageMutex), -3);

  if (f->lock != fd) {
    unlockToModify(f);
    if(f == storageHead || f == storageTail)UNLOCKR(&(storageMutex), -3);
    printf("file trovato ma senza lock da fd\n");

    return -3;
  }
  printf("Tutto bene lock=%d e fd=%d\n", f->lock, fd);

  // il file è esplicitamente loccato da fd tramite il flag

  // controllo che il client abbia aperto il file

  Client* aux = f->open;
  while (aux != NULL) {
    if (aux->fdC == fd) break;
    aux = aux->nextC;
  }
  if (aux == NULL) {
    unlockToModify(f);
    printf("file trovato ma non aperto da fd\n");
   if(f == storageHead || f == storageTail)UNLOCKR(&(storageMutex), -3);
    return -3;
  }
 

  // controllo se sforo la capacità dello storage in tal caso seleziono i
  // file vittima da rimuovere

  int victimsCount = 0;


  if(f != storageHead && f != storageTail)LOCKR(&(storageMutex), -3);  //altrimenti ho già la lock

  if (currMem + dim > capacity || currNFile == max) {
    victims = FindVictims(currMem + dim - capacity, fd, &victimsCount);
  }

  currNFile ++;
  currMem = currMem + dim;

  UNLOCKR(&(storageMutex), -1);


  f->dim = dim;
  f->buff = buff;
  unlockToModify(f);

  // invio al client il numero di file vittima
  int err;
  printf("pre write \n");
  err = writen(fd, &victimsCount, sizeof(int));

  printf("file vittima %d\n", victimsCount);
  // CHECKERRE(err, -1, "Errore writen: ");

  if (flags == 1) {
    // invio i file vittima 
    File* aux;
    while (victims != NULL) {
      // gestisci errore send
      aux = victims;
      printf("\n file vittima da inviare : %s \n", victims->name);
      err = sendFileWithName(victims, fd);
      if (err < 0) return err;

      victims = victims->nextFile;
     
      freeFile(aux);
    }

  } else {
    //altrimenti cancello i file vittima senza inviarli
    while (victims != NULL) {
      File* aux = victims;
      victims = victims->nextFile;
     
      freeFile(aux);
    }
  }


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


  if (flags == -1) {

    LOCKR(&(storageMutex), -3);
    File* f = findFile(name);
  
    if (f == NULL) {
    UNLOCKR(&(storageMutex), -3);
    return -1;
    }
    lockToModify(f);
    if(f != storageHead && f != storageTail)UNLOCKR(&(storageMutex), -3);

    // il file è esplicitamente lockato da fd
    if (f->lock == fd){
      unlockToModify(f);
      int res = sendFile(f, fd);
      if(f == storageHead || f == storageTail)UNLOCKR(&(storageMutex), -3);
      return res;
    }

 
    if (f->lock == -1) {
   
      int res = sendFile(f, fd);
      unlockToModify(f);
      if(f == storageHead || f == storageTail)UNLOCKR(&(storageMutex), -3);
      return res;
    }

    // invalid operation file locked
    if(f == storageHead || f == storageTail)UNLOCKR(&(storageMutex), -3);
    return -3;
  }


  // altrimenti il valore di flags è il numero che viene richiesto di file da
  // leggere
  int actualN;  // numero di file realmente inviati al client

  //se non ci sono file da leggere nello storage

  LOCKR(&(storageMutex), -3);
  if (currNFile == 0) {
  UNLOCKR(&(storageMutex), -3);
    int r = 0;
    sendResponse(fd, r);
    return -1;
  }

  //se mi sono stati richiesti n file:

  if (flags == 0 || flags > currNFile)
    actualN = currNFile;

  else {
    actualN = flags;
  }

  int availableCount = 0;
  File* aux = storageHead;
  while (aux!= NULL && availableCount < actualN){
    if (aux->lock == fd || aux->lock == -1) availableCount ++;

    aux = aux->nextFile; 
  }

  actualN = availableCount;

  printf("\t sto per inviare %d file", actualN);


  aux = storageHead;
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
    //lock appertenente ad un altro client 


      // TODO scorrere aux 
    // invalid operation file locked
    UNLOCKR(&(storageMutex), -3);
    return -3;
  }
  UNLOCKR(&(storageMutex), -3);
  return 0;
}

//setta il flag di lock da parte del client fd del file identificato da name 
int LockInStorage(char* name, int fd) {
  // controllo il flag di lock o lo setto o mi metto in coda

  LOCKR(&(storageMutex), -3);
  File* f = findFile(name);
   
   if (f == NULL) {
    UNLOCKR(&(storageMutex), -3);
    return -1;
  }
  lockToModify(f);

  if(f != storageHead && f != storageTail)UNLOCKR(&(storageMutex), -3);
  
  // provo ad acquisire la lock


  if (f->lock == fd) {
    unlockToModify(f);
    if(f == storageHead || f == storageTail)UNLOCKR(&(storageMutex), -3);
    return 0;
  }
  if (f->lock == -1) {
    f->lock = fd;
    unlockToModify(f);
    if(f == storageHead || f == storageTail)UNLOCKR(&(storageMutex), -3);
    printf("file %s lockato da client %d\n", f->name,fd);
    
    return 0;
  }

  Client* newC = malloc(sizeof(Client));
  newC->fdC = fd;
  newC->nextC = NULL;

  addClient(&(f->lockReqs), newC);
   unlockToModify(f);
  if(f == storageHead || f == storageTail)UNLOCKR(&(storageMutex), -3);

  return 1;


}

int UnlockInStorage(char* name, int fd) {
  // controllo che il file esista
  LOCKR(&(storageMutex), -3);
  File* f = findFile(name);

  if (f == NULL) {

    UNLOCKR(&(storageMutex), -3);
    return -1;
  }
  lockToModify(f);

  if(f != storageHead && f != storageTail)UNLOCKR(&(storageMutex), -3);

  // controllo che sia stata effettuata prima una lock dal client c

  if (f->lock != fd) {
    unlockToModify(f);
   if(f == storageHead || f == storageTail)UNLOCKR(&(storageMutex), -3);
    return -3;
  }
  // se il file esiste lockato da fd
  // controllo la richiesta di lock in coda

  if (f->lockReqs == NULL) {
    f->lock = -1;
    unlockToModify(f);
    if(f == storageHead || f == storageTail)UNLOCKR(&(storageMutex), -3);
    return 0;
  }

  f->lock = f->lockReqs->fdC;
  Client* aux = f->lockReqs;
  f->lockReqs = f->lockReqs->nextC;
  unlockToModify(f);
  if(f == storageHead || f == storageTail)UNLOCKR(&(storageMutex), -3);

  free(aux);


  sendResponse(f->lock, 0);

  return 0;
}

int DeleteFromStorage(char* name, int fd) {

  File* prec = NULL;
  LOCKR(&(storageMutex), -3);
  File* f = findFileAndPrec(name, &prec);
  if (f == NULL) {
   UNLOCKR(&(storageMutex), -3);
    return -1;
  }
  
  lockToModify(f);

  if (f->lock != fd ) {
     unlockToModify(f);
 
    UNLOCKR(&(storageMutex), -3);
    return -3;
  }

  if (prec == NULL) {
    storageHead = f->nextFile;
  
  } else {
    lockToModify(prec);
    prec->nextFile = f->nextFile;
    unlockToModify(prec);
  
    //  controllo anche la lock sul precedente?
  }
 
  currMem = currMem - f->dim;
  currNFile = currNFile - 1;
  UNLOCKR(&(storageMutex), -1);

  printf("ATTUALMENTE: %d MEMORIA\n%d FILE\n\n",currMem,currNFile);

  //se ho richieste pendenti di lock sul file che sto eleminando
  //riferisco ai client che il file non esiste più

  Client* aux = f->lockReqs;
  while (aux != NULL){
    sendResponse(aux->fdC, -1);
  }

   unlockToModify(f);
  

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