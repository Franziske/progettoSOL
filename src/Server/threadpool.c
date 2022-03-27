
#include "threadpool.h"

int readRequest(int fd_c, ServerRequest *req) {
  int op = 0, dim = 0, nameLen = 0, flags = -1;
  void *name = NULL;
  int err;
  err = readn(fd_c, &op, sizeof(int));
  PRINTERRS(err, -1, "readn op failed",-1);

  // printf("readn = %d \n",readn(fd_c,&dim, sizeof(int)));
  err = readn(fd_c, &dim, sizeof(int));
  PRINTERRS(err, -1, "readn dim failed",-1);

  err = readn(fd_c, &flags, sizeof(int));
  PRINTERRS(err, -1, "readn flags failed",-1);

  err = readn(fd_c, &nameLen, sizeof(int));

  // printf(" risultato readn lunghezza nome : %d \n", nameLen);
  PRINTERRS(err, -1, "readn name  failed",-1);
  if(nameLen > 0){
    name = malloc(nameLen);
    PRINTERRSR(name, NULL, "Errore malloc:");
  }

  err = readn(fd_c, name, nameLen);
  PRINTERRS(err, -1, "readn failed",-1);

  printf("ricevuto -> op=%d\tdim=%d\tnameLen=%d\n", op, dim, nameLen);
  if (!op) {

    // se leggo op == 0 un client si è disconnesso
    printf("\t\t\tUser disconnesso\n");
    removeFd(fd_c);
    return -1;
  }
  printf("ricevuto: name=%s\n", (char *)name);

  req->op = (Operation)op;
  req->dim = dim;
  req->nameLenght = nameLen;
  req->fileName = name;
  req->flags = flags;
  req->next = NULL;

  return 0;
}

Client *getRequest(Threadpool *pool) {
  pthread_mutex_lock(&(pool->lock));

  printf("un worker è pronto per la req \n");

  while ((pool->queue) == NULL) {
    // se ho finito le richieste in sospeso e devo terminare
    // invio il segnale per il server sulla pipe e restituisco null al worker
    //printf("sigterm %d \n", pool->termSig);

    if (pool->termSig == 2 || pool->termSig == 3) {
        if (pthread_cond_broadcast(&(pool->cond)) != 0) {
    pthread_mutex_unlock(&(pool->lock));
    errno = EFAULT;
    return NULL;
  }

      pthread_mutex_unlock(&(pool->lock));

      int endOfReqSig = -1;
      write(pool->fdsPipe, &endOfReqSig, sizeof(int));

      printf("Un thread è terminato \n");
      return NULL;
    }


    //printf("sigterm %d \n", pool->termSig);

    if (pool->termSig == 1) {
      if (pthread_cond_broadcast(&(pool->cond)) != 0) {
      pthread_mutex_unlock(&(pool->lock));
      errno = EFAULT;
      return NULL;
     }

      pthread_mutex_unlock(&(pool->lock));
      int endOfReqSig = -1;
      write(pool->fdsPipe, &endOfReqSig, sizeof(int));
    }
    // altrimenti mi metto in attesa
    printf("un worker aspetta la req \n");
    pthread_cond_wait(&(pool->cond), &(pool->lock));
  }

  printf("un worker ha preso la req \n");
  Client *c = pool->queue;
  pool->queue = pool->queue->nextC;
  pool->count--;
  pthread_mutex_unlock(&(pool->lock));
  return c;
}

/*void freeRequests(Client **list) {
  Client *aux;
  while (*list != NULL) {
    aux = *list;
    *list = aux->nextC;
    free(aux);
  }
}*/

int addToQueue(Threadpool *pool, int arg) {
  if (pool == NULL || arg < 2) {
    errno = EINVAL;
    return -1;
  }

  Client *fdReq = malloc(sizeof(Client));
  PRINTERRSR(fdReq, NULL, "Errore malloc:");
  fdReq->fdC = arg;
  fdReq->nextC = NULL;

  pthread_mutex_lock(&(pool->lock));

  if (pool->queue == NULL) {
    pool->queue = fdReq;
    pool->tail = fdReq;
  } else {
    pool->tail->nextC = fdReq;
    pool->tail = fdReq;
  }

  pool->count++;

  int r;
  if ((r = pthread_cond_signal(&(pool->cond))) != 0) {
    pthread_mutex_unlock(&(pool->lock));
    // UNLOCK_RETURN(&(pool->lock), -1);
    errno = r;
    return -1;
  }
  pthread_mutex_unlock(&(pool->lock));
  // UNLOCK_RETURN(&(pool->lock), -1);


  return 0;
}

