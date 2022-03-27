#define _POSIX_C_SOURCE 200112L  // necessario per utilizzare gettime, e usleep

#include "api.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h> /* ind AF_UNIX */
#include <time.h>
#include <unistd.h>

#include "../utils/utils.h"

#define UNIX_PATH_MAX 108 /* man 7 unix */
//#define SOCKNAME "../Server/mysock"
#define NUM 10

int usleep(useconds_t usec);

static int fdSkt;
static struct sockaddr_un sa;
static char* sock;

// funzione che invia al server le informazioni relative ad una richiesta

int sendRequest(serverOperation op, int dim, const char* name, int flags) {
  if (writen(fdSkt, (int*)&op, sizeof(int)) == -1) return -1;

  if (writen(fdSkt, (int*)&dim, sizeof(int)) == -1) return -1;

  if (writen(fdSkt, &flags, sizeof(int)) == -1) return -1;
  printf("...inviando una richiesta...\n");
  //printf("inviato -> op=%d\tdim=%d flags=%d\n", op, dim, flags);
  if (name != NULL) {
    int len = strlen(name) + 1;
    if (writen(fdSkt, &len, sizeof(int)) == -1) return -1;
    //printf("tento di inviare stringa %s di len %d\n", name, len);
    return writen(fdSkt, (void*)name, len);
    printf("richiesta inviata\n");
  }
  return 0;
}

int receiveResponse() {
  int res = 0;
  int err;
  err = readn(fdSkt, &res, sizeof(int));
  
  PRINTERRS(err, -1, "Errore readn: ",-4);
  //printf("ricevuto : %d\n", res);
  return res;
}

int sendFile(void* buffer, size_t n) {
  printf("...inviando file...");
  return writen(fdSkt, buffer, n);
  printf("file inviato");
}

int receiveAndSaveFile(const char* dirname) {
  int nameLen;
  char* name;
  int dim;
  void* buffer;

  int res;

  nameLen = receiveResponse();
  if (nameLen < -1) {
    errno = ECOMM;
    return -1;
  }

  dim = receiveResponse();
  if (dim < -1) {
    errno = ECOMM;
    return -1;
  }
  name = malloc(nameLen);
  PRINTERRSR(name, NULL, "Errore malloc:");
  res = readn(fdSkt, name, nameLen);

  
  if (res == -1) {

 
    errno = ECOMM;
    return -1;
  }

  buffer = malloc(dim);
  PRINTERRSR(buffer, NULL, "Errore malloc:");
  res = readn(fdSkt, buffer, dim);
  if (res == -1) {
    errno = ECOMM;
    return -1;
  }

  if (dirname != NULL) {
    int pathLen = (strlen(dirname) + nameLen) * sizeof(char) + 1;

    char* path = malloc(pathLen);
    PRINTERRSR(path, NULL, "Errore malloc:");
    strcpy(path, dirname);
  
    strcat(path, name);
    path[pathLen - 1] = '\0';

    //printf("PATH : %s\n", path);

    FILE* file = fopen(path, "w+");
    CHECKERRSC(file, NULL, "fopen failed");

    fwrite(buffer, 1, dim, file);

    fclose(file);

    free(buffer);
    free(name);
    free(path);
  }

  return 0;
}

// legge il contenuto del file fileName e lo scrive in buffer
// restituisce il numero di bytes letti
int getFile(void** buffer, const char* fileName) {
  FILE* ifp = fopen(fileName, "rb");
  CHECKERRSC(ifp, NULL, "Errore fopen: ");
  if (ifp == NULL) return -1;

  void* tmpBuff = malloc(NUM);
  PRINTERRSR(tmpBuff, NULL, "Errore malloc:");
  *buffer = malloc(NUM);
  PRINTERRSR(*buffer, NULL, "Errore malloc:");
  int justRead = 0;
  int bytesRead = 0;
  while (1) {
    // printf("bytesRead=%d\n", bytesRead);
    justRead = fread(tmpBuff, 1, NUM, ifp);
    // CHECKERRNE(ferror(ifp), 0, "fread failed");
    if (ferror(ifp) != 0) {
      free(tmpBuff);
      return -1;
    }

    *buffer = realloc(*buffer, (bytesRead + justRead));

    memcpy((*buffer + bytesRead), tmpBuff, justRead);
    bytesRead = bytesRead + justRead;

    if (feof(ifp)) {
      //printf("byte letti da file %d\n", bytesRead);
      break;
    }
  }
  free(tmpBuff);
  fclose(ifp);
  return bytesRead;
}

