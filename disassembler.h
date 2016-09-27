/*************************************************************************
    > File Name: disassembler.h
************************************************************************/
using namespace std;
class Inst;
const int _ref_map_sz = 128;

class Disassembler {
    typedef int (Disassembler::*DisF)(Inst&,char*);
    static bool _map_inited;
    static DisF _dis_map[_ref_map_sz];
    static DisF _dis_submap_zero[_ref_map_sz];
    static DisF _dis_submap_one[_ref_map_sz];
    bool _on_data_seg; // indicate if it is on data segments.
private:
    int __str2six(const char*); // six bits-string to int
    void __init_dis_map();
    void __init_dis_submaps();
    int __dis_sub1_zero(Inst&, char*);
    int __dis_sub1_one(Inst&, char*);
public:
    Disassembler();
    int trans2z(Inst&, char*); // int to string as '655556' form.
    int trans2t(Inst&, char*); // int to text as 'ADDI R1, R8, #12'
    int proc(Inst&, char*, int&); // process Inst, save string
    int proc_data(Inst&, char*, int&); // when it is data
    int proc_inst(Inst&, char*, int&); // when it is instruction
    int dis_unknown(Inst&, char*);
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
