


#include "threadpool.h"


int readRequest (int fd_c, ServerRequest* req)//(int *op, int *dim, int *nameLen, char **name, int* flags, int fd_c, ServerRequest *req)
{
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

int sendResponse(int fd, int res){
    int err;
    err = writen(fd, &res, sizeof(int));
    printf("inviato : %d\n", res);
    CHECKERRE(err, -1, "Errore writen: ");
    return res;
}


Client* getRequest(Threadpool* pool){

    pthread_mutex_lock(&(pool->lock));

    printf("un worker è pronto per la req \n");

    while ((pool->queue) == NULL) {

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


    //LOCK_RETURN(&(pool->lock), -1);
    /*if (pool->exiting)
    {
        UNLOCK_RETURN(&(pool->lock), -1);
        return 1; // esco con valore errore
    }*/
    printf("sono nella add to queue \n");

    Client* fdReq = malloc(sizeof(Client));
    fdReq->fdC = arg;
    fdReq->nextC = NULL;

    pthread_mutex_lock(&(pool->lock));

    pool->tail = addClient(&(pool->queue), fdReq);

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
            break;
}
         case CF : {
            res = CloseInStorage(req->fileName, req->client);
            break;
        }
         case W : {
           res = WriteInStorage(req->fileName, req->dim, req->client);
            break;
        }
         case R : {
            res = ReadFromStorage(req->fileName, req->client);
            break;
        }
         case L :{
                printf("Ho ricevuto una lock\n");
                res = LockInStorage(req->fileName, req->client);
                
                int r = write(pool->fdsPipe, &fdC, sizeof(int));
                //check write
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
            res = 0;
            closeConn = 1;
        }

        default:
            break;
        }
        sendResponse(fdC,res);

       
        printf(" scrivo su pipe %d fdc: %d\n", pool->fdsPipe, fdC);
        free(req);

        if(closeConn == 1) close(fdC);

     }

     //scrivi fd su pipe per comunicare al master di ri mettersi in ascolto

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
            destroyThreadPool(pool, 1);
            errno = EFAULT;
            return NULL;
        }
        //pool->nWorker++;
    }
    return pool;
}


int destroyThreadPool(Threadpool *pool, int sig){

    /*switch (sig){
     //SIGINT e SIGQUIT
    case 2: 
    case 3:{
        
        break;
    }

    //SIGHUP
    case 1:{
        
        break;
    }
    default:
        break;
    }*/
    return 0;
}
