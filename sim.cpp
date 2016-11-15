#include "sim.h"
#include <vector>
#include <assert.h>
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

WBCell::WBCell(int d1, int d2, int d3, int d4) :  
    id(d1), typ(d2), v(d3), tv(d4)
{
     
}

bool WBCell::operator < (const WBCell& rhs) const {
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
    pc = 600;
    __idseed = 107;
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
            if (reg2id.count(p) > 0) {
                int x = reg2id[p];
                ist.para_typ[i] = ist.ROBTAG;
                ist.para_val[i] = x;
            }
        }
    }
    
    /*Results*/
    if (ist.dst_typ == ist.MEMORY || ist.dst_typ == \
            ist.REGISTER) {
        reg2id[ist.dst_val] = ist.getid();
        ist.fake_dst_typ = ist.ROBTAG;
        ist.fake_dst_val = ist.getid();
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
    wbs.insert(WBCell(id, typ, v, tv));
}

void Simulator::fetch() {
    Inst* ist = ibuf->get_pc2ist(pc); 
    disa.proc(*ist, 0, pc); /*bug*/
    if (ist->isbranch)
        ist->exec();
    iq.insert(ist);  
}

void Simulator::issue() {
    for (Inst* ist: iq) {
        if (rs.size() < maxrob && rob.size() < maxrob) {
            int idx = get_new_id();
            id2ist[idx] = ist;
            iq.erase(ist);
            rs.insert(idx);

            /*update rob_tag*/
            RobCell c;
            c.id = idx; c.ok = false;
            rob.push_back(c);
            linktag(ist->getid()); 
        }
    }
}

void Simulator::exec() {
    /*get ist which is ready*/
    set<Inst*> S;
    for (int x: rs) {
        Inst* ist = id2ist[x];
        bool ok = true;
        for (int i=0; i<3; ++i) {
            if (ist->para_typ[i] == ist->ROBTAG) {
                ok = false;
                break;
            }
        }
        if (ok)
            S.insert(ist);
    }
   
    for (Inst* p: S) {
        p->exec(); 
    }
}

void Simulator::wb() {
    for (WBCell w: wbs) {
        Inst& ist = *id2ist[w.id];
        /*update rob*/
        for (RobCell& c: rob) { /*change fake_dst to real dst*/
            if (w.id != c.id) continue;
            c.typ = ist.dst_typ;
            c.v = ist.dst_val;
            c.ans = w.tv;
            c.ok = true;
            break;
        }
        /*update reserver station*/
        for (int x: rs) {
            Inst& cst = *id2ist[x];
            for (int i=0; i<3; ++i) {
                if (cst.para_typ[i] == w.typ && \
                    cst.para_val[i] == w.v) {
                    cst.para_typ[i] = cst.IMMEDIATE;
                    cst.para_val[i] = w.tv;
                }
            }
        }
    }
    wbs.clear();
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
    assert(reg2id.count(v) > 0);
    reg2id.erase(v);
}

void Simulator::commit() {
    RobCell c = rob.front();
    if (c.ok == false) return;
    
    /*Free resources*/
    rob.pop_front();
    rs.erase(c.id);
    id2ist.erase(c.id); 
    free_tag(c.typ, c.v);  

    /*Write results to real reg & mem*/
    commit(c.typ, c.v, c.ans);  
}

/*This is in IF stage*/
void Simulator::might_jump(int id, int dst) {
    Inst& ist = *id2ist[id];
    int z = btb.get(ist.textline());
    if (z == 0) return;
    pc = z;
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
    for (auto c:sids) /*erase from rs*/
        rs.erase(c);
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
        cancel_from_robtag(ist.getid());
        /*Reverse pc*/
        Inst* k = id2ist[rob.back().id];
        pc = k->textline() + 4;
    } else if (pdst==0&&dst) { /*false not-taken*/
        btb.set(ist.textline(), dst);
        cancel_from_robtag(ist.getid());
        /*Jump to new pc*/
        pc = dst;
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
    for (Inst* ist: iq) {
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

    return offset;
}
