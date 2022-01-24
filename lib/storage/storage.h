#include <stdio.h>
#include <string.h>

#include "../utils/utils.h"



typedef struct file_t{
    char*name;
    char*dim;
    int lock;
    Client* lockQueue;
    int open;           //il file è aperto? qualcuno sta leggendo?
    void* buff;         //puntatore al buffer in memoria contenente il file
    struct file_t* nextFile;

}File;



int OpenInStorage();
int CloseInStorage();
int WriteInStorage();
int ReadFromStorage();
int LockInStorage();
int UnlockInStorage();