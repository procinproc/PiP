#!/bin/sh

prog=$0;

man1_input="man1-inputs.tex"
man3_input="man3-inputs.tex"

echo > $man1_input;
echo > $man3_input;

man1_tex=`ls ../man/man1/*.1`
for man1 in $man1_tex; do
    bn=`basename -s.1 $man1`;
    if [ $bn == "PiP-Commands" ]; then continue; fi
    if [ -f ../latex/group__${bn}.tex ]; then
	tex=${bn};
	echo "\section{$tex}" >> $man1_input;
	echo "\input{../latex/group__${bn}}" >> $man1_input;
    else
	echo "$prog: $bn not found"
    fi
done

man3_tex=`ls ../latex/group__pip__*.tex 2>/dev/null`
for man3 in $man3_tex; do
    bn=`basename -s.tex $man3`;
    echo "\input{../latex/$bn}" >> $man3_input;
done
