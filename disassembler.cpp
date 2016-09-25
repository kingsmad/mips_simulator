#include <iostream>
#include "bin2buf.h"
#include <string.h>
#include "disassembler.h"
using namespace std;

typedef int (Disassembler::*DisF)(Inst&, char*);
bool Disassembler::_map_inited = false;
DisF Disassembler::_dis_map[128];
DisF Disassembler::_dis_submap_zero[128];
DisF Disassembler::_dis_submap_one[128];

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
    int p = 0;
    p += trans2z(ist, s);
    p += sprintf(s+p, " %d ", pc);
    p += trans2t(ist, s+p); 

    s[p++] = '\n';
    pc += 4;
    return p;
}

int Disassembler::proc_data(Inst& ist, char* s, int& pc) {
    for(int i=0; i<32; ++i) {
        if ((ist.d() & (1<<i))) 
            s[31-i] = '1';
        else 
            s[31-i] = '0';
    }

    int t = sprintf(s+32, " %d 0\n", pc);
    pc += 4;
    return t+32; 
}

// init op map
void Disassembler::__init_dis_map() {
    DisF* m = &_dis_map[0];
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
    return sprintf(s, "SW R%d, %d(R%d)", ist.rt(), \
            ist.getiv(), ist.rs());
}

// 100011
int Disassembler::dis_lw(Inst& ist, char* s) {
    return sprintf(s, "LW R%d, %d(R%d)", ist.rt(), \
            ist.getiv(), ist.rs());\
}

// 000010 bug here
int Disassembler::dis_j(Inst& ist, char* s) {
    uint32_t d = ist.getv(0, 25);
    d = d << 2;
    return sprintf(s, "J #%d", d);
}

//000100 the offset requires 2 bits shift.
int Disassembler::dis_beq(Inst& ist, char* s) {
    return sprintf(s, "BEQ R%d, R%d, #%d", ist.rs(), \
            ist.rt(), ist.getiv()<<2);
}

// 000101
int Disassembler::dis_bne(Inst& ist, char* s) {
    return sprintf(s, "BNE R%d, R%d, %d", ist.rs(), \
        ist.rt(), ist.getiv());
}


// 000001
int Disassembler::dis_bgez(Inst& ist, char* s) {
    return sprintf(s, "BGEZ R%d, %d", ist.rs(), \
            ist.getv(0, 15));
}

// 000111
int Disassembler::dis_bgtz(Inst& ist, char* s) {
    return sprintf(s, "BGTZ R%d, %d", ist.rs(), \
            ist.getiv());
}

// 000110
int Disassembler::dis_blez(Inst& ist, char* s) {
    return sprintf(s, "BLEZ R%d, %d", ist.rs(), \
            ist.getiv());
}

// 000001
int Disassembler::dis_bltz(Inst& ist, char* s) {
    return sprintf(s, "BLTZ R%d, %d", ist.rs(), \
            ist.getiv());
}

// 001000
int Disassembler::dis_addi(Inst& ist, char* s) {
   return sprintf(s, "ADDI R%d, R%d, #%d", ist.rt(), \
           ist.rs(), ist.getiv());
}

// 001001
int Disassembler::dis_addiu(Inst& ist, char* s) {
    return sprintf(s, "ADDIU R%d, R%d, #%d", ist.rt(), \
            ist.rs(), ist.getiv());
}

// 000000 - last6: 001101
int Disassembler::dis_break(Inst& ist, char* s) {
    this->_on_data_seg = true;
    return sprintf(s, "BREAK");
}

// 000000 - last6: 101010
int Disassembler::dis_slt(Inst& ist, char* s) {
    return sprintf(s, "SLT R%d, R%d, R%d", ist.rd(), \
            ist.rs(), ist.rt());
}

// 001010
int Disassembler::dis_slti(Inst& ist, char* s) {
    return sprintf(s, "SLTI R%d, R%d, #%d", ist.rt(), \
            ist.rs(), ist.getiv());
}

// 000000 - last6: 101011
int Disassembler::dis_sltu(Inst& ist, char* s) {
    return sprintf(s, "SLTU R%d, R%d, R%d", ist.rd(), \
            ist.rs(), ist.rt());
}

// 000000 - last6: 000000
// overlap with nop
int Disassembler::dis_sll_nop(Inst& ist, char* s) {
    if (ist.d() == 0) 
        return sprintf(s, "NOP");
    else 
        return sprintf(s, "SLL R%d, R%d, #%d", ist.rd(), \
            ist.rt(), ist.sa());
}

// 000000 - last6: 000010
int Disassembler::dis_srl(Inst& ist, char* s) {
    return sprintf(s, "SRL R%d, R%d, #%d", ist.rd(), \
            ist.rt(), ist.sa());
}

// 000000 - last6: 000011
int Disassembler::dis_sra(Inst& ist, char* s) {
    return sprintf(s, "SRA R%d, R%d, #%d", ist.rd(), \
            ist.rt(), ist.sa());
}

// 000000 - last6: 100010
int Disassembler::dis_sub(Inst& ist, char* s) {
    return sprintf(s, "SUB R%d, R%d, R%d", ist.rd(), \
            ist.rs(), ist.rt());
}

// 000000 - last6: 100011
int Disassembler::dis_subu(Inst& ist, char* s) {
    return sprintf(s, "SUBU, R%d, R%d, R%d", ist.rd(), \
            ist.rs(), ist.rt());
}

// 000000 - last6: 100000
int Disassembler::dis_add(Inst& ist, char* s) {
    return sprintf(s, "ADD R%d, R%d, R%d", ist.rd(), \
            ist.rs(), ist.rt());
}

// 000000 - last6: 100001
int Disassembler::dis_addu(Inst& ist, char* s) {
    return sprintf(s, "ADDU R%d, R%d, R%d", ist.rd(), \
            ist.rs(), ist.rt());
}

// 000000 - last6: 100100
int Disassembler::dis_and(Inst& ist, char* s) {
    return sprintf(s, "AND R%d, R%d, R%d", ist.rd(), \
            ist.rs(), ist.rt());
}

// 000000 - last6: 100101
int Disassembler::dis_or(Inst& ist, char* s) {
    return sprintf(s, "OR R%d, R%d, R%d", ist.rd(), \
            ist.rs(), ist.rt());
}

// 000000 - last6: 100110
int Disassembler::dis_xor(Inst& ist, char* s) {
    return sprintf(s, "XOR R%d, R%d, R%d", ist.rd(), \
            ist.rs(), ist.rt());
}

// 000000 - last6: 100111 
int Disassembler::dis_nor(Inst& ist, char* s) {
    return sprintf(s, "NOR R%d, R%d, R%d", ist.rd(), \
            ist.rs(), ist.rt());
}

// 000000 - last6: 000000
int Disassembler::dis_nop(Inst& ist, char* s) {
    return sprintf(s, "NOP");
}
