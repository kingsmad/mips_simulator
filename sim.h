#ifndef SIM_H
#define SIM_H
#include "scheduler.h"
#include "disassembler.h"
#include <list>
#include <map>
#include <cstdarg>
#include <unordered_map>
#include <set>

const int maxrs = 10;
const int maxrob = 6;
const int maxbtb = 16;


/*Defect in dealing with addrs*/
class BTB {
    unordered_map<int, pair<list<int>::iterator, int> > M;
    list<int> mlist;
public:
    int get(int k); 
    void set(int k, int v);
    void get_last_src();
    void get_last_dst();
    int print(char* s);
};

class Reg {
    int M[32];
public:
    inline int get(int x) { return M[x];}
    inline void set(int x, int v) { M[x] = v;}
    int print(char* s);
};

struct RobCell {
    int id, typ, v, ans;
    bool ok;
};

struct WBCell {
    int id, typ, v, tv;
    WBCell(int, int, int, int);
    bool operator < (const WBCell& rhs) const;
};

class Simulator : public InstExecHelper {
    int pc, __idseed;
    set<Inst*> iq;
    BTB btb;
    Reg reg;
    MM* mem; 
    Ibuf* ibuf;
    Disassembler disa;

    map<int, Inst*> id2ist;
    set<int> rs; /*Reserve Station -- sids*/
    list<RobCell> rob;
    map<int, int> reg2id; /*reg is used by which id*/
    set<WBCell> wbs; 
private:
    void commit(int typ, int v, int tv);
    void free_tag(int typ, int v);
    void cancel_from_robtag(int id);
public:
    Simulator();
    inline void setMM(MM* m) { mem = m;}
    inline void setibuf(Ibuf* b) { ibuf=b;}
    int get_new_id(); /*get an global uniq id*/
    void issue();
    void fetch();
    void exec();
    void wb();
    void commit();
public:
    void exec_set(int id, int typ, int v, int exec_v);
    int exec_get(int typ, int v);
    void linktag(int id);
    void linktag(Inst& ist);
    void might_jump(int id, int dst);
    void confirm_jump(int id, int b);
    int robprint(char* s);
    int rsprint(char* s);
    int iqprint(char* s);
    int holyprint(char* s, int clock);
};

#endif
