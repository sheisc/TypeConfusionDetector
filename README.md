# TCD: Type Confusion Detector for C++ Programs

## 1. How to Build

```sh

iron@CSE:~$ sudo apt install python-pip

iron@CSE:~$ sudo pip install wllvm

iron@CSE:~$ sudo apt-get install gcc-multilib g++-multilib

iron@CSE:~$ cd github/

iron@CSE:github$ pwd

/home/iron/github

iron@CSE:github$ git clone https://github.com/sheisc/TypeConfusionDetector.git

iron@CSE:github$ cd TypeConfusionDetector

iron@CSE:TypeConfusionDetector$ ./build.sh

```

## 2. How to Use

#### Open a New Terminal

```sh

iron@CSE:~$ cd github/TypeConfusionDetector

iron@CSE:TypeConfusionDetector$ . ./env.sh

iron@CSE:TypeConfusionDetector$ cd demo

iron@CSE:demo$ make CXX="wllvm++ -O0 -g"

iron@CSE:demo$ ../tools/tcd.sh ./main.mem2reg.bc

================================  (MUST BE) Type Error [1] ================================
Bad static_cast: 
	%struct.A*   ====>   %struct.C*
Where: 
	ln: 20 fl: main.cpp
Instruction: 
	  %4 = call i8* @__au_edu_unsw_static_cast_stub(i8* %2, i8* %3) #3, !dbg !933, !SrcTypeInfo !934, !DstTypeInfo !935, !UNSWSessionID !936
Src: 
	%struct.A = type { i32 }
Dst: 
	%struct.C = type { %struct.A, i32 }
Actual types of 1 pointee(s): 
(1)
	%struct.A = type { i32 }


```
