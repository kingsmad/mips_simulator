#include "scheduler.h"

BaseTimer::BaseTimer():
    stime(0),
    rtime(0),
    state_(TIMER_IDLE)
{

}

double BaseTimer::remainTime() {
    switch (state_){
        case TIMER_IDLE:
            return 0;

        case TIMER_PAUSED:
            return rtime;

        case TIMER_BUSY:
            return (stime + rtime - Scheduler::instance().clock());
        default:
            return 0;
    }
}


double BaseTimer::passedTime() {
    switch (state_) {
        case TIMER_IDLE:
            return 0;
            break;
        case TIMER_PAUSED:
            return 0;
            break;
        case TIMER_BUSY:
            return (Scheduler::instance().clock() - stime);
            break;
        default:
            return 0;
    }


}

void BaseTimer::start(double d){//timeridlerestart
    Scheduler& s = Scheduler::instance();
    switch(state_){
        case TIMER_BUSY:
        case TIMER_PAUSED:
            break;
        case TIMER_IDLE:
            stime = s.clock();
            rtime = d;
            s.schedule(this,&intr,rtime);
            state_ = TIMER_BUSY;
            break;
        default:
            break;
    }
}

void BaseTimer::restart(double d){//
    Scheduler& s = Scheduler::instance();
    switch(state_){
        case TIMER_BUSY:
            s.cancel(&intr);
        case TIMER_PAUSED:
        case TIMER_IDLE:
            stime = s.clock();
            rtime = d;
            s.schedule(this,&intr,rtime);
            state_ = TIMER_BUSY;
            break;
        default:
            break;
    }
}

void BaseTimer::stop() {
    Scheduler& s = Scheduler::instance();
    switch(state_){
        case TIMER_BUSY:
            s.cancel(&intr);
        case TIMER_PAUSED:
            stime = rtime = 0;
            state_ = TIMER_IDLE;
            break;
        case TIMER_IDLE:
        default:
            break;
    }
}

void BaseTimer::pause() {
    Scheduler &s = Scheduler::instance();
    switch(state_){
        case TIMER_BUSY:
            rtime = remainTime();
            stime = 0;
            state_ = TIMER_PAUSED;

            s.cancel(&intr);
            break;

        case TIMER_PAUSED:
        case TIMER_IDLE:
        default:
            break;
    }
}

void BaseTimer::resume() {
    assert( state_ == TIMER_PAUSED);
    start(rtime);
}

void BaseTimer::handle(Event*) {
    state_ = TIMER_IDLE;
    stime = 0;
    rtime = 0;
    expire();
}

int BaseTimer::state() {
    return state_;
}
