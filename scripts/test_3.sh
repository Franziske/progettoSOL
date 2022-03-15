#!/bin/bash

start=$(date +%s)
end=$(date +%s)
diff=$(($end - $start))
sec=30 # 30 secondi

echo "Start"
while [ $diff -le $sec ] 
do
    end=$(date +%s)
    diff=$(($end - $start))

    # fai partire i client
    for filename in input/*; do
        ./bin/client.out -f ./mysock -W $filename &
    done

    for filename in input/*; do
        ./bin/client.out -f ./mysock -W $filename &
    done
    
    for filename in input/*; do
        ./bin/client.out -f ./mysock -W $filename &
    done
done
echo "End"