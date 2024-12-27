#!/bin/sh

if [ $# -ne 1 ]
then
  echo "Usage: $0 <Testbench Name>"
  exit 1
fi

tb_name=${1}

if [ ! -f $tb_name ]
then
  echo "No such testbench: $tb_name"
  exit 1
fi

stdout=$(./$tb_name)
error=$(echo "$stdout" | grep "ERROR")

if [ "$error" = "" ]
then
  echo "[  OK  ]: $tb_name"
else
  echo "$stdout"
  echo "[FAILED]: $tb_name"
fi
