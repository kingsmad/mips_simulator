/*************************************************************************
    > File Name: disassembler.h
************************************************************************/
using namespace std;

class Inst;
class Disassembler {
    typedef int (Disassembler::*DisF)(Inst&,char*);
    static bool _map_inited;
    static DisF _dis_map[128];
    static DisF _dis_submap_zero[128];
    static DisF _dis_submap_one[128];
    bool _on_data_seg;
private:
    int __str2six(const char*);
    void __init_dis_map();
    void __init_dis_submaps();
    int __dis_sub1_zero(Inst&, char*);
    int __dis_sub1_one(Inst&, char*);
public:
    Disassembler();
    int trans2z(Inst&, char*);
    int trans2t(Inst&, char*);
    int proc(Inst& ist, char* s, int&);
    int proc_data(Inst&, char*, int&);
    int proc_inst(Inst&, char*, int&);
    int dis_special(Inst&, char*);
    int dis_sw(Inst&, char*);
    int dis_lw(Inst&, char*);
    int dis_j(Inst&, char*);
    int dis_beq(Inst&, char*);
    int dis_bne(Inst&, char*);
    int dis_bgez(Inst&, char*);
    int dis_bgtz(Inst&, char*);
    int dis_blez(Inst&, char*);
    int dis_bltz(Inst&, char*);
    int dis_addi(Inst&, char*);
    int dis_addiu(Inst&, char*);
    int dis_break(Inst&, char*);
    int dis_slt(Inst&, char*);
    int dis_slti(Inst&, char*);
    int dis_sltu(Inst&, char*);
    int dis_sll_nop(Inst&, char*);
    int dis_srl(Inst&, char*);
    int dis_sra(Inst&, char*);
    int dis_sub(Inst&, char*);
    int dis_subu(Inst&, char*);
    int dis_add(Inst&, char*);
    int dis_addu(Inst&, char*);
    int dis_and(Inst&, char*);
    int dis_or(Inst&, char*);
    int dis_xor(Inst&, char*);
    int dis_nor(Inst&, char*);
    int dis_nop(Inst&, char*);
};
