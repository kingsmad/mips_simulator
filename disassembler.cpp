#include <iostream>
#include "bin2buf.h"
#include <string.h>
#include <arpa/inet.h>
#include "disassembler.h"
#include "sim.h"
using namespace std;

extern void buginfo(const char *s, ...);
typedef int (Disassembler::*DisF)(Inst&, char*);
bool Disassembler::_map_inited = false;
DisF Disassembler::_dis_map[_ref_map_sz];
DisF Disassembler::_dis_submap_zero[_ref_map_sz];
DisF Disassembler::_dis_submap_one[_ref_map_sz];

Disassembler::Disassembler() {
    // single thread, don't worry.
    if (!_map_inited) {
        __init_dis_map(); 
        _map_inited = true;
    }
    _on_data_seg = false;
}

int Disassembler::trans2z(Inst& ist, char* s) {
    char b[32];
    for(int i=0; i<32; ++i) {
        b[31-i] = ((ist.d() & (1<<i)) != 0) ? '1' : '0';
    }
    char* p = &b[0], *q = s; 
    int cnt = 0;
    while(cnt < 6) {
        int sz = (cnt==0 || cnt==5) ? 6:5;
        memcpy(q, p, sz);
        q += sz, p += sz;
        *q++ = ' ';
        ++cnt;
    }

    return --q-s;
}

int Disassembler::trans2t(Inst& ist, char* s) {
    DisF f = _dis_map[ist.op()]; 
    return (this->*f)(ist, s);
}

int Disassembler::proc(Inst& ist, char* s, int& pc) {
    if (_on_data_seg) 
        return proc_data(ist, s, pc);
    return proc_inst(ist, s, pc);
}

int Disassembler::proc_inst(Inst& ist, char* s, int& pc) {
    /*Record current pc : textline*/
    ist.settextline(pc);
    int p = 0;
    if (s) p += trans2z(ist, s);
    if (s) p += sprintf(s+p, " %d ", pc);
    p += trans2t(ist, s+p); 

    if (s) s[p++] = '\n';
    pc += 4;

    //ist.hp = this->hp;
    //buginfo("ist.hp is: %lld\n", ist.hp);
    return p;
}

int Disassembler::proc_data(Inst& ist, char* s, int& pc) {
    for(int i=0; i<32; ++i) {
        if ((ist.d() & (1<<i))) 
            s[31-i] = '1';
        else 
            s[31-i] = '0';
    }

    int t = sprintf(s+32, " %d %d\n", pc, ist.d());
    pc += 4;
    return t+32; 
}

// init op map
void Disassembler::__init_dis_map() {
    DisF* m = &_dis_map[0];
    fill(m, m+_ref_map_sz, &Disassembler::dis_unknown);
    m[0] = &Disassembler::__dis_sub1_zero;
    m[1] = &Disassembler::__dis_sub1_one;
    m[43] = &Disassembler::dis_sw;
    m[35] = &Disassembler::dis_lw;
    m[2] = &Disassembler::dis_j;    
    m[4] = &Disassembler::dis_beq;
    m[5] = &Disassembler::dis_bne;
    m[7] = &Disassembler::dis_bgtz;
    m[6] = &Disassembler::dis_blez;
    m[8] = &Disassembler::dis_addi;
    m[9] = &Disassembler::dis_addiu;
    m[10] = &Disassembler::dis_slti;
    __init_dis_submaps();
}

// init sub maps, currently we have only zero submap;
void Disassembler::__init_dis_submaps() {
    DisF* m = &_dis_submap_zero[0];
    fill(m, m+_ref_map_sz, &Disassembler::dis_unknown);
    m[13] = &Disassembler::dis_break;
    m[42] = &Disassembler::dis_slt;
    m[43] = &Disassembler::dis_sltu;
    m[0] = &Disassembler::dis_sll_nop;
    m[2] = &Disassembler::dis_srl;
    m[3] = &Disassembler::dis_sra;
    m[34] = &Disassembler::dis_sub;
    m[35] = &Disassembler::dis_subu;
    m[32] = &Disassembler::dis_add;
    m[33] = &Disassembler::dis_addu;
    m[36] = &Disassembler::dis_and;
    m[37] = &Disassembler::dis_or;
    m[38] = &Disassembler::dis_xor;
    m[39] = &Disassembler::dis_nor;
}

int Disassembler::__str2six(const char* s) {
    uint32_t d = 0;
    for(int i=0; i<6; ++i) {
        if (s[i] == '1') 
            d = d | (1<<(5-i));
    }
    return d;
}

