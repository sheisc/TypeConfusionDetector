#!/bin/bash

#mkdir -p ${R4PATH}/build
#cd ${R4PATH}/build
#cmake ../SVF7.0
#make

# cd ${R4PATH}/AsmRewriter
# make

# sudo apt install python-pip
# sudo pip install wllvm
# sudo apt-get install gcc-multilib g++-multilib
# sudo apt-get install git


LLVM_BUILD_DIR=llvm-build


if [ ! -d $LLVM_BUILD_DIR ]; then
	mkdir -p $LLVM_BUILD_DIR	
	cd $LLVM_BUILD_DIR
	cmake ../llvm-7.0.0.src/
	make -j4
	cd ..
fi





SVF_BUILD_DIR=svf-build

if [ ! -d $SVF_BUILD_DIR ]; then
	mkdir -p $SVF_BUILD_DIR
	cmake ../SVF-7.0.0
	make -j4
	cd ..
fi