/*Viene aperta una connessione AF_UNIX al socket file sockname. Se il server non
accetta immediatamente la richiesta di connessione, la connessione da parte del
client viene ripetuta dopo ‘msec’ millisecondi e fino allo scadere del tempo
assoluto ‘abstime’ specificato come terzo argomento. Ritorna 0 in caso di
successo, -1 in caso di fallimento, errno viene settato opportunamente.*/
int openConnection(const char* sockname, int msec,const struct timespec abstime) {
  int res;
  struct timespec currSpec;
  printf("Apertura Connessione, soket: %s\n", sockname);
  if ((fdSkt = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) return -1;
  sock = malloc(strlen(sockname) * sizeof(char) + 1);
  PRINTERRSR(sock, NULL, "Errore malloc:");
  strcpy(sock, sockname);

  strncpy(sa.sun_path, sock, strlen(sock) * sizeof(char) + 1);
  sa.sun_family = AF_UNIX;

  // connetto il client alla socket del server
  clock_gettime(CLOCK_REALTIME, &currSpec);
  int notElapsed = 1;
  errno = 0;
  // tento di nuovo la connessione fino allo scadere del timeout
  while ((res = (connect(fdSkt, (struct sockaddr*)&sa, sizeof(sa))) == -1) &&
         (notElapsed = ((currSpec.tv_sec < abstime.tv_sec) ||
                        ((currSpec.tv_sec == abstime.tv_sec) &&
                         (currSpec.tv_nsec < abstime.tv_nsec))))) {
   
    printf(".....Connessione.....\n");
    if (errno == ECONNREFUSED || errno == ENOENT) {
      // la socket non esiste
      //  aspetto msec millisecondi e

   

      usleep(msec * 1000);
      clock_gettime(CLOCK_REALTIME, &currSpec);

    } else {
      // altro errore nella connessione
      perror("Errore Connect: ");
      exit(EXIT_FAILURE);
    }
  }
 
  if (notElapsed == 0) {
    // timeout scaduto
    // fprintf(stderr, "Timeout scaduto durante la connessione");
    errno = ETIME;
    perror("Timeout scaduto durante la connessione");
    exit(EXIT_FAILURE);
  }


  free(sock);
  // return fdSkt;
  printf("Conneso al serever\n");
  return 0;
}

/*Chiude la connessione AF_UNIX associata al socket file sockname. Ritorna 0 in
caso di successo, -1 in caso di fallimento, errno viene settato
opportunamente.*/
int closeConnection(const char* sockname) {
  int res;
  if (strncmp(sa.sun_path, sockname, (size_t)strlen(sockname) + 1) != 0) {
    errno = EINVAL;
    return -1;
  } else {
    int res = sendRequest(CC, 0, "bye", -1);
    if (res == -1) {
      errno = ECOMM;
      return -1;
    }
    res = receiveResponse();
    if(close(fdSkt) == -1)return -1;
    SETERRNO(res);
    if(res < 0) return -1;
    return 0;
  }
}

/*Richiesta di apertura o di creazione di un file. La semantica della openFile
dipende dai flags passati come secondo argomento che possono essere O_CREATE ed
O_LOCK. Se viene passato il flag O_CREATE ed il file esiste già memorizzato nel
server, oppure il file non esiste ed il flag O_CREATE non è stato specificato,
viene ritornato un errore. In caso di successo, il file viene sempre aperto in
lettura e scrittura, ed in particolare le scritture possono avvenire solo in
append. Se viene passato il flag O_LOCK (eventualmente in OR con O_CREATE) il
file viene aperto e/o creato in modalità locked, che vuol dire che l’unico che
può leggere o scrivere il file ‘pathname’ è il processo che lo ha aperto. Il
flag O_LOCK può essere esplicitamente resettato utilizzando la chiamata
unlockFile, descritta di seguito. Ritorna 0 in caso di successo, -1 in caso di
fallimento, errno viene settato opportunamente.*/
int openFile(const char* pathname, int flags) {
  if (pathname == NULL) {
    errno = EINVAL;
    return -1;
  }

  int res = sendRequest(OF, 0, pathname, flags);
  if (res == -1) {
    errno = ECOMM;
    return -1;
  }
  res = receiveResponse();
  SETERRNO(res);
    if(res < 0) return -1;
    return 0;

  /*if (res == 0) return 0;
  errno = EBUSY;
  return -1;*/
}

/*Legge tutto il contenuto del file dal server (se esiste) ritornando un
puntatore ad unarea allocata sullo heap nel parametro ‘buf’, mentre ‘size’
conterrà la dimensione del buffer dati (ossia la dimensione in bytes del file
letto). In caso di errore, ‘buf‘e ‘size’ non sono validi. Ritorna 0 in caso di
successo, -1 in caso di fallimento, errno viene settato opportunamente.*/
int readFile(const char* pathname, void** buf, size_t* size) {
  if (pathname == NULL) {
    errno = EINVAL;
    return -1;
  }

  int res = sendRequest(R, 0, pathname, -1);
  if (res == -1) {
    errno = ECOMM;
    return -1;
  }
  res = receiveResponse();  // res in questo caso è la dim del file
  if (res < 0) {
    errno = ENOENT;
    return -1;
  }
  if (res > 0) {
    //printf("RES RICEVUTO = %d\n", res);
    *size = (size_t)res;
  
    *buf = malloc((size_t)res);
    PRINTERRSR(*buf, NULL, "Errore malloc:");
    res = readn(fdSkt, *buf, (size_t)res);
  }
  if (res < 0) {
    errno = ECOMM;
    return -1;
  }
  
  return 0;
}

/*Richiede al server la lettura di ‘N’ files qualsiasi da memorizzare nella
directory ‘dirname’ lato client. Se il server ha meno di ‘N’ file disponibili,
li invia tutti. Se N<=0 la richiesta al server è quella di leggere tutti i file
memorizzati al suo interno. Ritorna un valore maggiore o uguale a 0 in caso di
successo (cioè ritorna il n. di file effettivamente letti), -1 in caso di
fallimento, errno viene settato opportunamente.*/
int readNFiles(int N, const char* dirname) {
  /* if (dirname == NULL) {
     errno = EINVAL;
     return -1;
   }*/

  int res = sendRequest(R, 0, "", N);
  printf("richiesta inviata\n");
  if (res == -1) {
    errno = ECOMM;
    return -1;
  }
  res = receiveResponse();  
  SETERRNO(res);
    if(res < 0) return -1;
  // res in questo caso è il numero di file che invia il server
  for (int i = 0; i < res; i++) {
    if (receiveAndSaveFile(dirname) == -1) {
      errno = ECOMM;
      return -1;
    }
  }

      return 0;
}

/*Scrive tutto il file puntato da pathname nel file server. Ritorna successo
solo se la precedente operazione, terminata con successo, è stata
openFile(pathname, O_CREATE| O_LOCK). Se ‘dirname’ è diverso da NULL, il file
eventualmente spedito dal server perchè espulso dalla cache per far posto al
file ‘pathname’ dovrà essere scritto in ‘dirname’; Ritorna 0 in caso di
successo, -1 in caso di fallimento, errno viene settato opportunamente.*/
int writeFile(const char* pathname, const char* dirname) {
  if (pathname == NULL) {
    errno = EINVAL;
    return -1;
  }


  void* buffer = NULL;

  int bytesRead = getFile(&buffer, pathname);
  if (bytesRead == -1){
    errno = ENOENT;
    return -1;
  }
  int res;
  if (dirname != NULL) res = sendRequest(W, bytesRead, pathname, 1);
  else res = sendRequest(W, bytesRead, pathname, 0);
  if (res == -1) {
    errno = ECOMM;
    return -1;
  }
  res = sendFile(buffer, bytesRead);
  //printf("file inviato byte letti %d  e res = %d\n", bytesRead, res);
  if (res == -1) {
    errno = ECOMM;
    return -1;
  }
  free(buffer);

  res = receiveResponse(); 
  SETERRNO(res);
  if(res < 0) return -1;
  // res in questo caso è il numero di file che invia
                            // il server

  //printf("numero FILE VITTIMA ricevuto %d\n",res);

  if (dirname != NULL) {
    for (int i = 0; i < res; i++) {
      if (receiveAndSaveFile(dirname) == -1) {
        errno = ECOMM;
        return -1;
      }
    }
  }
  return 0;
}

/*Richiesta di scrivere in append al file ‘pathname‘ i ‘size‘ bytes contenuti
nel buffer ‘buf’. L’operazione di append nel file è garantita essere atomica dal
file server. Se ‘dirname’ è diverso da NULL, il file eventualmente spedito dal
server perchè espulso dalla cache per far posto ai nuovi dati di ‘pathname’
dovrà essere scritto in ‘dirname’;
Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato
opportunamente.*/
/*int appendToFile(const char* pathname, void* buf, size_t size, const char*
dirname){ return -1;
}*/

/*In caso di successo setta il flag O_LOCK al file. Se il file era stato
aperto/creato con il flag O_LOCK e la richiesta proviene dallo stesso processo,
oppure se il file non ha il flag O_LOCK settato, l’operazione termina
immediatamente con successo, altrimenti l’operazione non viene completata fino a
quando il flag O_LOCK non viene resettato dal detentore della lock. L’ordine di
acquisizione della lock sul file non è specificato. Ritorna 0 in caso di
successo, -1 in caso di fallimento, errno viene settato opportunamente.*/
int lockFile(const char* pathname) {
  if (pathname == NULL) {
    errno = EINVAL;
    return -1;
  }

  int res = sendRequest(L, 0, pathname, 0);
  if (res == -1) {
    errno = ECOMM;
    return -1;
  }
  res = receiveResponse();
  SETERRNO(res);
  if(res < 0) return -1;
  return 0;
}

/*Resetta il flag O_LOCK sul file ‘pathname’. L’operazione ha successo solo se
l’owner della lock è il processo che ha richiesto l’operazione, altrimenti
l’operazione termina con errore. Ritorna 0 in caso di successo, -1 in caso di
fallimento, errno viene settato opportunamente.*/
int unlockFile(const char* pathname) {
  if (pathname == NULL) {
    errno = EINVAL;
    return -1;
  }

  int res = sendRequest(U, 0, pathname, 0);
  if (res == -1) {
    errno = ECOMM;
    return -1;
  }
  res = receiveResponse();
  SETERRNO(res);
  if(res < 0) return -1;
  return 0;
}

/*Richiesta di chiusura del file puntato da ‘pathname’. Eventuali operazioni sul
file dopo la closeFile falliscono.
Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato
opportunamente.*/
int closeFile(const char* pathname) {
  if (pathname == NULL) {
    errno = EINVAL;
    return -1;
  }

  //printf("invio richiesta close del file");

  int res = sendRequest(CF, 0, pathname, 0);
  if (res == -1) {
    errno = ECOMM;
    return -1;
  }
  res = receiveResponse();
  SETERRNO(res);
  if(res < 0) return -1;
  return 0;
}

/*Rimuove il file cancellandolo dal file storage server. L’operazione fallisce
se il file non è in stato locked, o è in stato locked da parte di un processo
client diverso da chi effettua la removeFile.*/
int removeFile(const char* pathname) {
  if (pathname == NULL) {
    errno = EINVAL;
    return -1;
  }

  int res = sendRequest(C, 0, pathname, 0);
  if (res == -1) {
    errno = ECOMM;
    return -1;
  }
  res = receiveResponse();
  SETERRNO(res);
  if(res < 0) return -1;
  return 0;
}