int Disassembler::dis_unknown(Inst& ist, char* s) {
    return sprintf(s, "Unknown Instruction\n"); 
}

int Disassembler::__dis_sub1_zero(Inst& ist, char* s) {
    DisF f = _dis_submap_zero[ist.fc()];
    return (this->*f)(ist, s);
}

int Disassembler::__dis_sub1_one(Inst& ist, char* s) {
    int v = ist.getv(16,20);
    if (v == 1) 
        return dis_bgez(ist, s);
    else if (v ==0) 
        return dis_bltz(ist, s);
    return -1;
}

// 101011
int Disassembler::dis_sw(Inst& ist, char* s) {
    ist.para_typ[0] = ist.REGISTER;
    ist.para_val[0] = ist.rt(); /*we know it's the reg val*/
    ist.para_typ[1] = ist.IMMEDIATE;
    ist.para_val[1] = ist.getiv();
    ist.para_typ[2] = ist.REGISTER;
    ist.para_val[2] = ist.rs();
    ist.dst_typ = ist.MEMORY_CHAO;
    ist.res_status = 2;
    ist.exec = [&]() {
        --ist.res_status;
        if (ist.res_status == 1) {
            int r = ist.hp->exec_get(ist.para_typ[2], ist.para_val[2]);
            ist.para_typ[2] = ist.NONE;
            /*mem addr is resolved, we can re-link tags now*/
            ist.dst_typ = ist.MEMORY;
            ist.dst_val = r + ist.para_val[1];
            /*link second time*/
            ist.hp->linktag(ist.id()); /*generate fake_dst_typ & fake_dst_val*/ 
            return;
        }
        /*Second stage*/ 
        int r = ist.hp->exec_get(ist.para_typ[0], ist.para_val[0]);
        ist.hp->exec_set(ist.id(), ist.fake_dst_typ, ist.fake_dst_val, r);
    };

    if (!s) 
        return sprintf(ist.ori_str, "SW R%d, %d(R%d)", ist.rt(), \
                ist.getiv(), ist.rs());
    return sprintf(s, "SW R%d, %d(R%d)", ist.rt(), \
            ist.getiv(), ist.rs());
}

// 100011
int Disassembler::dis_lw(Inst& ist, char* s) {
    ist.para_typ[0] = ist.NONE;
    ist.para_typ[1] = ist.IMMEDIATE;
    ist.para_typ[2] = ist.REGISTER;
    ist.para_val[1] = ist.getiv();
    ist.para_val[2] = ist.rs();
    /*dst info*/
    ist.dst_typ = ist.REGISTER;
    ist.dst_val = ist.rt();
    ist.res_status = 2;
    ist.exec = [&]() {
        --ist.res_status;
        if (ist.res_status == 1) {
            int p = ist.hp->exec_get(ist.para_typ[2], ist.para_val[2]);
            p += ist.para_val[1];
            ist.para_typ[0] = ist.MEMORY;
            ist.para_val[0] = p;
            ist.para_typ[1] = ist.para_typ[2] = ist.NONE;
            ist.hp->linktag(ist.id());
            return;
        }
        /*Second stage*/
        int r = ist.hp->exec_get(ist.para_typ[0], ist.para_val[0]);
        ist.hp->exec_set(ist.id(), ist.fake_dst_typ, ist.fake_dst_val, r);
    };

    if (!s)
        return sprintf(ist.ori_str, "LW R%d, %d(R%d)", ist.rt(), \
                ist.getiv(), ist.rs());
    else
        return sprintf(s, "LW R%d, %d(R%d)", ist.rt(), \
                ist.getiv(), ist.rs());
}

// 000010 bug here
int Disassembler::dis_j(Inst& ist, char* s) {
    uint32_t d = ist.getv(0, 25);
    d = d << 2;
    /*ist.para_typ[0] = ist.IMMEDIATE;
    ist.para_val[0] = ist.rt();*/
    ist.res_status = 2;
    ist.isbranch = true;
    ist.dst_typ = ist.NONE;
    ist.exec = [&]() {
        --ist.res_status;
        if (ist.res_status == 1) {
            /*0: indirect jump, 1: direct jump*/
            ist.hp->might_jump(ist.id(), 1, ist.getv(0,25)<<2);
            return;
        }
        ist.hp->confirm_jump(ist.id(), true);
    };

    if (!s) 
        return sprintf(ist.ori_str, "J #%d", d);

    return sprintf(s, "J #%d", d);
}

