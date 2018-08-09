#include <iostream>
#include <memory>

// Is delegated to perfrom work on our behalf
template <class T, typename P1, typename P2>
class OurSlaveType {
public:
    typedef void (T::*hard_work_t)(P1, P2);

    OurSlaveType(T *obj, hard_work_t work, P1 p1, P2 p2)
        : mObj(obj), mMethod(work), mP1(p1), mP2(p2)
    {}

    void doForUs() { (mObj->*mMethod)(mP1, mP2); }

    T  *mObj;
    P1  mP1;
    P2  mP2;
    hard_work_t mMethod;
};

class Work {
public:
    int demoValue = 0xDEAD; // for visibility in gdb
    
    virtual ~Work() {}

            void clean(int, int) { std::cout << "Work::clean\n"; }
    virtual void cook(int, int) { std::cout << "Work::cook\n"; }
};

class PainfulWork : public Work {
public:
    int demoValue = 0xBEEF; // for visibility in gdb

    virtual ~PainfulWork() {}

            void clean(int, int) { std::cout << "PainfulWork::clean\n"; }
    virtual void cook(int, int) { std::cout << "PainfulWork::cook\n"; }
};

int main() {

    std::unique_ptr<Work>        work(new Work());
    std::unique_ptr<PainfulWork> painfulWork(new PainfulWork());

    OurSlaveType<Work, int, int> slave1(painfulWork.get(), &Work::clean, 111, 222);
    slave1.doForUs();

    OurSlaveType<Work, int, int> slave2(painfulWork.get(), &Work::cook, 333, 444);
    slave2.doForUs();

    return 0;
}
