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
public:
    enum STATUS {
        NOTSET,
        NT,
        T
    };
private:
    unordered_map<int, pair<list<int>::iterator, pair<int, BTB::STATUS> > > M;
    list<int> mlist;
public:
    
    pair<int, BTB::STATUS> get(int k, int dst); 
    BTB::STATUS raw_get(int k);
    void set(int k, BTB::STATUS s);
    void get_last_src();
    void get_last_dst();
    int print(char* s);
};

class Reg {
    int M[32];
public:
    Reg();
    inline int get(int x) { return M[x];}
    inline void set(int x, int v) { M[x] = v;}
    int print(char* s);
};

struct RobCell {
    int id, typ, v, ans;
    bool ok, need_flush;
    RobCell();
};

struct WBCell {
    int id, typ, v, tv, time;
    WBCell(int, int, int, int, int);
    bool operator < (const WBCell& rhs) const;
};

class Simulator : public InstExecHelper {
    int __pc, __idseed, __clock;
    char* __strout; int offset = 0;
    bool active = true; /*if met break, active is false*/
    list<int> iq;
    BTB btb;
    Reg reg;
    MM* mem; 
    Ibuf* ibuf;
    Disassembler disa;

    map<int, Inst*> id2ist; /*every ist in the simulator*/
    list<int> rs; /*Reserve Station -- sids*/
    list<RobCell> rob;
    map<int, int> reg2id; /*reg is used by which id*/
    set<WBCell> wbs; 
private:
    void commit(int typ, int v, int tv);
    void free_tag(int typ, int v);
    void flush_from(int id);
    void will_flush_from(int id, int newpc);
    void write_res_back(WBCell w);
public:
    Simulator();
    inline void setMM(MM* m) { mem = m;}
    inline void set_strout(char* s) { __strout = s;}
    inline void setibuf(Ibuf* b) { ibuf=b;}
    inline int& pc() { return __pc;}
    int get_new_id(); /*get an global uniq id*/
    int issue();
    int fetch();
    int exec();
    int wb();
    int commit();
    inline int& clock() { return __clock;}
public:
    void exec_set(int id, int typ, int v, int exec_v);
    int exec_get(int typ, int v);
    void exec_break(int id);
    bool check_pre_sw(int id);
    bool check_pre_ac(int id);
    void linktag(int id);
    void linktag(Inst& ist);
    void might_jump(int id, int jtyp, int dst);
    void confirm_jump(int id, bool ok);
    int robprint(char* s);
    int rsprint(char* s);
    int iqprint(char* s);
    int holyprint(char* s, int clock);
    void run(); /*Run the simulator*/
};

#endif
