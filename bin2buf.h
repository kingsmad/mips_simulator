/*************************************************************************
    > File Name: bin2buf.h
************************************************************************/
#include <iostream>
#include <stdlib.h>

using namespace std;
/*op:6, rs:5, rt:5, rd:5, sa:5, fc:6*/
class Inst {
    uint32_t _d;
public:
    inline uint32_t d() { return _d;}
    int getv(int st, int ed);
    int getiv();
    void setd(int v);
    void setv(uint32_t);
    inline int op() { return getv(26,31);}
    void setop(uint32_t);
    inline int rs() { return getv(21,25);}
    void setrs(uint32_t);
    inline int rt() { return getv(16,20);}
    void setrt(uint32_t);
    inline int rd() { return getv(11,15);}
    void setrd(uint32_t);
    inline int sa() { return getv(6,10);}
    void setsa(uint32_t);
    inline int fc() { return getv(0,5);}
    void setfc(uint32_t);
    int trans2z(char*);
public:
};

class Bin2buf {
private:
    int fd, fsize, cpos;
    int ofd, ocpos;
    char* fbuf, *ofbuf;
private:
    int __file_size(int);
    int __zflush();
public:
    Bin2buf();
    int open_in_file(const char* sf);
    int open_out_file(const char* of);

    int get_inst(Inst& dst);
    int write_buf(const char*, int);
    ~Bin2buf();
};

