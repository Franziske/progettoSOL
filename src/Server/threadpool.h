#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include <pthread.h>

#include "../../lib/utils/utils.h"

typedef struct client_t{
    int fdC;
    struct client_t* nextC;
}Client;

typedef struct threadpool_t {
    pthread_mutex_t  lock;    // mutua esclusione nell'accesso all'oggetto
    pthread_cond_t   cond;    // usata per notificare un worker thread 
    pthread_t  *threads; // array di worker id
    Client *queue;      // coda interna per client che hanno inviato una richiesta
    Client *head, *tail;           // riferimenti della coda
    int nWorker;           // numero di thread (size dell'array threads)
    int count;                // numero di task nella coda dei task pendenti
} Threadpool;

Threadpool* createThreadPool(int nWorker);
int destroyThreadPool(Threadpool *pool, int force);
int addRequestToPool(Threadpool *pool, ServerRequest* req) ;


/**
 * @function spawnThread
 * @brief lancia un thread che esegue la funzione fun passata come parametro, il thread viene lanciato in modalità detached e non fa parte del pool.
 * @param fun  funzione da eseguire per eseguire il task
 * @param arg  argomento della funzione
 * @return 0 se successo, -1 in caso di fallimento, errno viene settato opportunamente.
 */
//int spawnThread(void (*f)(void*), void* arg);


#endif