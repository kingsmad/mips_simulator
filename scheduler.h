#ifndef SCHEDULER_H
#define SCHEDULER_H
#include <queue>

using namespace std;

class Event {
public:
    double exp_time;
    void expire();
    void* arg;
};

class BaseTimer {
public:
    BaseTimer();
    virtual void handle(Event*);//expire
    virtual void start(double);
    virtual void pause();
    virtual void resume();
    virtual void restart(double);
    virtual void stop();

    enum TIMERSTATE {
        TIMER_IDEL,
        TIMER_BUSY,
        TIMER_PAUSED
    };

    int state();
    double remain_time();
    double passed_time();
protected:
    double stime; // timer start time
    double rtime; // timer remaining time for expiering
    Event intr;
    virtual void expire() = 0;
    int _state;
};

class __void_class_blackOps{};

template <class T, typename Arg = __void_class_blackOps>
class Timer : public BaseTimer {
    typedef void (T::*memberFuncA)(Arg arg);
    T* _t;
    memberFuncA _f;
    Arg _arg;
    public:
    Timer(T* t, memberFuncA f);
    virtual void start(double, Arg);
    virtual void restart(double, Arg);
    using BaseTimer::restart;
    virtual void expire();
    void setFunc(memberFuncA);
};

template<class T, typename Arg>
Timer<T, Arg>::Timer(T* t, memberFuncA f) :
    _t(t),
    _f(f),
    _arg(0)
{

}

template<class T, typename Arg>
void Timer<T, Arg>::start(double d, Arg arg){
    _arg = arg;
    BaseTimer::start(d);
}

template<class T, typename Arg>
void Timer<T, Arg>::restart(double d, Arg arg){
    _arg = arg;
    BaseTimer::restart(d);
}

template<class T, typename Arg>
void Timer<T, Arg>::setFunc(memberFuncA f){
    _f = f;
}

template<class T, typename Arg>
void Timer<T, Arg>::expire(){
    (_t->*_f)(_arg);
}


/*Single para version */
template<class T>
class Timer<T, __void_class_blackOps> : public BaseTimer{
    typedef void (T::*memberFunc)();//T
    T* _t;
    memberFunc _f;
    public:
    Timer(T* t, memberFunc f);//
    virtual void expire();//BaseTimer
    void setFunc(memberFunc);//timertimeoutfunc
};

template<class T>
    Timer<T,__void_class_blackOps>::Timer(T* t, memberFunc f) :
        _t(t),
        _f(f)
{
}

template<class T>
void Timer<T,__void_class_blackOps>::expire(){
    (_t->*_f)();
}

template <class T>
void Timer<T,__void_class_blackOps>::setFunc(memberFunc nf) {
    _f = nf;
}

class Pred {
public:
    bool operator() (const Event& e1, const Event& e2) {
        return e1.exp_time < e2.exp_time;
    }
};

class Scheduler {
private:
    priority_queue<Event, vector<Event>, Pred> Q;
    double __clock;
public:
    double clock();
    void set_event(Event e, double d); 
    static Scheduler& instance();
    void cancel(Event* ev);
    void schedule(BaseTimer* tm, Event* ev, double rt);
};

#endif
