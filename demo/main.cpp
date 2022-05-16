#include <string>
#include <iostream>
#include <vector>
using namespace std;

struct A{
	int x;
};

struct C: public A {
	int y;
};

void f(A *aptr){
	C *c = static_cast<C*>(aptr);
}

char buf[1024];

int main(int argc, char * argv[]) {
	A *a = new (buf) A;
	f(a);
	return 0;
}

