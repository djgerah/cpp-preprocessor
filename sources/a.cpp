// this comment before include
#include "dir1/b.h"
// text between b.h and c.h
#include "dir1/d.h"

int SayHello() {
    cout << "hello, world!" << endl;
#   include<dummy.txt>
}
