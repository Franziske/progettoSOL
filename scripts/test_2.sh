#!/bin/bash

for i in {1..20}; do
    for filename in input/*.txt; do
        ./bin/client.out -p -f ./mysock -W $filename -D ./ejectedFiles &
    done
done