//000100 the offset requires 2 bits shift.
int Disassembler::dis_beq(Inst& ist, char* s) {
    ist.para_typ[0] = ist.para_typ[1] = ist.REGISTER;
    ist.para_typ[2] = ist.IMMEDIATE;
    ist.para_val[0] = ist.rs();
    ist.para_val[1] = ist.rt();
    ist.para_val[2] = ist.getiv()<<2;
    ist.res_status = 2;
    ist.dst_typ = ist.NONE;
    ist.isbranch = true;
    ist.exec = [&]() {
        --ist.res_status;
        if (ist.res_status == 1) { /* IF stage */
            ist.hp->might_jump(ist.id(), 0, ist.para_val[2]);
            return;
        }
        int p = ist.hp->exec_get(ist.para_typ[0], ist.para_val[0]);
        int q = ist.hp->exec_get(ist.para_typ[1], ist.para_val[1]);
        if (p == q) ist.hp->confirm_jump(ist.id(), true);
        else ist.hp->confirm_jump(ist.id(), false);
    };

    if (!s)
        return sprintf(ist.ori_str, "BEQ R%d, R%d, #%d", ist.rs(), \
                ist.rt(), ist.getiv()<<2);

    return sprintf(s, "BEQ R%d, R%d, #%d", ist.rs(), \
            ist.rt(), ist.getiv()<<2);
}

// 000101
int Disassembler::dis_bne(Inst& ist, char* s) {
    ist.para_typ[0] = ist.para_typ[1] = ist.REGISTER;
    ist.para_typ[2] = ist.IMMEDIATE;
    ist.para_val[0] = ist.rs();
    ist.para_val[1] = ist.rt();
    ist.para_val[2] = ist.getiv()<<2;
    ist.res_status = 2;
    ist.isbranch = true;
    ist.dst_typ = ist.NONE;
    ist.exec = [&]() {
        --ist.res_status;
        if (ist.res_status == 1) { /* IF stage */
            ist.hp->might_jump(ist.id(), 0, ist.para_val[2]);
            return;
        }
        int p = ist.hp->exec_get(ist.para_typ[0], ist.para_val[0]);
        int q = ist.hp->exec_get(ist.para_typ[1], ist.para_val[1]);
        if (p != q) ist.hp->confirm_jump(ist.id(), true);
        else ist.hp->confirm_jump(ist.id(), false);
    };

    if (!s) 
        return sprintf(ist.ori_str, "BNE R%d, R%d, #%d", ist.rs(), \
                ist.rt(), ist.getiv()<<2);

    return sprintf(s, "BNE R%d, R%d, #%d", ist.rs(), \
            ist.rt(), ist.getiv()<<2);
}


// 000001
int Disassembler::dis_bgez(Inst& ist, char* s) {
    ist.para_typ[0] = ist.REGISTER;
    ist.para_val[0] = ist.rs();
    ist.para_typ[1] = ist.IMMEDIATE;
    ist.para_val[1] = ist.getiv()<<2;
    ist.res_status = 2;
    ist.isbranch = true;
    ist.dst_typ = ist.NONE;
    ist.exec = [&]() {
        --ist.res_status;
        if (ist.res_status == 1) { /*IF stage*/
            ist.hp->might_jump(ist.id(), 0, ist.para_val[1]);
            return;
        }
        int p = ist.hp->exec_get(ist.para_typ[0], ist.para_val[0]);
        if (p >= 0) ist.hp->confirm_jump(ist.id(), true);
        else ist.hp->confirm_jump(ist.id(), false);
    };

    if (!s)
        return sprintf(ist.ori_str, "BGEZ R%d, #%d", ist.rs(), \
                ist.getiv()<<2);
    return sprintf(s, "BGEZ R%d, #%d", ist.rs(), \
            ist.getiv()<<2);
}

// 000111
int Disassembler::dis_bgtz(Inst& ist, char* s) {
    ist.para_typ[0] = ist.REGISTER;
    ist.para_val[0] = ist.rs();
    ist.para_typ[1] = ist.IMMEDIATE;
    ist.para_val[1] = ist.getiv()<<2;
    ist.res_status = 2;
    ist.dst_typ = ist.NONE;
    ist.isbranch = true;
    ist.exec = [&]() {
        --ist.res_status;
        if (ist.res_status == 1) { /*IF stage*/
            ist.hp->might_jump(ist.id(), 0, ist.para_val[1]);
            return;
        }
        int p = ist.hp->exec_get(ist.para_typ[0], ist.para_val[0]);
        if (p > 0) ist.hp->confirm_jump(ist.id(), true);
        else ist.hp->confirm_jump(ist.id(), false);
    };

    if (!s) 
        return sprintf(ist.ori_str, "BGTZ R%d, #%d", ist.rs(), \
                ist.getiv()<<2);
    return sprintf(s, "BGTZ R%d, #%d", ist.rs(), \
            ist.getiv()<<2);
}

