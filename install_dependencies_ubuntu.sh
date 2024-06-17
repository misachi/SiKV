#!/bin/bash

if [ ! -z "$(command -v apt-get)" ]; then
  echo "Debian Distribution"
  DIST="debian"
  PKG_MGR=apt-get
  sudo $PKG_MGR update -y > /dev/null 2>&1
else
  echo "Unsupported distribution -- only Debian is currently supported."
  exit 1
fi

if [ -z "$(command -v gcc)" ]; then
  echo "Attempting to install GCC..."
  sudo $PKG_MGR install -y gcc g++ > /dev/null
fi

if [ -z "$(command -v make)" ]; then
  echo "Attempting to install Make..."
  sudo $PKG_MGR install -y make > /dev/null
fi

if [ -z "$(command -v valgrind)" ]; then
  echo "Attempting to install valgrind..."
  sudo $PKG_MGR install -y valgrind > /dev/null
fi