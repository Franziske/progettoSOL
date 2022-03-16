#!/bin/bash


# richiedo la scrittura sul server di tutti i file nella sotto directory 1 di input
#...
./bin/client.out -p -t 200 -f ./mysock -w ./input/subdir1 -W ./input/File-A.txt -r ./input/File-A.txt,./input/File-N.txt -R n= 5 -l ./input/File-A.txt, -u ./input/File-A.txt, -l ./input/File-N.txt, -c ./input/File-N.txt,