// 000110
int Disassembler::dis_blez(Inst& ist, char* s) {
    ist.para_typ[0] = ist.REGISTER;
    ist.para_val[0] = ist.rs();
    ist.para_typ[1] = ist.IMMEDIATE;
    ist.para_val[1] = ist.getiv()<<2;
    ist.res_status = 2;
    ist.dst_typ = ist.NONE;
    ist.isbranch = true;
    ist.exec = [&]() {
        --ist.res_status;
        if (ist.res_status == 1) { /*IF stage*/
            ist.hp->might_jump(ist.id(), 0, ist.para_val[1]);
            return;
        }
        int p = ist.hp->exec_get(ist.para_typ[0], ist.para_val[0]);
        if (p <= 0) ist.hp->confirm_jump(ist.id(), true);
        else ist.hp->confirm_jump(ist.id(), false); 
    };

    if (!s)
        return sprintf(ist.ori_str, "BLEZ R%d, #%d", ist.rs(), \
                ist.getiv()<<2);

    return sprintf(s, "BLEZ R%d, #%d", ist.rs(), \
            ist.getiv()<<2);
}

// 000001
int Disassembler::dis_bltz(Inst& ist, char* s) {
    ist.para_typ[0] = ist.REGISTER;
    ist.para_val[0] = ist.rs();
    ist.para_typ[1] = ist.IMMEDIATE;
    ist.para_val[1] = ist.getiv()<<2;
    ist.res_status = 2;
    ist.dst_typ = ist.NONE;
    ist.isbranch = true;
    ist.exec = [&]() {
        --ist.res_status;
        if (ist.res_status == 1) { /*IF stage*/
            ist.hp->might_jump(ist.id(), 0, ist.para_val[1]);
            return;
        }
        int p = ist.hp->exec_get(ist.para_typ[0], ist.para_val[0]);
        if (p < 0) ist.hp->confirm_jump(ist.id(), true);
        else ist.hp->confirm_jump(ist.id(), false);
    };

    if (!s)
        return sprintf(ist.ori_str, "BLTZ R%d, #%d", ist.rs(), \
                ist.getiv()<<2);

    return sprintf(s, "BLTZ R%d, #%d", ist.rs(), \
            ist.getiv()<<2);
}

// 001000
int Disassembler::dis_addi(Inst& ist, char* s) {
    ist.para_typ[0] = ist.NONE;
    ist.para_typ[1] = ist.REGISTER;
    ist.para_typ[2] = ist.IMMEDIATE;
    ist.para_val[0] = ist.rt();
    ist.para_val[1] = ist.rs();
    ist.para_val[2] = ist.getiv();
    ist.dst_typ = ist.REGISTER;
    ist.dst_val = ist.rt(); 
    ist.res_status = 1;
    ist.exec = [&]() {
        --ist.res_status;
        int q = ist.hp->exec_get(ist.para_typ[1], ist.para_val[1]);
        int p = q + ist.para_val[2];
        ist.hp->exec_set(ist.id(), ist.fake_dst_typ, ist.fake_dst_val, p);
    };

    if (!s)
        return sprintf(ist.ori_str, "ADDI R%d, R%d, #%d", ist.rt(), \
                ist.rs(), ist.getiv());

    return sprintf(s, "ADDI R%d, R%d, #%d", ist.rt(), \
            ist.rs(), ist.getiv());
}

// 001001
int Disassembler::dis_addiu(Inst& ist, char* s) {
    ist.para_typ[0] = ist.NONE;
    ist.para_typ[1] = ist.REGISTER;
    ist.para_typ[2] = ist.IMMEDIATE;
    ist.para_val[0] = ist.rt();
    ist.para_val[1] = ist.rs();
    ist.para_val[2] = ist.getiv();
    ist.dst_typ = ist.REGISTER;
    ist.dst_val = ist.rt(); 
    ist.res_status = 1;
    ist.exec = [&]() {
        --ist.res_status;
        uint32_t p = ist.hp->exec_get(ist.para_typ[0], ist.para_val[0]);
        uint32_t q = ist.hp->exec_get(ist.para_typ[1], ist.para_val[1]);
        p += q + ist.para_val[2];
        ist.hp->exec_set(ist.id(), ist.fake_dst_typ, ist.fake_dst_val, p);
    };

    if (!s)
        return sprintf(ist.ori_str, "ADDIU R%d, R%d, #%d", ist.rt(), \
                ist.rs(), ist.getiv());
    return sprintf(s, "ADDIU R%d, R%d, #%d", ist.rt(), \
            ist.rs(), ist.getiv());
}

