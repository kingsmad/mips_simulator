/*************************************************************************
    > File Name: main.cpp
************************************************************************/
const bool debug = true;
#include <cstdio>
#include <iostream>
#include <cmath>
#include <stack>
#include <queue>
#include <climits>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <cstring>
#include <algorithm>
#include <stdarg.h>
using namespace std;
#define per(i,a,n) for(int i=n-1;i>=a;--i)
#define rep(i,a,n) for(int i=a;i<n;++i)
#define erep(i,a,n) for(int i=a;i<=n;++i)
#define fi first
#define se second
#define all(x) (x).begin(), (x).end()
#define pb push_back
#define mp make_pair
typedef long long ll;
typedef vector<int> VI;
typedef pair<int, int> PII;
#define lc(o) (o<<1)
#define rc(o) (o<<1|1)
ll powmod(ll a,ll b, ll MOD) {ll res=1;a%=MOD;for(;b;b>>=1){if(b&1)res=res*a%MOD;a=a*a%MOD;}return res;}
void buginfo(const char* f, ...) {if(!debug)return;va_list al; va_start(al, f);vprintf(f, al);va_end(al);}
/*----------- head-----------*/

#include "bin2buf.h"
#include "disassembler.h"

int main(int argc, char** argv) {
    // Setup input && output files.
    Bin2buf bin2buf;
    Disassembler disa;
    char* inf = (char*) malloc(100);
    char* otf = (char*) malloc(100);
    bool idone = false, odone = false;
    if (argv[1] && argv[2]) {
        int r1 = bin2buf.open_in_file(argv[1]);            
        int r2 = bin2buf.open_out_file(argv[2]);
        if (r1!=-1) idone = true;
        if (r2!=-1) odone = true;
    }
    while(!idone || !odone) {
        if (!idone) {
            printf("\nPlease enter input filename:"); 
            scanf("%s", inf);
            idone = (bin2buf.open_in_file(inf) != -1);
        }
        if (!odone) {
            printf("\nPlease enter output filename:");
            scanf("%s", otf);
            odone = (bin2buf.open_out_file(otf) != -1);
        }
    }
    
    Inst ist;     
    int pc_cnt = 600;
    char* s = (char*)malloc(200);
    while(bin2buf.get_inst(ist) != -1) {
        memset(s, 0, 200);
        disa.proc(ist, s, pc_cnt);
        bin2buf.write_buf(s, strlen(s));
    }
     
    return 0;
}
