#! /bin/bash
TGT=${1-feather32u4}
nodemon -e c,h,cpp,ini -w . -V -x "pio run -e $TGT -t upload && (sleep 1;pio run -t monitor)"
