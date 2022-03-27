#include "utils.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int stringToInt(const char* s) {
  errno = 0;
  int n = strtol(s, NULL, 10);

  NOTEQUAL(errno, EINVAL, "strtol EINVAL");
  NOTEQUAL(errno, ERANGE, "strtol ERANGE");
  return n;
}

/*void printOp(Operation op, char* f, int retvalue, int bytes){
  switch(op) {
  case Read: printf("Operazione Read "); break;
  case Write: printf("Operazione Write "); break;
  case Lock: printf("Operazione Lock "); break;
  case Unlock: printf("Operazione Unlock "); break;
  case Cancel: printf("Operazione Cancel "); break;
  case Close: printf("Operazione Close "); break;
  default: fprintf(stderr,"Invalid Enum Value ");
  }
  printf("sul file %s terminata con risultato: %d \n",f,retvalue);
  if(bytes > 0) printf("sono stati coivolti nell'operazione %d bytes", bytes);
    
}*/

void printOp (const char* op, char* f, int retvalue, int bytes){
  printf("\nOperazione %s \n effettuata sul file:\n%s \n terminata con risultato: %d \n\n",op,f,retvalue);
   if(bytes > 0) printf("sono stati coivolti nell'operazione %d bytes", bytes);
}

int sendResponse(int fd, int res) {
  int err;
  err = writen(fd, &res, sizeof(int));
  //printf("inviato : %d\n", res);
  //CHECKERRE(err, -1, "Errore writen: ");
  return err;
}



Client* addClient(Client** list, Client* newReq){
  printf("aggiungo una nuova req alla coda \n");
  if ((*list) == NULL) {
    *list = newReq;
  } else {
    Client* aux = (*list);

    while (aux->nextC != NULL) {
      aux = aux->nextC;
    }
    aux->nextC = newReq;
  }

  return newReq;
}

int removeClient(Client** list, int c) {
  if (*list == NULL) return -3;

  Client* aux = (*list);
  Client* prec = NULL;
  if (aux->fdC == c) {
    (*list) = (*list)->nextC;
    free(aux);
    return 0;
  }
  prec = aux;
  aux = aux->nextC;
  while (aux != NULL) {
    if (aux->fdC == c) {
      prec->nextC = aux->nextC;
      free(aux);
      return 0;
    }
    prec = aux;
    aux = aux->nextC;
  }
  return -3;
}

// funzioni utilizzate per gestire liste di stringhe

void freeStringList(string** list) {
  string* aux;
  while ((*list) != NULL) {
    aux = *list;
    *list = aux->nextString;
    free(aux->name);
    free(aux);
  }
}

void addToList(string** list, string* f) {
  if ((*list) == NULL) {
    *list = f;
  } else {
    string* aux = (*list);

    while (aux->nextString != NULL) {
      aux = aux->nextString;
    }
    aux->nextString = f;
  }
}

int removeHead(string** list) {
  if ((*list) == NULL) return -1;

  string* aux = *list;
  *list = (*list)->nextString;
  // non cancello aux-> nome perchè usato come newPath
  free(aux);
  return 0;
}
int listFileInDir(char* dirName, int n, string** list) {
  int done = 0;
  int fileCount = 0;
  string* dirToSee = NULL;  // lista con i nomi delle directory da visitare

  // inizializzo la lista delle directory da visitare aggiungendovi la directory
  // dirName
  string* dir = malloc(sizeof(string));
  PRINTERRSR(dir, NULL, "Errore malloc:");
  dir->name = dirName;
  dir->nextString = NULL;
  addToList(&dirToSee, dir);

    while (!done){
        if (dirToSee == NULL) done = 1;
        // se ho ancora directory da visitare
        else {
            char* currPath = dirToSee->name;  // il path corrente è il nome della directory da visitare
            removeHead(&dirToSee);
            // currPath = malloc(sizeof(dirName)*sizeof(char));
            // strcpy(currPath,dirName);
            DIR* d = opendir(currPath);  // apro il path
            if (d == NULL)
                return -1;          // se non riesco ad aprire la directory return -1
            struct dirent* curr;  // scorro il contenuto della directory
            while ((curr = readdir(d)) != NULL) {  // se posso leggere qualcosa

                //printf("curr restituito %s", curr->d_name);

                if (strcmp(curr->d_name, ".") == 0 || strcmp(curr->d_name, "..") == 0)
                continue;

                //printf("non sono uscito dal while\n");
                // costruisco il nuovo path
                size_t length = sizeof(currPath) + sizeof(curr->d_name) + 2;

                char* newPath = malloc(length * sizeof(char));
                PRINTERRSR(newPath, NULL, "Errore malloc:");
                strcpy(newPath, currPath);
                strcat(newPath, "/");
                strcat(newPath, curr->d_name);
                newPath[length - 1] = '\0';

                string* f = malloc(sizeof(string));
                PRINTERRSR(f, NULL, "Errore malloc:");

                f->name = newPath;
                f->nextString = NULL;

                // se quello che ho letto non è una directory lo aggiungo alla lista dei
                // file
                if (curr->d_type != DT_DIR) {
                addToList(list, f);
                printf("file %s trovato\n", f->name);
                fileCount++;
                    if (n != 0 && fileCount == n) {
                        done = 1;
                        freeStringList(&dirToSee);
                    }
                }
                // se quello che ho letto è una directory la aggiungo alle directory da
                // visitare
                else if (curr->d_type == DT_DIR && strcmp(curr->d_name, ".") != 0 &&
                        strcmp(curr->d_name, "..") != 0){
                addToList(&dirToSee, f);
                printf("directory %s, aggiunta alla lista da visitare \n",curr->d_name);
                }
            }
            // se  DirName è vuota free invalida passata come costante al main non alloc
            if (currPath != dirName){
                //printf("currpath = %s\n", currPath);
                free(currPath);
            }
            closedir(d);
        }
    }

  return fileCount;
}
