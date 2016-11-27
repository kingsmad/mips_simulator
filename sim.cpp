#include "sim.h"
#include <vector>
#include <algorithm>
#include <assert.h>
#include <string.h>
#include <set>
#include <map>

bool debug = true;
extern void buginfo(const char* f, ...);

using namespace std;

class Transit_Info {
public:
    /* In commit stage, how many rs or rob 
     * were canceled, they can't be used in
     * current clock()!*/
    int cloak_rs_cnt = 0;
    int cloak_rob_cnt = 0;
    int newpc = 0;
    int curpc = 0;
} tinfo;

pair<int, BTB::STATUS> BTB::get(int k, int dst) {
    auto c = M.find(k);
    if (c == M.end()) {
        while (M.size() > maxbtb) {
            int ky = mlist.front();
            mlist.erase(mlist.begin());
            M.erase(ky);
        }
        mlist.push_back(k);
        auto it = mlist.end(); --it;
        M.insert(make_pair(k, make_pair(it, make_pair(dst, NOTSET))));

        return make_pair(dst, NOTSET);
    } else {
        pair<int, BTB::STATUS> v = c->second.second;
        auto lc = c->second.first;
        int ky = *lc;
        mlist.erase(lc);
        mlist.push_back(ky);
        lc = mlist.end(); --lc;
        M[k].first = lc;

        return v;
    }
}

BTB::STATUS BTB::raw_get(int k) {
    auto c = M.find(k);
    if (c == M.end()) return BTB::STATUS::NOTSET;
    return c->second.second.second;
}

void BTB::set(int k, BTB::STATUS s) {
    /*we assert we have a get before*/
    assert(M.count(k) > 0);
    pair<int, BTB::STATUS> c = get(k, 0);

    if (c.second == s) return;
    M[k].second.second = s;
    return;
}

int BTB::print(char* s) {
    int offset = 0;
    offset += sprintf(s+offset, "BTB:\n");  
    int cnt = 1;
    for (auto it: M) {
        int d1 = it.first;
        int d2 = it.second.second.first;
        int d3 = it.second.second.second;
        if (d3 != BTB::STATUS::NOTSET) {
            d3 = (d3==BTB::STATUS::T) ? 1 : 0;
            offset += sprintf(s+offset, "[Entry %d]:<%d,%d,%d>\n", \
                    cnt, d1, d2, d3);
        } else
            offset += sprintf(s+offset, "[Entry %d]:<%d,%d,NotSet>\n",
                    cnt, d1, d2);
        ++cnt;
    }
    return offset;
}

Reg::Reg() {
    memset(M, 0, sizeof(M));
}

int Reg::print(char* s) {
    int offset = 0;
    offset += sprintf(s+offset, "Registers:\n");
    for (int i=0; i<4; ++i) {
        offset += sprintf(s+offset, "R%02d:", i*8);
        for (int j=0; j<8; ++j) {
            int x = i*8+j;
            offset += sprintf(s+offset, "\t%d",  get(x));
        }
        offset += sprintf(s+offset, "\n");
    }

    return offset;
}

WBCell::WBCell(int d1, int d2, int d3, int d4, int d5) :  
    id(d1), typ(d2), v(d3), tv(d4), time(d5)
{
     
}

bool WBCell::operator < (const WBCell& rhs) const {
    if (time < rhs.time) return true;
    if (time > rhs.time) return false;
    if (id < rhs.id) return true;
    if (id > rhs.id) return false;
    if (typ < rhs.typ) return true;
    if (typ > rhs.typ) return false;
    if (v < rhs.v) return true;
    if (v > rhs.v) return false;
    if (tv < rhs.tv) return true;

    return false;
}

RobCell::RobCell() {
    id = typ = v = ans = 0;
    ok = false;
    need_flush = false;
}

Simulator::Simulator() {
    __pc = 600;
    __idseed = 19000;
    __strout = (char*) malloc(1024);
}

void Simulator::linktag(int id) {
    Inst& ist = *id2ist[id];    
    linktag(ist);
}

