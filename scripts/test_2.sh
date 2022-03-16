#!/bin/bash

#for i in {1..20}; do
    #for filename in input/*.txt; do
        #./bin/client.out -p -f ./mysock -W $filename -D ./ejectedFiles &
    #done
#done

./bin/client.out -p -f ./mysock -w ./input/subdir2 -D ejectedFiles &

for filename in ./input/subdir1/subdir11/*.txt; do
    ./bin/client.out -p -f ./mysock -W $filename -D ejectedFiles &
done

