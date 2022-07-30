#!/bin/sh

make

Successfull=0
Total=0

for expected in $(ls ./tests/*.txt); do
	test="$(basename "$expected" .txt).out"
	if ./tests/"$test" | diff - "$expected"; then
		Successfull="$(expr "$Successfull" + 1)"
	fi
	Total="$(expr "$Total" + 1)"
done

echo "Passed $Successfull of $Total"
exec test "$Successfull" -eq "$Total"