// 000000 - last6: 001101 
/*bug Waiting for fix*/
int Disassembler::dis_break(Inst& ist, char* s) {
    this->_on_data_seg = true;
    ist.para_typ[0] = ist.para_typ[1] = ist.para_typ[2] = ist.NONE;
    ist.dst_typ = ist.NONE;
    ist.fake_dst_typ = ist.NONE;
    ist.res_status = 1;
    ist.exec = [&]() {
        --ist.res_status; 
        ist.hp->exec_break(ist.id());
    };

    if (!s) 
        return sprintf(ist.ori_str, "BREAK");
    return sprintf(s, "BREAK");
}

// 000000 - last6: 101010
int Disassembler::dis_slt(Inst& ist, char* s) {
    ist.para_typ[0] = ist.NONE;
    ist.para_typ[1] = ist.para_typ[2] = \
                      ist.REGISTER;
    ist.para_val[0] = ist.rd();
    ist.para_val[1] = ist.rs();
    ist.para_val[2] = ist.rt();
    ist.dst_typ = ist.REGISTER;
    ist.dst_val = ist.rd();
    ist.res_status = 1;
    ist.exec = [&]() {
        --ist.res_status;
        int p = ist.hp->exec_get(ist.para_typ[1], ist.para_val[1]);
        int q = ist.hp->exec_get(ist.para_typ[2], ist.para_val[2]);
        if (p < q)
            ist.hp->exec_set(ist.id(), ist.fake_dst_typ, ist.fake_dst_val, 1);
        else 
            ist.hp->exec_set(ist.id(), ist.fake_dst_typ, ist.fake_dst_val, 0);
    };
    if (!s)
        return sprintf(ist.ori_str, "SLT R%d, R%d, R%d", ist.rd(), \
                ist.rs(), ist.rt());
    return sprintf(s, "SLT R%d, R%d, R%d", ist.rd(), \
            ist.rs(), ist.rt());
}

// 001010
int Disassembler::dis_slti(Inst& ist, char* s) {
    ist.para_typ[0] = ist.NONE;
    ist.para_typ[1] = ist.REGISTER;
    ist.para_typ[2] = ist.IMMEDIATE;
    ist.para_val[0] = ist.rt();
    ist.para_val[1] = ist.rs();
    ist.para_val[2] = ist.getiv();
    ist.dst_typ = ist.REGISTER;
    ist.dst_val = ist.rt();
    ist.res_status = 1;
    ist.exec = [&]() {
        --ist.res_status;
        int p = ist.hp->exec_get(ist.para_typ[1], ist.para_val[1]);
        int q = ist.getiv();
        if (p < q)
            ist.hp->exec_set(ist.id(), ist.fake_dst_typ, ist.fake_dst_val, 1);
        else 
            ist.hp->exec_set(ist.id(), ist.fake_dst_typ, ist.fake_dst_val, 0);
    };

    if (!s)
        return sprintf(ist.ori_str, "SLTI R%d, R%d, #%d", ist.rt(), \
                ist.rs(), ist.getiv());
    return sprintf(s, "SLTI R%d, R%d, #%d", ist.rt(), \
            ist.rs(), ist.getiv());
}

// 000000 - last6: 101011
int Disassembler::dis_sltu(Inst& ist, char* s) {
    ist.para_typ[0] = ist.NONE;
    ist.para_typ[1] = ist.para_typ[2] = \
                      ist.REGISTER;
    ist.para_val[0] = ist.rd();
    ist.para_val[1] = ist.rs();
    ist.para_val[2] = ist.rt();
    ist.dst_typ = ist.REGISTER;
    ist.dst_val = ist.rd();
    ist.res_status = 1;
    ist.exec = [&]() {
        --ist.res_status;
        uint32_t p = ist.hp->exec_get(ist.para_typ[1], ist.para_val[1]);
        uint32_t q = ist.hp->exec_get(ist.para_typ[2], ist.para_val[2]);
        if (p < q)
            ist.hp->exec_set(ist.id(), ist.fake_dst_typ, ist.fake_dst_val, 1);
        else 
            ist.hp->exec_set(ist.id(), ist.fake_dst_typ, ist.fake_dst_val, 0);
    };

    if (!s)
        return sprintf(ist.ori_str, "SLT R%d, R%d, R%d", ist.rd(), \
                ist.rs(), ist.rt());
    return sprintf(s, "SLTU R%d, R%d, R%d", ist.rd(), \
            ist.rs(), ist.rt());
}

