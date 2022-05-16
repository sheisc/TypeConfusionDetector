
export TCDPATH=$(cd $(dirname $0);pwd)


export LLVM_INSTALL_PATH=$TCDPATH/llvm-build
export SVFPATH=$TCDPATH/svf-build
export PATH=$LLVM_INSTALL_PATH/bin:$SVFPATH/bin:$PATH
export LLVM_COMPILER=clang
export LLVM_DIR=$LLVM_INSTALL_PATH
export LD_LIBRARY_PATH=$LLVM_INSTALL_PATH/lib/


 
# iron@CSE:~$ cd github/TypeConfusionDetector/
# iron@CSE:TypeConfusionDetector$ cd svf-build/
# iron@CSE:svf-build$ cd ..
# iron@CSE:TypeConfusionDetector$ . ./env.sh 
# iron@CSE:TypeConfusionDetector$ cd svf-build/
