#include "threadpool.h"


void addRequest(ServerRequest** list, Request* newReq){

    if( (*list) == NULL){
        *list = newReq;
    }
    else{
        Request* aux = (*list);

        while (aux->next != NULL){
            aux = aux->next;
        }
        aux->next = newReq;
    }

}


void freeRequests(ServerRequest** list){
    ServerRequest*aux;
    while(*list != NULL){
        aux = *list;
        *list = aux->next;
        free(aux->fileName);
        free(aux);
    }
}


threadpool* createThreadPool(int nWorker, int dimQueue){
    if(nWorker <= 0 || dimQueue < 0) {
	errno = EINVAL;
        return NULL;
    }
    
    threadpool *pool = (threadpool *)malloc(sizeof(threadpool));
    if (pool == NULL) return NULL;

    // condizioni iniziali
    pool->nWorker   = 0;
    pool->taskonthefly = 0;
    pool->queueMax = dimQueue;
    pool->head = pool->tail = pool->count = 0;
    pool->exiting = 0;

    /* Allocate thread and task queue */
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * nWorker);
    if (pool->threads == NULL) {
	free(pool);
	return NULL;
    }
    pool->queue = (taskfun_t *)malloc(sizeof(taskfun_t) * abs(pool->queue_size));
    if (pool->queue == NULL) {
	free(pool->threads);
	free(pool);
	return NULL;
    }
    if ((pthread_mutex_init(&(pool->lock), NULL) != 0) ||
	(pthread_cond_init(&(pool->cond), NULL) != 0))  {
	free(pool->threads);
	free(pool->queue);
	free(pool);
	return NULL;
    }
    for(int i = 0; i < nWorker; i++) {
        if(pthread_create(&(pool->threads[i]), NULL,
                          workerpool_thread, (void*)pool) != 0) {
	    /* errore fatale, libero tutto forzando l'uscita dei threads */
            destroyThreadPool(pool, 1);
	    errno = EFAULT;
            return NULL;
        }
        pool->nWorker++;
    }
    return pool;

}

int addRequestToPool(threadpool *pool, ServerRequest* req){


}
int destroyThreadPool(threadpool *pool, int force){


}