// 000000 - last6: 000000
// overlap with nop
int Disassembler::dis_sll_nop(Inst& ist, char* s) {
    if (ist.d() == 0) {
        ist.res_status = 1;
        ist.exec = [&]() {
            --ist.res_status;
            ist.hp->exec_set(ist.id(), ist.NONE, 0, 0); 
        };

        if (!s)
            return sprintf(ist.ori_str, "NOP");

        return sprintf(s, "NOP");
    }  
    ist.para_typ[0] = ist.NONE;
    ist.para_typ[1] = ist.REGISTER;
    ist.para_typ[2] = ist.IMMEDIATE;
    ist.para_val[0] = ist.rd();
    ist.para_val[1] = ist.rt();
    ist.para_val[2] = ist.sa();
    ist.dst_typ = ist.REGISTER;
    ist.dst_val = ist.rd();
    ist.res_status = 1;
    ist.exec = [&]() {
        --ist.res_status;
        int q = ist.hp->exec_get(ist.para_typ[1], ist.para_val[1]);
        q = q << ist.para_val[2];
        ist.hp->exec_set(ist.id(), ist.fake_dst_typ, ist.fake_dst_val, q);
    };

    if (!s)
        return sprintf(&ist.ori_str[0], "SLL R%d, R%d, #%d", ist.rd(), \
            ist.rt(), ist.sa());

    return sprintf(s, "SLL R%d, R%d, #%d", ist.rd(), \
            ist.rt(), ist.sa());
}

// 000000 - last6: 000010
int Disassembler::dis_srl(Inst& ist, char* s) {
    ist.para_typ[0] = ist.NONE;
    ist.para_typ[1] = ist.REGISTER;
    ist.para_typ[2] = ist.IMMEDIATE;
    ist.para_val[0] = ist.rd();
    ist.para_val[1] = ist.rt();
    ist.para_val[2] = ist.sa();
    ist.dst_typ = ist.REGISTER;
    ist.dst_val = ist.rd();
    ist.res_status = 1;
    ist.exec = [&]() {
        --ist.res_status;
        int q = ist.hp->exec_get(ist.para_typ[1], ist.para_val[1]);
        int c = ist.para_val[2];
        q = q >> c;
        /*It is logical shift*/
        for (int i=0; i<c; ++i)
            q = q & (~(1<<(31-i)));
        ist.hp->exec_set(ist.id(), ist.fake_dst_typ, ist.fake_dst_val, q);
    };

    if (!s)
        return sprintf(ist.ori_str, "SRL R%d, R%d, #%d", ist.rd(), \
                ist.rt(), ist.sa());
    return sprintf(s, "SRL R%d, R%d, #%d", ist.rd(), \
            ist.rt(), ist.sa());
}

// 000000 - last6: 000011
int Disassembler::dis_sra(Inst& ist, char* s) {
    ist.para_typ[0] = ist.NONE;
    ist.para_typ[1] = ist.REGISTER;
    ist.para_typ[2] = ist.IMMEDIATE;
    ist.para_val[0] = ist.rd();
    ist.para_val[1] = ist.rt();
    ist.para_val[2] = ist.sa();
    ist.dst_typ = ist.REGISTER;
    ist.dst_val = ist.rd();
    ist.res_status = 1;
    ist.exec = [&]() {
        --ist.res_status;
        int q = ist.hp->exec_get(ist.para_typ[1], ist.para_val[1]);
        int c = ist.para_val[2];
        q = q >> c;
        ist.hp->exec_set(ist.id(), ist.fake_dst_typ, ist.fake_dst_val, q);
    };

    if (!s)
        return sprintf(ist.ori_str, "SRA R%d, R%d, #%d", ist.rd(), \
                ist.rt(), ist.sa());
    return sprintf(s, "SRA R%d, R%d, #%d", ist.rd(), \
            ist.rt(), ist.sa());
}

