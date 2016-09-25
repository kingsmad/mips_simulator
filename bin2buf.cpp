/*************************************************************************
    > File Name: bin2buf.cpp
************************************************************************/
#include "bin2buf.h"
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;
const int buf_sz = 1e5 + 10;

int Inst::getv(int st, int ed) {
    uint32_t msk = 0;
    for(int i=st; i<=ed; ++i) {
        msk = msk | (1<<i);
    }
    return (_d & msk) >> st;
}

int Inst::getiv() {
    int q = getv(0, 15);
    if ((1<<15) & q) {
        q |= 0xffff0000;
    }
    return q;
}

void Inst::setop(uint32_t v) {
    v = (127 & v) << 26;
    _d = _d << 6 >> 6; 
    _d = _d | v;
}

void Inst::setrs(uint32_t v) {
    uint32_t msk = ~(63 << 21);
    v = (63 & v) << 21;  
    _d = (_d & msk) | v;
}

void Inst::setrt(uint32_t v) {
    uint32_t msk = ~(63 << 16);
    v = (63 & v) << 16;  
    _d = (_d & msk) | v;
}

void Inst::setrd(uint32_t v) {
    uint32_t msk = ~(63 << 11);
    v = (63 & v) << 11;  
    _d = (_d & msk) | v;
}

void Inst::setsa(uint32_t v) {
    uint32_t msk = ~(63 << 6);
    v = (63 & v) << 6;  
    _d = (_d & msk) | v;
}

void Inst::setfc(uint32_t v) {
    uint32_t msk = ~(uint32_t)127;
    v = (127 & v) << 6;  
    _d = (_d & msk) | v;
}

void Inst::setd(int v) {
    _d = v;
}

// translate inst to binary-chars, format->6 5 5 5 5 6;
int Inst::trans2z(char* s) {
    char b[32];
    for(int i=0; i<32; ++i) {
        b[32-i] = ((s[i] & ((uint32_t)1<<i)) != 0) ? '1' : '0';
    }
    char* p = &b[0], *q = s; 
    int cnt = 0;
    while(cnt < 6) {
        int sz = (cnt==1 || cnt==5) ? 6:5;
        memcpy(q, p, sz);
        q += sz, p += sz;
        *q++ = ' ';
        ++cnt;
    }

    return 0;
}

Bin2buf::Bin2buf() {
    fd = fsize = cpos = 0;
    ofd = ocpos = 0;
    fbuf = ofbuf = 0;
}

int Bin2buf::__file_size(int fd) {
    struct stat s;
    if (fstat(fd, &s) == -1) {
        printf("\nfstat returned erro;");
        return -1;
    }
    return s.st_size;
}

int Bin2buf::__zflush() {
    if (ocpos == 0) return 0; 
    ofbuf[ocpos] = 0; 
    int r = write(ofd, ofbuf, ocpos);
    if (r == -1) {
        printf("\nWrite erro occured when zflush.");
        return -1;
    }
    ocpos = 0;
    return 0;
}

int Bin2buf::open_in_file (const char* sf) {
    if (fd) close(fd);
    fd = open(sf, O_RDONLY, 0644);
    if (fd == -1) {
        printf("\nIncorrect file name or non-exist file.");
        return -1;
    }
    if ((fsize = __file_size(fd)) == -1) 
        return -1;
    if (fbuf) 
        free(fbuf);
    // read all data
    fbuf = (char*)malloc(fsize + 1000);
    memset(fbuf, 0, sizeof(fsize+1000));
    int tsz = 0;
    while(tsz < fsize) 
        tsz += read(fd, fbuf+tsz, fsize);
    cpos = 0;

    printf("%s\n", fbuf);
    return 0;
}

int Bin2buf::open_out_file(const char* sf) {
    if (ofd) close(ofd);
    ofd = open(sf, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (ofd == -1) {
        printf("\nIncorrect output filename.");
        return -1;
    }
    if (ofbuf) free(ofbuf);
    ofbuf = (char*) malloc(buf_sz); 
    ocpos = 0;
    return 0;
}

int Bin2buf::get_inst(Inst& dst) {
    if (cpos == fsize) 
        return -1;

    uint32_t t; 
    memcpy(&t, fbuf+cpos, 4);
    dst.setd(htonl(t));
    cpos += 4;

    return 0;
}

int Bin2buf::write_buf(const char* s, int sz) {
    if (ocpos + sz >= buf_sz) {
        if (__zflush() == -1) 
            return -1;
    }
    
    memcpy(ofbuf+ocpos, s, sz);
    ocpos += sz;
    return 0;
}

Bin2buf::~Bin2buf() {
    __zflush();
    if (fd) close(fd);
    if (ofd) close(ofd);
    if (fbuf) free(fbuf);
    if (ofbuf) free(ofbuf);
}
