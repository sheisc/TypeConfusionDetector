#!/bin/bash

LLVM_BUILD_DIR=llvm-build


if [ ! -d $LLVM_BUILD_DIR ]; then
	mkdir -p $LLVM_BUILD_DIR	
	cd $LLVM_BUILD_DIR
	cmake ../llvm-7.0.0.src/
	make -j4
	cd ..
fi


. ./env.sh


SVF_BUILD_DIR=svf-build

if [ ! -d $SVF_BUILD_DIR ]; then
	mkdir -p $SVF_BUILD_DIR
	cd $SVF_BUILD_DIR
	cmake ../SVF-7.0.0
	make -j4
	cd ..
fi
