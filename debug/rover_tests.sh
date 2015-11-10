#!/bin/sh

d='rover_tests'
for x in "$d"/in_*; do
	i=$(echo "$x" | sed 's/.*_//')
	echo "Running test $i..."
	(sleep 1; cat $x) | ./rover 2>/dev/null | head -c 100 > "$d"/out_$i
	diff -ur "$d"/good_$i "$d"/out_$i
done