static void *workerFun(void *threadpool) {
  int res;
  int closeConn;
  
  Threadpool *pool = (Threadpool *)threadpool;
  // controlla != NULL

  while (1) {
    closeConn = 0;
    Client *c = getRequest(pool);
    if (c == NULL)
      // la coda è vuota 
      break;
    // leggo la richiesta fatta dal client c
    int fdC = c->fdC;
    ServerRequest *req = malloc(sizeof(ServerRequest));
    PRINTERRSR(req, NULL, "Errore malloc:");

    // leggo la richiesta
    res = readRequest(fdC, req);
    if (res == -1) {
        printf("impossibile leggere la richiesta da client %d\n", fdC);
        free(req);
        free(c);
        continue;
    }
    // non deve terminare il server 
    req->client = fdC;

    switch (req->op) {
      case OF: {
        res = OpenInStorage(req->fileName, req->dim, req->flags, req->client);
        // controlla errori send
        sendResponse(fdC, res);
        int r = write(pool->fdsPipe, &fdC, sizeof(int));
        // se non creo il file al termine dell'op dealloco il nome
        if(req->flags < 2) free(req->fileName);

        // check write
        break;
      }
      case CF: {
        res = CloseInStorage(req->fileName, req->client);
        // controlla errori send
        sendResponse(fdC, res);
        int r = write(pool->fdsPipe, &fdC, sizeof(int));
        free(req->fileName);

        // check write
        break;
      }
      case W: {
        res = WriteInStorage(req->fileName, req->dim, req->flags, req->client);
        //printf("risultato Write in storage: %d\n", res);
        int r = write(pool->fdsPipe, &fdC, sizeof(int));
        free(req->fileName);
        break;
      }
      case R: {
        //printf("richiesta di leggere dallo storage con %d FLAGS \n", req->flags);
        res = ReadFromStorage(req->fileName, req->flags, req->client);
        //printf("risultato di read da storage %d\n", res);
        int r = write(pool->fdsPipe, &fdC, sizeof(int));
        free(req->fileName);
        break;
      }
      case L: {
        //printf("Ho ricevuto una lock\n");
        res = LockInStorage(req->fileName, req->client);
        if (res != 1) {  // la richiesta è terminata o a buon fine o no

          // controlla errori send
          sendResponse(fdC, res);
          int r = write(pool->fdsPipe, &fdC, sizeof(int));
          free(req->fileName);
          // check write
        }
        // se res == 1 la richiesta di lock è memorizzata nella coda del file
        // sarà eseguita quando il possessore della lock la rilascerà
        break;
      }
      case U: {
        res = UnlockInStorage(req->fileName, req->client);
        sendResponse(fdC, res);
        int r = write(pool->fdsPipe, &fdC, sizeof(int));
        free(req->fileName);
        // check write
        break;
      }

      case C: {
        res = DeleteFromStorage(req->fileName, req->client);
        sendResponse(fdC, res);
        int r = write(pool->fdsPipe, &fdC, sizeof(int));
        free(req->fileName);
        break;
      }
      case CC: {
        printf("%s from client %d \n", req->fileName, req->client);
        free(req->fileName);
        res = 0;
        closeConn = 1;
      }

      default:
        break;
    }
    // sendResponse(fdC,res);

  

    free(c);
    free(req);

    if (closeConn == 1)
      close(fdC);
  }

  return NULL;
}

Threadpool *createThreadPool(int nWorker, int fd) {
  if (nWorker <= 0) {
    errno = EINVAL;
    return NULL;
  }

  Threadpool *pool = malloc(sizeof(Threadpool));
  if (pool == NULL) return NULL;

  // condizioni iniziali
  pool->nWorker = nWorker;
  pool->tail = NULL;
  pool->count = 0;
  pool->fdsPipe = fd;
  pool->termSig = 0;

  /* Allocate thread and task queue */
  pool->threads = malloc(sizeof(pthread_t) * nWorker);
  if (pool->threads == NULL) {
    free(pool);
    return NULL;
  }

  pool->queue = NULL;

  if ((pthread_mutex_init(&(pool->lock), NULL) != 0) ||
      (pthread_cond_init(&(pool->cond), NULL) != 0)) {
    free(pool->threads);
    free(pool->queue);
    free(pool);
    return NULL;
  }
  for (int i = 0; i < nWorker; i++) {
    if (pthread_create(&(pool->threads[i]), NULL, workerFun, (void *)pool) !=
        0) {
      /* errore fatale, libero tutto forzando l'uscita dei threads */
      destroyThreadPool(pool);
      errno = EFAULT;
      return NULL;
    }
    // pool->nWorker++;
  }
  return pool;
}

int terminationProtocol(Threadpool *pool, int signal) {
  if (pool == NULL || signal < 0) {
    errno = EINVAL;
    return -1;
  }
  pthread_mutex_lock(&(pool->lock));

  pool->termSig = signal;
  if (pthread_cond_broadcast(&(pool->cond)) != 0) {
    pthread_mutex_unlock(&(pool->lock));
    errno = EFAULT;
    return -1;
  }

  pthread_mutex_unlock(&(pool->lock));
  return 0;
}





int destroyThreadPool(Threadpool *pool) {
  if (pool == NULL) {
    errno = EINVAL;
    return -1;
  }

  pthread_mutex_lock(&(pool->lock));
  pool->termSig = 2;
  printf("sto deallocando threadpool\n");

  if (pthread_cond_broadcast(&(pool->cond)) != 0) {
    pthread_mutex_unlock(&(pool->lock));
    errno = EFAULT;
    return -1;
  }
  pthread_mutex_unlock(&(pool->lock));

  for (int i = 0; i < pool->nWorker; i++) {
   
    printf("terminazione thread %d\n  ",i);

    if (pthread_join(pool->threads[i], NULL) != 0) {
      errno = EFAULT;
      //pthread_mutex_unlock(&(pool->lock));
      return -1;
    }
    printf("terminato thread %d\n  ",i);
  
  }

  if (pool->threads) {
    free(pool->threads);
    free(pool->queue);

    printf("sto deallocando threadpool  \n");

    pthread_mutex_destroy(&(pool->lock));
    pthread_cond_destroy(&(pool->cond));
  }
  printf("sto deallocando threadpool  \n");
  freeStorage();
  free(pool);

  return 0;
}