void Simulator::linktag(Inst& ist) {
    /*update: memory no longer need re-mapping*/

    /*We assume mem addrs will > 32, thus no
     * overlapping would happend*/
    /*Dependencies*/

    for (int i=0; i<3; ++i) {
        if (ist.para_typ[i] == ist.REGISTER/* || \
                ist.para_typ[i] == ist.MEMORY*/) {
            int p = ist.para_val[i];
            if (reg2id.count(p) > 0) { /*dependency*/
                int x = reg2id[p];
                ist.para_typ[i] = ist.ROBTAG;
                ist.para_val[i] = x;
            } else if (ist.para_typ[i] == ist.REGISTER) { 
                /* No previous dependency, read from 
                 * register file */
                ist.para_typ[i] = ist.IMMEDIATE;
                ist.para_val[i] = reg.get(ist.para_val[i]);
            }
        }
    }
    
    /*output targets, Results*/
    if (/*ist.dst_typ == ist.MEMORY || */ist.dst_typ == \
            ist.REGISTER) {
        reg2id[ist.dst_val] = ist.id();
        ist.fake_dst_typ = ist.ROBTAG;
        ist.fake_dst_val = ist.id();
    } else { 
        /*No renaming happened*/
        ist.fake_dst_typ = ist.dst_typ;
        ist.fake_dst_val = ist.dst_val;
    }
}

int Simulator::get_new_id() {
    __idseed += 7;
    return __idseed;
}

int Simulator::exec_get(int typ, int v) {
    assert(typ != Inst::__para_type::ROBTAG);
    assert(typ != Inst::__para_type::NONE);
    switch (typ) {
        case Inst::__para_type::REGISTER:
            return reg.get(v);
        case Inst::__para_type::MEMORY:
            return mem->getmem(v);
        case Inst::__para_type::IMMEDIATE:
            return v;
        default:
            buginfo("exec_get wrong: %d %d\n", \
                    typ, v);
            return 0;
    }
}

void Simulator::exec_set(int id, int typ,\
        int v, int tv) {
    wbs.insert(WBCell(id, typ, v, tv, clock()+1));
}

void Simulator::exec_break(int id) {
    Inst* ist = id2ist[id];
    assert(ist->isbreak());
    wbs.insert(WBCell(id, -1, 0, 0, clock()+1)); 
}

/* Accoding to current pc, fetch ist from ibuffer
 * put ist into id2ist, and save its' id*/
int Simulator::fetch() {
    if (!active) {
        buginfo("No longer active now!\n");
        return -1;
    }

    /*Save ist into iq && make up for initialization*/
    Inst* ist = ibuf->get_pc2ist(pc()); 
    ist->id() = get_new_id();
    iq.push_back(ist->id());
    ist->hp = dynamic_cast<InstExecHelper*>(this);
    id2ist[ist->id()] = ist;

    if (ist->isbreak()) active = false;
    disa.proc(*ist, 0, pc());
    if (ist->isbranch) /*If branch, exec 1st stage*/
        ist->exec();

    buginfo("Time: %d, fetch is non-empty\n", clock());
    return 0;
}

/*Primary job for issue is to put ist into rs & rob*/
int Simulator::issue() {
    if (iq.empty()) return -1;
    vector<int> v;
    for (int id: iq) {
        /*get ist*/
        assert(id2ist.count(id) > 0);
        Inst* ist = id2ist[id];

        /*Break && Nop don't use RS station*/
        if (ist->isbreak() || ist->isnop()) {
            if (rob.size() + tinfo.cloak_rob_cnt < maxrob) {
                v.push_back(id);
                RobCell c;
                c.id = ist->id(); 
                c.ok = true; /*Onece issued, it can be committed*/
                rob.push_back(c);
                linktag(*ist);
            }
        } else if (rs.size() + tinfo.cloak_rs_cnt <maxrs && \
                rob.size() + tinfo.cloak_rob_cnt < maxrob) {
            /*Both rs and rob has seats*/
            v.push_back(id);
            int idx = ist->id();
            rs.push_back(idx);

            /*update rob_tag*/
            RobCell c;
            c.id = idx; c.ok = false;
            /*If it is a BREAK, since 'break' has no actions
             * Nobody will update its' rob result status.
             * we deal with this special case in wb stage*/
            /*if (ist->isbreak()) c.ok = true;*/
            rob.push_back(c);
            linktag(*ist); /*Register renaming*/
        }
    }

    /*Remove used ist*/
    for (int id: v) {
        auto it = find(iq.begin(), iq.end(), id);
        iq.erase(it);
    }

    return 0;
}