// 000000 - last6: 100010
int Disassembler::dis_sub(Inst& ist, char* s) {
    ist.para_typ[0] = ist.NONE;
    ist.para_typ[1] = ist.para_typ[2] \
                      = ist.REGISTER;
    ist.para_val[0] = ist.rd();
    ist.para_val[1] = ist.rs();
    ist.para_val[2] = ist.rt();
    ist.dst_typ = ist.REGISTER;
    ist.dst_val = ist.rd();
    ist.res_status = 1;
    ist.exec = [&]() {
        --ist.res_status;
        int p = ist.hp->exec_get(ist.para_typ[1], ist.para_val[1]);
        int q = ist.hp->exec_get(ist.para_typ[2], ist.para_val[2]);
        ist.hp->exec_set(ist.id(), ist.fake_dst_typ, ist.fake_dst_val, p-q);
    };

    if (!s)
        return sprintf(ist.ori_str, "SUB R%d, R%d, R%d", ist.rd(), \
                ist.rs(), ist.rt());
    return sprintf(s, "SUB R%d, R%d, R%d", ist.rd(), \
            ist.rs(), ist.rt());
}

// 000000 - last6: 100011
int Disassembler::dis_subu(Inst& ist, char* s) {
    ist.para_typ[0] = ist.NONE;
    ist.para_typ[1] = ist.para_typ[2] \
                      = ist.REGISTER;
    ist.para_val[0] = ist.rd();
    ist.para_val[1] = ist.rs();
    ist.para_val[2] = ist.rt();
    ist.dst_typ = ist.REGISTER;
    ist.dst_val = ist.rd();
    ist.res_status = 1;
    ist.exec = [&]() {
        --ist.res_status;
        uint32_t p = ist.hp->exec_get(ist.para_typ[1], ist.para_val[1]);
        uint32_t q = ist.hp->exec_get(ist.para_typ[2], ist.para_val[2]);
        ist.hp->exec_set(ist.id(), ist.fake_dst_typ, ist.fake_dst_val, p-q);
    };

    if (!s)
        return sprintf(ist.ori_str, "SUBU R%d, R%d, R%d", ist.rd(), \
                ist.rs(), ist.rt());
    return sprintf(s, "SUBU R%d, R%d, R%d", ist.rd(), \
            ist.rs(), ist.rt());
}

// 000000 - last6: 100000
int Disassembler::dis_add(Inst& ist, char* s) {
    ist.para_typ[0] = ist.NONE;
    ist.para_typ[1] = ist.para_typ[2] \
                      = ist.REGISTER;
    ist.para_val[0] = ist.rd();
    ist.para_val[1] = ist.rs();
    ist.para_val[2] = ist.rt();
    ist.dst_typ = ist.REGISTER;
    ist.dst_val = ist.rd();
    ist.res_status = 1;
    ist.exec = [&]() {
        --ist.res_status;
        int p = ist.hp->exec_get(ist.para_typ[1], ist.para_val[1]);
        int q = ist.hp->exec_get(ist.para_typ[2], ist.para_val[2]);
        ist.hp->exec_set(ist.id(), ist.fake_dst_typ, ist.fake_dst_val, p+q);
    };

    if (!s)
        return sprintf(ist.ori_str, "ADD R%d, R%d, R%d", ist.rd(), \
                ist.rs(), ist.rt());
    return sprintf(s, "ADD R%d, R%d, R%d", ist.rd(), \
            ist.rs(), ist.rt());
}

// 000000 - last6: 100001
int Disassembler::dis_addu(Inst& ist, char* s) {
    ist.para_typ[0] = ist.NONE;
    ist.para_typ[1] = ist.para_typ[2] \
                      = ist.REGISTER;
    ist.para_val[0] = ist.rd();
    ist.para_val[1] = ist.rs();
    ist.para_val[2] = ist.rt();
    ist.dst_typ = ist.REGISTER;
    ist.dst_val = ist.rd();
    ist.res_status = 1;
    ist.exec = [&]() {
        --ist.res_status;
        uint32_t p = ist.hp->exec_get(ist.para_typ[1], ist.para_val[1]);
        uint32_t q = ist.hp->exec_get(ist.para_typ[2], ist.para_val[2]);
        ist.hp->exec_set(ist.id(), ist.fake_dst_typ, ist.fake_dst_val, p+q);
    };
    if (!s)
        return sprintf(ist.ori_str, "ADDU R%d, R%d, R%d", ist.rd(), \
                ist.rs(), ist.rt());
    return sprintf(s, "ADDU R%d, R%d, R%d", ist.rd(), \
            ist.rs(), ist.rt());
}

