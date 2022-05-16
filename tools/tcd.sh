#!/bin/bash
############################################################
# ./tcd.sh main.bc
############################################################
#APP=$1
#LIBS=$2

#wpa -cxt -flowbg=10000 -cxtbg=10000 -maxcxt=2 -detect-reinterpret -detect-tc -report-all=true -report-threshold=2 -svfmain -ignore-stl=false  $1 > $1.report.txt

wpa -cxt -flowbg=10000 -cxtbg=10000 -maxcxt=2 -detect-reinterpret -detect-tc -svfmain  $1 | tee  $1.report.txt



