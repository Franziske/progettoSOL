#include "storage.h"

extern int capacity;
extern int max;

//Ho una specifica funzione per ogni possibile operazione da effetuare sullo storage


int OpenInStorage(char* name, int dim, int flags){
    if(flags == 0){
        //O_LOCK e O_CREATE non sono settati
        //controllo che il file sia presente nello storage
        //altrimenti errore

        //ritorno la struttura file 

    }
    if(flags > 2){
        //O_CREATE è settato
        //controlle che il file non esista già
        //altrimenti errore


        //creo il file e ritorno la struttura file
    }

    if(flags % 2 == 1){
        //O_LOCK settato
        //setto flag lock

    }
    //setto il flag open

}

int CloseInStorage(char* name){
    //controllo che il file esista

    //controllo che sia stata effettuata prima una open dal client c

}
int WriteInStorage(char*name, int dim){
    //controllo che sia stata effetuata open con create e lock flags

    //aggiungo file

}
int ReadFromStorage(char*name){
    //controllo che il file esista

    //invio la dim
    //invio il file

}
int LockInStorage(char*name){
    //controllo il flag di lok o lo setto o mi metto in coda

}
int UnlockInStorage(char*name){

    //controllo il flag di lock 
    //altrimenti errore

}
