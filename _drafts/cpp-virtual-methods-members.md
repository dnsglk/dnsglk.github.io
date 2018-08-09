---
layout:         post
title:          C++: call a pointer to a virtual member method.
date:           2018-08-09 12:00
categories:     cpp
tags:           [C++, c++, cxx, cpp, programming, vptr, virtual method]
---

## Before you read
If you are not sure about the answer - you are welcome to read the whole stuff. Otherwise, it will be probably a waste of time.

What the following code should print?

{% highlight c++ %}
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
{% endhighlight %}

## It's a shame not to know this trivial thing!

Oh no, it's not. It's a joke. When I met the similar code in my work project, the second case wasn't obvious to me. It was a long time since I used C++ and as one knows this language is not an easy. BTW this [Scott Meyer's video](https://www.youtube.com/watch?v=KAWA1DuvCnQ) soothed my worry of being ignorant. 

Lets return to the question. The first one should be something like this:
- asd
- das
- asd

## I don't believe and want to use gdb

Here is notes which I took when run the program via `gdb`. The asm code belongs to `OurSlaveType::doForUs()`.

```
::deliver()

0x000000000040124a <+0>:    push   %rbp            ; save base pointer (x86_64 useless)
0x000000000040124b <+1>:    mov    %rsp,%rbp       ; save stack top to $rbp
0x000000000040124e <+4>:    sub    $0x10,%rsp      ; new stack top at +16 bytes
0x0000000000401252 <+8>:    mov    %rdi,-0x8(%rbp) ; move 8byte arg from $rdi to $rbp
0x0000000000401256 <+12>:   mov    -0x8(%rbp),%rax ; move that stuff to $rax 
0x000000000040125a <+16>:   mov    0x8(%rax),%rax  ; move data located at $rax+8 to $rax
0x000000000040125e <+20>:   and    $0x1,%eax       ; check the last bit is set to 1
0x0000000000401261 <+23>:   test   %rax,%rax       ; - if 1 go to virutal method call 
                                                   ;     preparation +38
                                                   ; - if 0 then the method is not virtual, load                                                    ;   the method address again and jump to +77

0x0000000000401264 <+26>:   jne    0x401270 <AsyncEventAgent2<A, int, int>::deliver()+38>

0x0000000000401266 <+28>:   mov    -0x8(%rbp),%rax ; restore non-virtual method address
0x000000000040126a <+32>:   mov    0x8(%rax),%rax  ; $rax stores A::vFoo address

0x000000000040126e <+36>:   jmp    0x401297 <AsyncEventAgent2<A, int, int>::deliver()+77>

0x0000000000401270 <+38>:   mov    -0x8(%rbp),%rax ; move 'this' to $rdx
0x0000000000401274 <+42>:   mov    (%rax),%rdx

0x0000000000401277 <+45>:   mov    -0x8(%rbp),%rax ; this section loads ptr to vtbl to rdx
0x000000000040127b <+49>:   mov    0x10(%rax),%rax ; 
0x000000000040127f <+53>:   add    %rdx,%rax       ; add 0 to 'this'
0x0000000000401282 <+56>:   mov    (%rax),%rdx     ; rdx now has ptr to vtbl

0x0000000000401285 <+59>:   mov    -0x8(%rbp),%rax ; this section loads addr to correct method
0x0000000000401289 <+63>:   mov    0x8(%rax),%rax  ; load '17' to $rax
0x000000000040128d <+67>:   sub    $0x1,%rax       ; now $rax is '16'
0x0000000000401291 <+71>:   add    %rdx,%rax       ; rax = vtbl + 16
0x0000000000401294 <+74>:   mov    (%rax),%rax     ; vtbl + 16 stores ptr to virtual method

0x0000000000401297 <+77>:   mov    -0x8(%rbp),%rdx ; load last arg ('333') and put it to $edx
0x000000000040129b <+81>:   mov    0x1c(%rdx),%edx ; 

0x000000000040129e <+84>:   mov    -0x8(%rbp),%rcx ; load next arg ('222') to $ecx
0x00000000004012a2 <+88>:   mov    0x18(%rcx),%ecx ; 

0x00000000004012a5 <+91>:   mov    -0x8(%rbp),%rsi ; store pointer to some memory region
0x00000000004012a9 <+95>:   mov    (%rsi),%rdi     ; in $rdi

0x00000000004012ac <+98>:   mov    -0x8(%rbp),%rsi ; load ? ('0') as 1st arg
0x00000000004012b0 <+102>:  mov    0x10(%rsi),%rsi ; to $rsi
0x00000000004012b4 <+106>:  add    %rsi,%rdi       ; 0 + 0x00615c40 -> $rdi
0x00000000004012b7 <+109>:  mov    %ecx,%esi       ; now store 1st arg ('222') in another reg?

0x00000000004012b9 <+111>:  callq  *%rax
0x00000000004012bb <+113>:  nop
0x00000000004012bc <+114>:  leaveq 
0x00000000004012bd <+115>:  retq   

------------------------------------------------
rdi             -> 0x7fffffffdb10
-0x8(%rbp)      -> 0x7fffffffdad8
------------------------------------------------
(gdb) x/2 0x7fffffffdad8
0x7fffffffdad8: 0xffffdb10      0x00007fff
-------------------------------------------------

(gdb) x/4 0x7fffffffdb10
0x7fffffffdb10: 0x00615c40      0x00000000      0x00400c56      0x00000000
                    |                               |
                    |                               |
               what's here ?                        |
                                                    |
  +0x8 = 0x7fffffffdb18    -------------->          |->  <A::vFoo(int, int)>
```