int Simulator::exec() {
    if (rs.empty()) return -1;

    /*get ist which is ready*/
    set<Inst*> S;

    /* To each ist, if no paras are robtag, and 
     * also the res_status is not 0 --- which means 
     * already completed ---. then it is ready*/
    for (int x: rs) {
        Inst* ist = id2ist[x];
        if (ist->res_status == 0) continue;
        assert(ist->res_status > 0);
        
        /*it is ready*/
        if (ist->isready())
            S.insert(ist);
    }

    /* Find the first mem-related ist that haven't 
     * calc address yet*/
    int mid = 0;
    for (RobCell& c: rob) {
        bool ok = false;
        Inst* t = id2ist[c.id];
        for (int i=0; i<3; ++i) {
            if (t->para_typ[i] == t->MEMORY || \
                    t->dst_typ == t->MEMORY) {
                ok = true;
                break;
            }
        }
        if (ok) {
            mid = c.id;
            break;
        }
    }
    
   
    for (Inst* p: S) {
        /*Special case for lw && sw*/
        /*Check if p is a lw || sw*/
        bool ok = false;
        for (int i=0; i<3; ++i) {
            if (p->para_typ[i] == p->MEMORY || \
                    p->dst_typ == p->MEMORY) {
                ok = true;
                break;
            }
        }

        if (ok && p->id()!=mid) {
            switch (p->res_status) {
                case 2: /*non-first && addres unkown*/
                    continue; /*skip it*/
                case 1:
                    assert(p->dst_typ!=p->MEMORY_CHAO);
                    if (p->dst_typ != p->MEMORY) {
                        /* Means this is not a SW.
                         * Then must be a load*/
                        /* Check if there is any store before
                         * it and not commit yet*/ 
                        bool a = false;
                        for (auto it=rob.begin(); \
                                it->id!=p->id(); ++it) {
                            Inst* t = id2ist[it->id];
                            if (t->dst_typ == t->MEMORY) {
                                a = true; break;
                            }
                        }
                        if (a) continue;
                    }
                    break;
                default:
                    break;
            }
        }

        if (p->res_status > 0) 
            p->exec(); 
    }

    return 0;
}

int Simulator::wb() {
    if (wbs.empty()) return -1;
    vector<WBCell> vw;
    for (WBCell w: wbs) {
        if (w.time != clock()) continue;
        
        vw.push_back(w);
        Inst& ist = *id2ist[w.id];

        /*check if w.id is a break ist*/
        if (ist.isbreak()) {
            for (RobCell& c: rob) {
                if (w.id == c.id) {
                    c.ok = true; 
                    break;
                }
            }
            continue;  
        }

        /*Normal ist, doing write back*/
        /*update rob*/
        for (RobCell& c: rob) { /*change fake_dst to real dst*/
            if (w.id != c.id) continue;
            c.typ = ist.dst_typ;
            c.v = ist.dst_val;
            c.ans = w.tv;
            c.ok = true;
            break;
        }

        /*update reserve station*/
        for (int x: rs) {
            Inst& cst = *id2ist[x];
            for (int i=0; i<3; ++i) {
                if (cst.para_typ[i] != ist.IMMEDIATE && \
                        cst.para_typ[i] == w.typ && \
                    cst.para_val[i] == w.v) {
                    cst.para_typ[i] = cst.IMMEDIATE;
                    cst.para_val[i] = w.tv;
                }
            }
        }
    
        /*Naive o(n), free registers*/
        for (auto it=reg2id.begin(); it!=reg2id.end(); ++it) {
            if (it->second == w.id) {
                reg2id.erase(it);
                break;
            }
        }
    }

    for (WBCell w: vw) 
        wbs.erase(w);
    //wbs.clear();
    return 0;
}

