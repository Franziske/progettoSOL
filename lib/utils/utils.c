#include "utils.h"

#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>



int stringToInt(const char* s){
    errno = 0;
    int n = strtol(s, NULL, 10);
    
    NOTEQUAL(errno, EINVAL,  "strtol EINVAL");
    NOTEQUAL(errno, ERANGE,  "strtol ERANGE");
    return n;

}

Client* addRequest(Client **list, Client *newReq){

    printf("aggiungo req alla coda \n");
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

    return newReq;
}

//funzioni utilizzate per gestire liste di stringhe

void freeStringList(string** list){

    string*aux;
    while((*list) != NULL){
        aux = *list;
        *list = aux->nextString;
        free(aux->name);
        free(aux);
    }
}

void addToList(string** list, string* f){

    if( (*list) == NULL){
        *list = f;
    }
    else{
        string* aux = (*list);

        while (aux->nextString != NULL){
            aux = aux->nextString;
        }
        aux->nextString = f;
    }

printf("elemento aggiunto alla lista : %s\n", f->name);

}

int removeHead(string** list){
   if((*list) == NULL) return -1;

   string* aux = *list;
   *list = (*list)->nextString;
   //non cancello aux-> nome perchè usato come newPath
   free(aux);
   return 0;
}
int listFileInDir(char *dirName, int n, string **list)
{
   int done = 0;
   int fileCount = 0;
   string* dirToSee = NULL;      //lista con i nomi delle directory da visitare
   
   //inizializzo la lista delle directory da visitare aggiungendovi la directory dirName
   string* dir = malloc(sizeof(string));
   dir->name = dirName;
   dir->nextString = NULL;
   addToList(&dirToSee, dir);
   

   while (!done) { 
      if(dirToSee == NULL) done = 1; //se ho ancora directory da visitare
      else{
         char* currPath = dirToSee->name;    //il path corrente è il nome della directory da visitare
         removeHead(&dirToSee);
         // currPath = malloc(sizeof(dirName)*sizeof(char));
         //strcpy(currPath,dirName);
         DIR *d = opendir(currPath); // apro il path
         if (d == NULL)
            return -1;                      // se non riesco ad aprire la directory return -1
         struct dirent *curr;                // scorro il contenuto della directory
         while ((curr = readdir(d)) != NULL){ // se posso leggere qualcosa
            
            printf("sono nel while\n");
            printf("curr restituito %s", curr->d_name);

            if(strcmp(curr->d_name, ".") == 0 || strcmp(curr->d_name, "..") == 0) continue;
            
            printf("non sono uscito dal while\n");
            //costruisco il nuovo path
            size_t length = sizeof(currPath) + sizeof(curr->d_name) + 2;

            char* newPath = malloc(length * sizeof(char));
            strcpy(newPath,currPath);
            strcat(newPath, "/");
            strcat(newPath, curr->d_name);
            newPath[length - 1] = '\0';

            string* f = malloc(sizeof(string));
            f->name = newPath;
            f->nextString = NULL;

            //se quello che ho letto non è una directory lo aggiungo alla lista dei file
            if (curr->d_type != DT_DIR){
               
               addToList(list, f);
               fileCount++;
               if(n != 0 && fileCount == n){
                  done = 1;
                  freeStringList(&dirToSee);
               }
            }
            //se quello che ho letto è una directory la aggiungo alle directory da visitare
            else if (curr->d_type == DT_DIR && strcmp(curr->d_name, ".") != 0 && strcmp(curr->d_name, "..") != 0) // if it is a directory
            {
               addToList(&dirToSee, f);
               
            }
         }
         //se  DirName è vuota free invalida passata come costante al main non alloc
         free(currPath);
         closedir(d); 
      }

   }

   return fileCount;
}
