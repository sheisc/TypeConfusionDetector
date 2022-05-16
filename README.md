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

iron@CSE:demo$ cat -n main.cpp
     1	#include <string>
     2	#include <iostream>
     3	#include <vector>
     4	using namespace std;
     5	
     6	struct A{
     7		int x;
     8	};
     9	
    10	struct C: public A {
    11		int y;
    12	};
    13	
    14	void f(A *aptr){
    15		C *c = static_cast<C*>(aptr);
    16	}
    17	
    18	char buf[1024];
    19	
    20	int main(int argc, char * argv[]) {
    21		A *a = new (buf) A;
    22		f(a);
    23		return 0;
    24	}
    25	


iron@CSE:demo$ make CXX="wllvm++ -O0 -g"

iron@CSE:demo$ ../tools/tcd.sh ./main.mem2reg.bc

================================  (MUST BE) Type Error [1] ================================
Bad static_cast: 
	%struct.A*   ====>   %struct.C*
Where: 
	ln: 15 fl: main.cpp
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