void Simulator::commit(int typ, int v, int tv) {
    if (typ == Inst::__para_type::REGISTER) {
        reg.set(v, tv);
    } else if (typ == Inst::__para_type::MEMORY) {
        mem->setmem(v, tv);
    } else {
        buginfo("Non-effect commit %d %d %d\n", \
                typ, v, tv);
        return;
    }
}

void Simulator::free_tag(int typ, int v) {
    /*since reg & mem are unified, typ is 
     * ignored*/ 
    //assert(reg2id.count(v) > 0);
    reg2id.erase(v);
}

int Simulator::commit() {
    /*Restor the tinfo*/
    tinfo.cloak_rs_cnt = 0;
    tinfo.cloak_rob_cnt = 0;

    if (rob.empty()) { 
        buginfo("Time: %d, rob is empty()\n", clock());
        return -1;
    }

    RobCell c = rob.front();
    Inst* ist = id2ist[c.id];
    
    /*Special cases*/
    if (ist->isjump()) { /*it's a jump*/
        /*Hard-code the dst*/
        //int d = ist->getv(0,25) << 2;
        btb.set(ist->textline(), BTB::STATUS::T);
    }
    /*if (ist->isnop() || ist->isbreak()) {
        c.ok = true; 
    }*/

    /*Check if the head is completed*/
    if (c.ok == false) {
        buginfo("Clock %d ,%s rob head not ready!\n", clock(), ist->ori_str);
        return 0;
    }

    /* When reach here, The head is ready to commit*/
    /* First check if this is a ist waiting for flush*/
    if (c.need_flush == true) {
        flush_from(c.id);
        return 0;
    }

    /*Free resources*/
    rob.pop_front(); ++tinfo.cloak_rob_cnt;
    auto it = find(rs.begin(), rs.end(), c.id);
    if (it != rs.end()) { /*nop && break doesn't have rs seat*/
        rs.erase(it);
        ++tinfo.cloak_rs_cnt;
    }
    id2ist.erase(c.id); 

    /*free register tag should be done in wb stage*/
    //free_tag(c.typ, c.v);  

    /*Write results to real reg & mem*/
    commit(c.typ, c.v, c.ans);  
    buginfo("Time: %d, rob is non-empty\n", clock());
    return 0;
}

/*This is in IF stage*/
/* jtyp == 0: indirect jump
 * jtyp == 1: direct jump*/
void Simulator::might_jump(int id, int jtyp, int dst) {
    /* Calc and save destination*/
    Inst& ist = *id2ist[id];
    dst = (jtyp) ? dst : (pc()+dst); /*dest we requested*/
    ist.para_val[3] = dst; /*save addr*/
    
    bool jump = false;
    pair<int, BTB::STATUS> z = btb.get(ist.textline(), dst);
    if (z.second == BTB::STATUS::NOTSET) {
        /*If it is a jump, always jump*/
        if (ist.isjump()) jump = true;
        else jump = false;
    } else if (z.second == BTB::STATUS::NT) {
        jump = false;
    } else {
        jump = true;
    }

    if (jump) { 
        pc() = dst;
    }
}

/*Mark this ist, it will be canceld in commit stage*/
void Simulator::will_flush_from(int id, int newpc) {
    /*Find RobCell from id*/ 
    list<RobCell>::iterator p = rob.begin();
    while(p!=rob.end() && p->id!=id) ++p;
    assert(p!=rob.end());
    
    p->need_flush = true;
    p->ok = true;
    tinfo.newpc = newpc;
}

/*cancel all actions that came after id*/
void Simulator::flush_from(int id) {
    /*Find id*/
    list<RobCell>::iterator p = rob.begin();
    while(p!=rob.end() && p->id!=id) ++p;
    assert(p!=rob.end());

    set<int> sids;
    for (auto q=p; q!=rob.end(); ++q) {
        sids.insert(q->id);
    }
    
    rob.erase(p, rob.end());/*erase from rob*/

    for (auto c:sids) { /*erase from rs*/
        auto it = find(rs.begin(), rs.end(), c);
        rs.erase(it); ++tinfo.cloak_rs_cnt;
    }

    /*Erase from reg2id*/
    vector<int> tk;
    for (auto c: reg2id) {
        if (sids.count(c.second) > 0) {
            tk.push_back(c.first); 
        }
    }
    for (int d: tk) reg2id.erase(d);

    /* Erase all iq, because they must came after 
     * current ist we want to flush*/
    iq.clear();

    /*update pc*/
    pc() = tinfo.newpc; 
}

