#include <iostream>
#include <memory>

template <class ClassType, typename ParamType1, typename ParamType2>
class AsyncEventAgent {
public:
    typedef void (ClassType::*event_method_t)(ParamType1, ParamType2);
    typedef ParamType1 &NoReferenceType1;
    typedef ParamType2 &NoReferenceType2;

    AsyncEventAgent(ClassType *object,
                     event_method_t method,
                     ParamType1 param1,
                     ParamType2 param2)
        : mObject(object), mMethod(method), mParam1(param1), mParam2(param2)
    {
    }

    void deliver()
    {
        (mObject->*mMethod)(mParam1, mParam2);
    }

    ClassType *mObject;
    event_method_t mMethod;
    ParamType1 mParam1;
    ParamType2 mParam2;

};


class god {
public:
        int demoValue = 0xDEAD;
    virtual ~god() {};

public:
            void foo(int, int) { std::cout << "god::foo\n"; };
    virtual void bar(int, int) { std::cout << "god::bar\n"; };
};

class dog : public god {

public:
    int demoValue = 0xBEEF;

            void foo(int, int) { std::cout << "dog::foo\n"; };
    virtual void bar(int, int) { std::cout << "dog::bar\n"; };
};

int main() {

    std::unique_ptr<god> objGod(new god());
    std::unique_ptr<dog> objDog(new dog());

    AsyncEventAgent<god, int, int> asyncFoo(objDog.get(), &god::foo, 222,333);
    asyncFoo.deliver();

    AsyncEventAgent<god, int, int> asyncBar(objDog.get(), &god::bar, 222,333);
    asyncBar.deliver();

    return 0;
}
