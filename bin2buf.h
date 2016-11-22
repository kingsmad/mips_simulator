#ifndef BIN2BUF_H
#define BIN2BUF_H
/*************************************************************************
    > File Name: bin2buf.h
************************************************************************/
#include <iostream>
#include <stdlib.h>
#include <map>
#include <functional>

class InstExecHelper {
public:
    virtual void exec_set(int id, int tpy, int v, int exec_v) = 0;
    virtual int exec_get(int tpy, int v) = 0;
    virtual void exec_break(int id) = 0;
    virtual void linktag(int id) = 0;
    virtual void might_jump(int id, int jtyp, int dst) = 0;
    virtual void confirm_jump(int id, bool ok) = 0;
};

using namespace std;
/*op:6, rs:5, rt:5, rd:5, sa:5, fc:6*/
class Inst {
    uint32_t _d;
    int __id;
    int __textline;
    int ebuf[3]; /*store status for exec*/
public:
    InstExecHelper* hp = 0;
    inline uint32_t d() { return _d;}
    int getv(int st, int ed); // get unsigned-int from st-ed;
    int getiv(); // return 0-15 bits as signed int.
    void setd(int v); // set _d;
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
    int trans2z(char*); // deprecated

    /*Designed for simulator*/
public:
    Inst();
    enum __para_type {
        NONE,
        REGISTER,
        MEMORY,
        MEMORY_CHAO,/*mem addr is not resolved*/
        ROBTAG, /*ID*/
        IMMEDIATE
    };
    /*Only 3 required, but left one for storeages*/
    int para_typ[4];
    int para_val[4];
    int dst_typ, dst_val;
    int fake_dst_typ, fake_dst_val;
    char ori_str[30];
    int res_status = 0;
    bool isbranch = false;
    inline int& id() { return __id;}
    inline int textline() { return __textline;}
    inline void settextline(int x) { __textline = x;}
    bool isjump();
    bool isbreak(); 
public:
    function<void()> exec;
};

class MM {
public:
    virtual int getmem(int x) = 0;
    virtual void setmem(int x, int v) = 0;
    virtual int mmprint(char* s) = 0;
};

class Ibuf {
public:
    virtual Inst* get_pc2ist(int pc) = 0;
    virtual int ibufprint(char* s) = 0; 
    virtual int write_buf(const char*, int) = 0;
};

// read && write binary-file from/to buffer.
class Bin2buf : public MM, public Ibuf {
private:
    int fd, fsize, cpos; // input
    int ofd, ocpos; // output
    char* fbuf, *ofbuf;

    /*For mm and ibuf*/
    map<int, Inst> pc2ist;
    map<int, int> pc2mem;
    int data_seg; /*Start addr of data-segment*/
    int data_end; /*End addr of data-segment*/
private:
    int __file_size(int);
    int __zflush(); // flush output buffer.
public:
    Bin2buf();
    int open_in_file(const char* sf);
    int open_out_file(const char* of);

    int get_inst(Inst& dst);
    void readall_inst();
    int write_buf(const char*, int);
    ~Bin2buf();
public:
    void setmem(int x, int v);
    int getmem(int x);
    Inst* get_pc2ist(int pc);
    int ibufprint(char* s);
    int mmprint(char* s);
};

#endif
