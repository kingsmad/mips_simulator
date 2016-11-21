#ifndef SIM_H
#define SIM_H
#include "sim.h"

class Scheduler {
private:
    Simulator __sim;
    int __clock;
public:
    Scheduler();
    inline Simulator& sim() { return __sim;}
    void run(); 
};


























#endif
