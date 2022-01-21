#include "threadpool.h"


static void* workerFun(void *threadpool){

    
}

void addRequest(Client **list, Client *newReq)
{

    if ((*list) == NULL)
    {
        *list = newReq;
    }
    else
    {
        Client* aux = (*list);

        while (aux->nextC != NULL)
        {
            aux = aux->nextC;
        }
        aux->nextC = newReq;
    }
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

Threadpool *createThreadPool(int nWorker)
{
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
    pool->head = pool->tail = NULL;
    pool->count = 0;

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

int addToQueue(Threadpool *pool, int* args){

     if (pool == NULL || args == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    LOCK_RETURN(&(pool->lock), -1);
    /*if (pool->exiting)
    {
        UNLOCK_RETURN(&(pool->lock), -1);
        return 1; // esco con valore errore
    }*/

    Client* req = malloc(sizeof(Client));
    req->fdC = args[1];
    req->nextC = NULL;

    addRequest(&(pool->queue), req);

    int r;
    if ((r = pthread_cond_signal(&(pool->cond))) != 0)
    {
        UNLOCK_RETURN(&(pool->lock), -1);
        errno = r;
        return -1;
    }

    UNLOCK_RETURN(&(pool->lock), -1);
    return 0;

}
int destroyThreadPool(Threadpool *pool, int force)
{
}