// 000000 - last6: 100100
int Disassembler::dis_and(Inst& ist, char* s) {
    ist.para_typ[0] = ist.NONE;
    ist.para_typ[1] = ist.para_typ[2] \
                      = ist.REGISTER;
    ist.para_val[0] = ist.rd();
    ist.para_val[1] = ist.rs();
    ist.para_val[2] = ist.rt();
    ist.dst_typ = ist.REGISTER;
    ist.dst_val = ist.rd();
    ist.res_status = 1;
    ist.exec = [&]() {
        --ist.res_status;
        int p = ist.hp->exec_get(ist.para_typ[1], ist.para_val[1]);
        int q = ist.hp->exec_get(ist.para_typ[2], ist.para_val[2]);
        ist.hp->exec_set(ist.id(), ist.fake_dst_typ, ist.fake_dst_val, p&q);
    };
    if (!s)
        return sprintf(ist.ori_str, "AND R%d, R%d, R%d", ist.rd(), \
                ist.rs(), ist.rt());
    return sprintf(s, "AND R%d, R%d, R%d", ist.rd(), \
            ist.rs(), ist.rt());
}

// 000000 - last6: 100101
int Disassembler::dis_or(Inst& ist, char* s) {
    ist.para_typ[0] = ist.NONE;
    ist.para_typ[1] = ist.para_typ[2] \
                      = ist.REGISTER;
    ist.para_val[0] = ist.rd();
    ist.para_val[1] = ist.rs();
    ist.para_val[2] = ist.rt();
    ist.dst_typ = ist.REGISTER;
    ist.dst_val = ist.rd();
    ist.res_status = 1;
    ist.exec = [&]() {
        --ist.res_status;
        int p = ist.hp->exec_get(ist.para_typ[1], ist.para_val[1]);
        int q = ist.hp->exec_get(ist.para_typ[2], ist.para_val[2]);
        ist.hp->exec_set(ist.id(), ist.fake_dst_typ, ist.fake_dst_val, p|q);
    };

    if (!s)
        return sprintf(ist.ori_str, "OR R%d, R%d, R%d", ist.rd(), \
                ist.rs(), ist.rt());
    return sprintf(s, "OR R%d, R%d, R%d", ist.rd(), \
            ist.rs(), ist.rt());
}

// 000000 - last6: 100110
int Disassembler::dis_xor(Inst& ist, char* s) {
    ist.para_typ[0] = ist.NONE;
    ist.para_typ[1] = ist.para_typ[2] \
                      = ist.REGISTER;
    ist.para_val[0] = ist.rd();
    ist.para_val[1] = ist.rs();
    ist.para_val[2] = ist.rt();
    ist.res_status = 1;
    ist.dst_typ = ist.REGISTER;
    ist.dst_val = ist.rd();
    ist.exec = [&]() {
        --ist.res_status;
        int p = ist.hp->exec_get(ist.para_typ[1], ist.para_val[1]);
        int q = ist.hp->exec_get(ist.para_typ[2], ist.para_val[2]);
        ist.hp->exec_set(ist.id(), ist.fake_dst_typ, ist.fake_dst_val, p^q);
    };
    if (!s)
        return sprintf(ist.ori_str, "XOR R%d, R%d, R%d", ist.rd(), \
                ist.rs(), ist.rt());
    return sprintf(s, "XOR R%d, R%d, R%d", ist.rd(), \
            ist.rs(), ist.rt());
}

// 000000 - last6: 100111 
int Disassembler::dis_nor(Inst& ist, char* s) {
    ist.para_typ[0] = ist.NONE;
    ist.para_typ[1] = ist.para_typ[2] \
                      = ist.REGISTER;
    ist.para_val[0] = ist.rd();
    ist.para_val[1] = ist.rs();
    ist.para_val[2] = ist.rt();
    ist.dst_typ = ist.REGISTER;
    ist.dst_val = ist.rd();
    ist.res_status = 1;
    ist.exec = [&]() {
        --ist.res_status;
        int p = ist.hp->exec_get(ist.para_typ[1], ist.para_val[1]);
        int q = ist.hp->exec_get(ist.para_typ[2], ist.para_val[2]);
        int ans = (!p && !q) ? 1 : 0;
        ist.hp->exec_set(ist.id(), ist.fake_dst_typ, ist.fake_dst_val, ans);
    };

    if (!s)
        return sprintf(ist.ori_str, "NOR R%d, R%d, R%d", ist.rd(), \
                ist.rs(), ist.rt());
    return sprintf(s, "NOR R%d, R%d, R%d", ist.rd(), \
            ist.rs(), ist.rt());
}

// 000000 - last6: 000000
/*diprecated*/
int Disassembler::dis_nop(Inst& ist, char* s) {
    return sprintf(s, "NOP");
}
