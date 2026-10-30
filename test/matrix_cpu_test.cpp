#define NOMINMAX
#include <windows.h>
#include <xmmintrin.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <initializer_list>
#include "../src/stereo_math.h"
#define CHECK(x) do{if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
// Precise scalar arithmetic from MAIN 69b02db1, before the optimization.
__declspec(noinline) static void Baseline(const float* a,const float* b,float* out){
    float result[16]{};
    for(int r=0;r<4;++r)for(int c=0;c<4;++c)for(int k=0;k<4;++k)result[r*4+c]+=a[r*4+k]*b[k*4+c];
    for(int i=0;i<16;++i)out[i]=result[i];
}
__declspec(noinline) static void Candidate(const float* a,const float* b,float* out){d2vr_math::Multiply(a,b,out);}
struct Matrix {float v[16];};
static Matrix input[2048],output[2048];
static unsigned state=123;
static unsigned Next(){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static volatile float sink=0;
int main(){
    for(auto& m:input)for(float& f:m.v)f=(int(Next()%200001)-100000)/4096.f;
    for(unsigned n=0;n<100000;++n){
        float a[16],b[16],expected[16],storage[48];
        for(float& f:a)f=(int(Next()%200001)-100000)/4096.f;
        for(float& f:b)f=(int(Next()%200001)-100000)/4096.f;
        Baseline(a,b,expected);Candidate(a,b,storage);CHECK(!memcmp(storage,expected,64));
        memcpy(storage,a,64);Candidate(storage,b,storage);CHECK(!memcmp(storage,expected,64));
        memcpy(storage,b,64);Candidate(a,storage,storage);CHECK(!memcmp(storage,expected,64));
        memcpy(storage,a,64);memcpy(storage+20,b,64);Candidate(storage,storage+20,storage+3);CHECK(!memcmp(storage+3,expected,64));
    }
    const unsigned originalControl=_mm_getcsr();
    for(unsigned rounding:{unsigned(_MM_ROUND_NEAREST),unsigned(_MM_ROUND_DOWN),unsigned(_MM_ROUND_UP),unsigned(_MM_ROUND_TOWARD_ZERO)}){
        _mm_setcsr((originalControl&~_MM_ROUND_MASK)|rounding);
        for(unsigned n=0;n<20000;++n){
            float a[16],b[16],expected[16],actual[16];
            for(float& f:a){unsigned bits=Next();memcpy(&f,&bits,4);}
            for(float& f:b){unsigned bits=Next();memcpy(&f,&bits,4);}
            Baseline(a,b,expected);Candidate(a,b,actual);
            for(unsigned i=0;i<16;++i)if(memcmp(expected+i,actual+i,4) && !(std::isnan(expected[i]) && std::isnan(actual[i]))){
                unsigned oldBits,newBits;memcpy(&oldBits,expected+i,4);memcpy(&newBits,actual+i,4);
                printf("mismatch round=%x pair=%u element=%u old=%08x new=%08x\n",rounding,n,i,oldBits,newBits);
                for(unsigned k=0;k<4;++k)printf("term%u %.9g %.9g\n",k,a[(i/4)*4+k],b[k*4+i%4]);CHECK(false);
            }
        }
    }
    _mm_setcsr(originalControl);
    puts("PASS 80000 full-range bit-pattern pairs / four MXCSR rounding modes: exact non-NaN bits, same NaN classification");
    puts("PASS 100000 matrix pairs: bit-exact scalar equivalence, alias A/B and partial unaligned overlap");
    for(int mode=0;mode<2;++mode){
        double best=1e9;
        for(unsigned trial=0;trial<7;++trial){
            auto begin=std::chrono::steady_clock::now();
            for(unsigned i=0;i<2000000;++i){unsigned n=i&2047; if(mode)Candidate(input[n].v,input[(n+37)&2047].v,output[n].v);else Baseline(input[n].v,input[(n+37)&2047].v,output[n].v);}
            auto ns=std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-begin).count()/2000000;
            if(ns<best)best=ns;sink=output[trial].v[3];
        }
        printf("BENCH %s %.2f ns/matrix, %.3f ms / 20000 compositions (CPU only)\n",mode?"candidate":"baseline",best,best*.02);
    }
}