/* In confirm_jump, btb will be updated, this
 * is in exec stage*/
void Simulator::confirm_jump(int id, bool ok) {
    /* Retrieve jump address from ist*/
    Inst& ist = *id2ist[id];
    int dst = ist.para_val[3];

    BTB::STATUS pre_res = btb.raw_get(id);
    /* If this is jump, the record was not in btb
     * yet, thus pdst is always 0*/
    if (ist.isjump()) { /*keep this*/ 
        pre_res = BTB::STATUS::T;
    }
    if (pre_res == BTB::STATUS::NOTSET) {
        pre_res = BTB::STATUS::NT;
    }
    bool pres = (pre_res == BTB::STATUS::T) ? true : false;

    /*Correct prediction*/
    if (ok == pres) {
        /*update the rob*/
        for (RobCell& c: rob) {
            if (c.id == id) {
                c.ok = true;
                return;
            }
        }
    } else if (pres && !ok) { /*a false taken*/
        btb.set(ist.textline(), BTB::STATUS::NT); 
        will_flush_from(ist.id(), ist.textline()+4); 
    } else if (!pres && ok) { /*a false not-taken*/
        btb.set(ist.textline(), BTB::STATUS::T);
        will_flush_from(ist.id(), dst);
    }
}

int Simulator::rsprint(char* s) {
    int offset = 0;  
    offset += sprintf(s+offset, "RS:\n");
    for (int c: rs) {
        Inst* ist = id2ist[c];
        offset += sprintf(s+offset, "[%s]\n", ist->ori_str);
    }
    return offset;
}

int Simulator::robprint(char* s) {
    int offset = 0;
    offset += sprintf(s+offset, "ROB:\n");
    for (RobCell c: rob) {
        Inst* ist = id2ist[c.id];
        offset += sprintf(s+offset, "[%s]\n", ist->ori_str);
    }
    return offset;
}

int Simulator::iqprint(char* s) {
    int offset = 0;
    offset += sprintf(s+offset, "IQ:\n");
    for (int id: iq) {
        Inst* ist = id2ist[id];
        offset += sprintf(s+offset, "[%s]\n", ist->ori_str);
    }
    return offset;
}

int Simulator::holyprint(char* s, int clock) {
    int offset = 0;
    offset += sprintf(s+offset, "Cycle <%d>:\n", clock);
    offset += iqprint(s+offset);
    offset += rsprint(s+offset);
    offset += robprint(s+offset);
    offset += btb.print(s+offset);
    offset += reg.print(s+offset);
    offset += mem->mmprint(s+offset);

    /*Set size of strout to very large, so we can flush
     * only onece here*/
    ibuf->write_buf(s, offset);
    offset = 0;
    return offset;
}

void Simulator::run() {
    clock() = 1;
    offset = 0;
    while(1) {
        buginfo("Running circle: %d, PC: %d...\n", clock(), pc());
        int res[5]; memset(res, 0, sizeof(res));
        tinfo.curpc = pc(); /*store current cycle pc*/
        if (clock() == 38) {
            printf("Hi");
        }
        res[0] = commit();
        res[1] = wb();
        res[2] = exec();
        res[3] = issue();
        res[4] = fetch();

        if (res[0] != -1) buginfo("commit non-empty\n");
        if (res[1] != -1) buginfo("wb non-empty\n");
        if (res[2] != -1) buginfo("exec non-empty\n");
        if (res[3] != -1) buginfo("issue non-empty\n");
        if (res[4] != -1) buginfo("fetch non-empty\n");

        offset += holyprint(__strout+offset, clock());
        clock() = clock() + 1;
        if (res[0]==-1 && res[1]==-1 && res[2]==-1 && res[3]==-1 && res[4]==-1) break;
    }
}
