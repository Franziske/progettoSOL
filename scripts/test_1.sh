#!/bin/bash
echo "Test 1 starting..."
sleep 5
for filename in input/subdir2/*; do
    ./bin/client.out -t 100 -f ./mysock -W $filename
done

