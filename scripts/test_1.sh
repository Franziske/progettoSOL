#!/bin/bash


# richiedo la scrittura sul server di tutti i file nella sotto directory 1 di input, poi la scrittura del singolo File-A.txt 
# successivamente si chiede la lettura di File-A.txt e di File-N.txt, con -R poi richiedo 5 file qualsiasi dallo storage
# salvandoli nella cartella filesRead,
# dopo effetuo la lock e l'unlock su File-A.txt e infine la lock su File-N.txt per poterlo cancellare con -c 
./bin/client.out -p -t 200 -f ./mysock -w ./input/subdir1 -W ./input/File-A.txt -r ./input/File-A.txt,./input/subdir1/File-N.txt -Rn=5 -d ./filesRead -l ./input/File-A.txt, -u ./input/File-A.txt, -l ./input/subdir1/File-N.txt, -c ./input/subdir1/File-N.txt