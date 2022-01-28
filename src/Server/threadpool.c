
#include "threadpool.h"




int readRequest (int fd_c, ServerRequest* req){
   int op = 0, dim = 0, nameLen = 0, flags = -1;
   void *name = NULL;
   int err;
   err = readn(fd_c, &op, sizeof(int));
   printf("ricevuto : %d\n", op);
   CHECKERRE(err, -1, "readn failed");

   //printf("readn = %d \n",readn(fd_c,&dim, sizeof(int)));
   err = readn(fd_c, &dim, sizeof(int));
   CHECKERRE(err, -1, "readn failed");
   printf("ricevuto : %d\n", dim);

   err = readn(fd_c, &flags, sizeof(int));
   CHECKERRE(err, -1, "readn failed");
   printf("ricevuto : %d\n", flags);

   err = readn(fd_c, &nameLen, sizeof(int));

  // printf(" risultato readn lunghezza nome : %d \n", nameLen);
   CHECKERRE(err, -1, "readn failed");
   printf("ricevuto : %d\n", nameLen);

   name = malloc(nameLen);

   err = readn(fd_c, name, nameLen);
   CHECKERRE(err, -1, "readn failed");

   printf("ricevuto: %d,%d,%d\n", op, dim, nameLen);
   printf("ricevuto: %s\n",(char*)name);

   req->op = (Operation)op;
   req->dim = dim;
   req->nameLenght = nameLen;
   req->fileName = name;
   req->flags = flags;
   req->next = NULL;

   return 0;
}

Client* getRequest(Threadpool* pool){

    pthread_mutex_lock(&(pool->lock));

    printf("un worker è pronto per la req \n");

    while ((pool->queue) == NULL) {

        //se ho finito le richieste in sospeso e devo terminare
        //invio il segnale per il server sulla pipe e restituisco null al worker

        if(pool->termSig == 2 || pool->termSig== 3){
            pthread_mutex_unlock(&(pool->lock));

            int endOfReqSig = -1;
            write(pool->fdsPipe, &endOfReqSig, sizeof(int));
            return NULL;
        }

        //???????????
        
        if(pool->termSig == 1){
            int endOfReqSig = -1;
            write(pool->fdsPipe, &endOfReqSig, sizeof(int));
        }
        //altrimenti mi metto in attesa 
        printf("un worker aspetta la req \n");
        pthread_cond_wait(&(pool->cond), &(pool->lock));          
    }

    printf("un worker ha preso la req \n");
    Client* c = pool->queue;
    pool->queue = pool->queue->nextC;
    pool->count --;
    pthread_mutex_unlock(&(pool->lock));
    return c;

}



void freeRequests(Client **list)
{

    Client *aux;
    while (*list != NULL)
    {
        aux = *list;
        *list = aux->nextC;
        free(aux);
    }
}

int addToQueue(Threadpool *pool, int arg){

     if (pool == NULL || arg < 2)
    {
        errno = EINVAL;
        return -1;
    }
    printf("sono nella add to queue \n");

    Client* fdReq = malloc(sizeof(Client));
    fdReq->fdC = arg;
    fdReq->nextC = NULL;

    pthread_mutex_lock(&(pool->lock));

    if(pool->queue == NULL){
        pool->queue = fdReq;
        pool->tail = fdReq;
    }
    else{
        pool->tail->nextC = fdReq;
        pool->tail = fdReq;

    }

    pool->count ++;

    int r;
    if ((r = pthread_cond_signal(&(pool->cond))) != 0)
    {
        pthread_mutex_unlock(&(pool->lock));
       // UNLOCK_RETURN(&(pool->lock), -1);
        errno = r;
        return -1;
    }
    pthread_mutex_unlock(&(pool->lock));
    //UNLOCK_RETURN(&(pool->lock), -1);

    printf("ritorno dalla add to queue\n");
    return 0;

}


