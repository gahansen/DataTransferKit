#!/usr/bin/env bash

grep \
  --exclude={*.doc,*.eps,*.gif,*.jpg,*.pdf,*.png} \
  --exclude={.project,.cproject} \
  --exclude={*Makefile*,*makefile*} \
  --exclude=.gitmodules \
  --exclude=DTK_Fortran_wrap.cpp \
  --exclude-dir=data \
  --regexp '[[:blank:]]$' \
  --regexp $'\t' \
  --line-number $(git ls-files)
test $? -eq 1
