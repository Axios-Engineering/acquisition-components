#!/bin/bash
configure='configure'
makefile_in='Makefile.in'
config_ac='configure.ac'
make_am='Makefile.am'
makefile='Makefile'

if [ "$1" == 'clean' ]; then
  make clean
else
  # Checks if build is newer than makefile (based on modification time)
  if [[ ! -e $configure || ! -e $makefile_in || $config_ac -nt $makefile || $make_am -nt $makefile ]]; then
    ./reconf
    ./configure
  fi
  make
  exit 0
fi