static void* workerFun(void *threadpool){

    int res;
    int closeConn;
    printf("sono un tread e sto funzionando\n");
    Threadpool *pool = (Threadpool *)threadpool;
    //controlla != NULL

     while (1) {
        closeConn = 0;
        Client* c = getRequest(pool);
        if (c == NULL)
        //la coda è vuota e devo terminare
            break;
        // leggo la richiesta fatta dal client c
        int fdC = c->fdC;
        ServerRequest *req = malloc(sizeof(ServerRequest));

        //leggo la richiesta
        res = readRequest(fdC, req);
       // non deve terminare il server CHECKERRE(res, -1, "Errore readRequest: ");
       req->client = fdC;

       switch (req->op){
        case OF : {
            res = OpenInStorage(req->fileName, req->dim, req->flags, req->client);
            //controlla errori send
                sendResponse(fdC,res);
                int r = write(pool->fdsPipe, &fdC, sizeof(int));

                //check write
            break;
}
         case CF : {
            res = CloseInStorage(req->fileName, req->client);
            //controlla errori send
                sendResponse(fdC,res);
                int r = write(pool->fdsPipe, &fdC, sizeof(int));
                free(req->fileName);
                //check write
            break;
        }
         case W : {
           res = WriteInStorage(req->fileName, req->dim, req->flags,req->client);
            printf("risultato Write in storage: %d\n", res);
            int r = write(pool->fdsPipe, &fdC, sizeof(int));
            free(req->fileName);
            break;
        }
         case R : {
            res = ReadFromStorage(req->fileName, req->client);
            break;
        }
         case L :{
                printf("Ho ricevuto una lock\n");
                res = LockInStorage(req->fileName, req->client);
                if(res != 1){       //la richiesta è terminata o a buon fine o no

                //controlla errori send
                sendResponse(fdC,res);
                int r = write(pool->fdsPipe, &fdC, sizeof(int));
                //check write
                }
                //se res == 1 la richiesta di lock è memorizzata nella coda del file
                //sarà eseguita quando il possessore della lock la rilascerà
                break;
        }
         case U : {
                res = UnlockInStorage(req->fileName, req->client);
                int r = write(pool->fdsPipe, &fdC, sizeof(int));
                //check write
            break;
        }

        case C : {

            res = DeleteFromStorage(req->fileName, req->client);

            break;
        }
        case CC : {

            printf("%s from client %d \n", req->fileName,req->client);
            free(req->fileName);
            res = 0;
            closeConn = 1;
        }

        default:
            break;
        }
        //sendResponse(fdC,res);

       
        printf(" scrivo su pipe %d fdc: %d\n", pool->fdsPipe, fdC);
        
        free(c);
        free(req);

        if(closeConn == 1) close(fdC);

     }

    

return NULL;

}




Threadpool *createThreadPool(int nWorker, int fd){

    if (nWorker <= 0)
    {
        errno = EINVAL;
        return NULL;
    }

    Threadpool *pool = malloc(sizeof(Threadpool));
    if (pool == NULL)
        return NULL;

    // condizioni iniziali
    pool->nWorker = nWorker;
    pool->tail = NULL;
    pool->count = 0;
    pool->fdsPipe = fd;
    pool->termSig = 0;

    /* Allocate thread and task queue */
    pool->threads = malloc(sizeof(pthread_t) * nWorker);
    if (pool->threads == NULL)
    {
        free(pool);
        return NULL;
    }
    
    pool->queue = NULL;

    if ((pthread_mutex_init(&(pool->lock), NULL) != 0) ||
        (pthread_cond_init(&(pool->cond), NULL) != 0))
    {
        free(pool->threads);
        free(pool->queue);
        free(pool);
        return NULL;
    }
    for (int i = 0; i < nWorker; i++)
    {
        if (pthread_create(&(pool->threads[i]), NULL,
                           workerFun, (void *)pool) != 0)
        {
            /* errore fatale, libero tutto forzando l'uscita dei threads */
            destroyThreadPool(pool);
            errno = EFAULT;
            return NULL;
        }
        //pool->nWorker++;
    }
    return pool;
}

int terminationProtocol(Threadpool *pool,int signal){
      if (pool == NULL || signal < 0)
    {
        errno = EINVAL;
        return -1;
    }
    pthread_mutex_lock(&(pool->lock));
    
    pool->termSig = signal;

    pthread_mutex_unlock(&(pool->lock));
    return 0;
}


int destroyThreadPool(Threadpool *pool){

     if (pool == NULL){
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock (&(pool->lock));

    printf("sto deallocando threadpool\n");

    //forse prima devo fare la join dei thread

    if (pthread_cond_broadcast(&(pool->cond)) != 0){
        pthread_mutex_unlock (&(pool->lock));
        errno = EFAULT;
        return -1;
    }
    pthread_mutex_unlock (&(pool->lock));

    for (int i = 0; i < pool->nWorker; i++){
        if (pthread_join(pool->threads[i], NULL) != 0){
            errno = EFAULT;
            pthread_mutex_unlock (&(pool->lock));
            return -1;
        }
    }

     if (pool->threads){
        free(pool->threads);
        free(pool->queue);

        printf("sto deallocando threadpool  1\n");

        pthread_mutex_destroy(&(pool->lock));
        pthread_cond_destroy(&(pool->cond));
    }
    printf("sto deallocando threadpool  1\n");
    freeStorage();
    free(pool);
    
    return 0;
}
