#!/bin/bash

rm *.gif
rm *.png

yourfilenames=`ls sample/*.raw`
for entry in $yourfilenames
do
  g="`basename $entry .raw`.gif"
  p="`basename $entry .raw`.png"
  ./prn_cnv $entry $g
  convert $g -monochrome $p
  oxipng -o 6 --strip all $p
done

img2pdf.py *.png -o book.pdf

rm *.gif
rm *.png