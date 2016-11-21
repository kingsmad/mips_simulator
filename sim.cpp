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

int BTB::get(int k) {
    auto c = M.find(k);
    if (c == M.end()) {
        while (M.size() > maxbtb) {
            int ky = mlist.front();
            mlist.erase(mlist.begin());
            M.erase(ky);
        }
        mlist.push_back(k);
        auto it = mlist.end(); --it;
        M.insert(make_pair(k, make_pair(it, 0)));

        return 0;
    } else {
        int v = c->second.second;
        auto lc = c->second.first;
        int ky = *lc;
        mlist.erase(lc);
        mlist.push_back(ky);
        lc = mlist.end(); --lc;
        M[k].first = lc;

        return v;
    }
}

void BTB::set(int k, int v) {
    int c = get(k);
    if (c == v) return;
    M[k].second = v;

    return;
}

int BTB::print(char* s) {
    int offset = 0;
    offset += sprintf(s+offset, "BTB:\n");  
    int cnt = 1;
    for (auto it: M) {
        int d1 = it.first;
        int d2 = it.second.second;
        int d3 = d2 > 0 ? 1 : 0;
        offset += sprintf(s+offset, "[Entry %d]:<%d,%d,%d>\n", \
                cnt, d1, d2, d3);
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
            offset += sprintf(s+offset, "    %d",  get(x));
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
    /*We assume mem addrs will > 32, thus no
     * overlapping would happend*/
    /*Dependencies*/
    for (int i=0; i<3; ++i) {
        if (ist.para_typ[i] == ist.REGISTER || \
                ist.para_typ[i] == ist.MEMORY) {
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
    if (ist.dst_typ == ist.MEMORY || ist.dst_typ == \
            ist.REGISTER) {
        reg2id[ist.dst_val] = ist.id();
        ist.fake_dst_typ = ist.ROBTAG;
        ist.fake_dst_val = ist.id();
    }
}

int Simulator::get_new_id() {
    __idseed += 7;
    return __idseed;
}

int Simulator::exec_get(int typ, int v) {
    assert(typ != Inst::__para_type::ROBTAG);
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

        /*If rs and rob both has seats*/
        if (rs.size() < maxrob && rob.size() < maxrob) {
            v.push_back(id);
            int idx = ist->id();
            rs.push_back(idx);

            /*update rob_tag*/
            RobCell c;
            c.id = idx; c.ok = false;
            /*If it is a BREAK, since 'break' has no actions
             * Nobody will update its' rob result status.
             * So, we mark it as completed in the initial stage*/
            /*if (ist->isbreak()) c.ok = true;*/
            rob.push_back(c);
            linktag(*ist); /*Register renaming*/
        }
    }

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
        bool ok = true;
        for (int i=0; i<3; ++i) {
            if (ist->para_typ[i] == ist->ROBTAG) {
                ok = false;
                break;
            }
        }
        /*it is ready*/
        if (ok)
            S.insert(ist);
    }
   
    for (Inst* p: S) {
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
        buginfo("wrong commit %d %d %d\n", \
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
    if (rob.empty()) { 
        buginfo("Time: %d, rob is empty()\n", clock());
        return -1;
    }
    RobCell c = rob.front();
    if (c.ok == false) {
        Inst* ist = id2ist[c.id];
        buginfo("%s rob head not ready!\n", ist->ori_str);
        return 0;
    }
    
    if (c.id == 19042) 
        buginfo("Catch u\n");
    /*Free resources*/
    rob.pop_front();
    auto it = find(rs.begin(), rs.end(), c.id);
    rs.erase(it);
    id2ist.erase(c.id); 
    //free_tag(c.typ, c.v);  

    /*Write results to real reg & mem*/
    commit(c.typ, c.v, c.ans);  
    buginfo("Time: %d, rob is non-empty\n", clock());
    return 0;
}

/*This is in IF stage*/
void Simulator::might_jump(int id, int dst) {
    Inst& ist = *id2ist[id];
    int z = btb.get(ist.textline());
    if (z == 0) return;
    pc() = z;
}

/*cancel actions that came after id*/
void Simulator::cancel_from_robtag(int id) {
    list<RobCell>::iterator p = rob.begin();
    while(p!=rob.end() && p->id!=id) ++p;
    assert(p!=rob.end());
    set<int> sids;
    while(p!=rob.end()) {
        sids.insert(p->id);
        ++p;
    }
    
    rob.erase(p, rob.end());/*erase from rob*/
    for (auto c:sids) { /*erase from rs*/
        auto it = find(rs.begin(), rs.end(), c);
        rs.erase(it);
    }
    /*Erase from reg2id*/
    vector<int> tk;
    for (auto c: reg2id) {
        if (sids.count(c.second) > 0) {
            tk.push_back(c.first); 
        }
    }
    for (int d: tk) reg2id.erase(d);
}

/*branch ist doesn't have WB-stage
 * But this happened at WB stage*/
void Simulator::confirm_jump(int id, int dst) {
    Inst& ist = *id2ist[id];
    int pdst = btb.get(ist.textline());
    /*Correct prediction*/
    if ((pdst>0&&dst) || (pdst==0&&!dst)) {
        for (RobCell c: rob) {
            if (c.id == id) {
                c.ok = true;
                return;
            }
        }
    } else if (pdst>0&&!dst) { /*false taken*/
        btb.set(ist.textline(), 0); 
        cancel_from_robtag(ist.id());
        /*Reverse pc*/
        Inst* k = id2ist[rob.back().id];
        pc() = k->textline() + 4;
    } else if (pdst==0&&dst) { /*false not-taken*/
        btb.set(ist.textline(), dst);
        cancel_from_robtag(ist.id());
        /*Jump to new pc*/
        pc() = dst;
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
    clock() = 0;
    offset = 0;
    while(1) {
        buginfo("Running circle: %d, PC: %d...\n", clock(), pc());
        int res[5]; memset(res, 0, sizeof(res));
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
