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
    for filename in input/*.txt; do
        ./bin/client.out -f ./mysock -W $filename -l $filename -u $filename -l $filename -c $filename&
    sleep 1
    done
    
    
done
echo "End"