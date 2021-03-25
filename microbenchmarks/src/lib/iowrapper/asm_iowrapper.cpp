#include <cstring>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#ifdef __linux
#include <linux/version.h>
#endif

#include "util.hpp"
#include "iowrapper.hpp"
#define EXEC_2_TIMES(x)   x x
#define EXEC_4_TIMES(x)   EXEC_2_TIMES(EXEC_2_TIMES(x))
#define EXEC_8_TIMES(x)   EXEC_2_TIMES(EXEC_4_TIMES(x))
#define EXEC_16_TIMES(x)  EXEC_2_TIMES(EXEC_8_TIMES(x))
#define EXEC_32_TIMES(x)   EXEC_2_TIMES(EXEC_16_TIMES(x))
#define EXEC_64_TIMES(x)   EXEC_2_TIMES(EXEC_32_TIMES(x))
#define EXEC_128_TIMES(x)   EXEC_2_TIMES(EXEC_64_TIMES(x))
#define EXEC_256_TIMES(x)   EXEC_2_TIMES(EXEC_128_TIMES(x))

//used 
#define UNROLL_CONSTANT 1024
#ifdef UNROLL_JUMPS
	#define UNROLL_OPT(x) EXEC_64_TIMES(x)
	#define ACCESSES_PER_LOOP 64
#endif
#ifndef UNROLL_JUMPS
	#define UNROLL_OPT(x) x
	#define ACCESSES_PER_LOOP 1
#endif



#define READ_ADDR_ADD_8192 \
	"add $8192, %[addr]\n"

#define READ_NEXT_ADDR \
	"movq %%xmm0, %[next_addr]\n"

#define READ_OFFSET

#define READ_NT_64_ASM \
    "vmovntdqa 0(%[addr]), %%zmm0 \n"

#define READ_NT_128_ASM \
    "vmovntdqa 0(%[addr]), %%zmm0 \n"\
    "vmovntdqa 1*64(%[addr]), %%zmm1 \n"

#define READ_NT_256_ASM \
    "vmovntdqa 0(%[addr]),      %%zmm0 \n" \
    "vmovntdqa 1*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 2*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 3*64(%[addr]),   %%zmm1 \n"

#define READ_NT_512_ASM \
    "vmovntdqa 0*64(%[addr]),   %%zmm0 \n" \
    "vmovntdqa 1*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 2*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 3*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 4*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 5*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 6*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 7*64(%[addr]),   %%zmm1 \n" 

#define READ_NT_1024_ASM \
    "vmovntdqa 0*64(%[addr]),   %%zmm0 \n" \
    "vmovntdqa 1*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 2*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 3*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 4*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 5*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 6*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 7*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 8*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 9*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 10*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 11*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 12*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 13*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 14*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 15*64(%[addr]),   %%zmm1 \n" \

#define READ_NT_2048_ASM \
    "vmovntdqa 0*64(%[addr]),   %%zmm0 \n" \
    "vmovntdqa 1*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 2*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 3*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 4*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 5*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 6*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 7*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 8*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 9*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 10*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 11*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 12*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 13*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 14*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 15*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 16*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 17*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 18*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 19*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 20*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 21*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 22*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 23*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 24*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 25*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 26*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 27*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 28*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 29*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 30*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 31*64(%[addr]),   %%zmm1 \n"

#define READ_NT_4096_ASM \
    "vmovntdqa 0*64(%[addr]),   %%zmm0 \n" \
    "vmovntdqa 1*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 2*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 3*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 4*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 5*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 6*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 7*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 8*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 9*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 10*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 11*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 12*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 13*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 14*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 15*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 16*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 17*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 18*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 19*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 20*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 21*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 22*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 23*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 24*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 25*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 26*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 27*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 28*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 29*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 30*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 31*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 32*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 33*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 34*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 35*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 36*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 37*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 38*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 39*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 40*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 41*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 42*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 43*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 44*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 45*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 46*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 47*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 48*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 49*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 50*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 51*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 52*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 53*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 54*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 55*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 56*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 57*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 58*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 59*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 60*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 61*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 62*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 63*64(%[addr]),   %%zmm1 \n"

#define READ_NT_8192_ASM \
    "vmovntdqa 0*64(%[addr]),   %%zmm0 \n" \
    "vmovntdqa 1*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 2*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 3*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 4*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 5*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 6*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 7*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 8*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 9*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 10*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 11*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 12*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 13*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 14*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 15*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 16*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 17*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 18*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 19*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 20*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 21*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 22*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 23*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 24*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 25*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 26*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 27*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 28*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 29*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 30*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 31*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 32*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 33*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 34*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 35*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 36*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 37*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 38*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 39*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 40*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 41*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 42*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 43*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 44*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 45*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 46*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 47*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 48*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 49*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 50*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 51*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 52*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 53*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 54*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 55*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 56*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 57*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 58*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 59*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 60*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 61*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 62*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 63*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 64*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 65*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 66*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 67*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 68*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 69*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 70*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 71*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 72*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 73*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 74*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 75*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 76*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 77*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 78*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 79*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 80*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 81*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 82*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 83*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 84*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 85*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 86*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 87*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 88*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 89*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 90*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 91*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 92*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 93*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 94*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 95*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 96*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 97*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 98*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 99*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 100*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 101*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 102*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 103*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 104*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 105*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 106*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 107*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 108*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 109*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 110*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 111*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 112*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 113*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 114*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 115*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 116*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 117*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 118*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 119*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 120*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 121*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 122*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 123*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 124*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 125*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 126*64(%[addr]),   %%zmm1 \n" \
    "vmovntdqa 127*64(%[addr]),   %%zmm1 \n"

#define WRITE_NT_64_ASM \
    "vmovntdq %%zmm0, 0(%[addr]) \n"\

#define WRITE_NT_128_ASM \
    "vmovntdq %%zmm0, 0(%[addr]) \n"\
    "vmovntdq %%zmm0, 1*64(%[addr]) \n"

#define WRITE_NT_256_ASM \
    "vmovntdq %%zmm0, 0(%[addr])     \n" \
    "vmovntdq %%zmm0, 1*64(%[addr])    \n" \
    "vmovntdq %%zmm0, 2*64(%[addr])    \n" \
    "vmovntdq %%zmm0, 3*64(%[addr])    \n"

#define WRITE_NT_512_ASM \
    "vmovntdq %%zmm0, 0*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 1*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 2*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 3*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 4*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 5*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 6*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 7*64(%[addr]) \n" 

#define WRITE_NT_1024_ASM \
    "vmovntdq   %%zmm0, 0*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 1*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 2*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 3*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 4*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 5*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 6*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 7*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 8*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 9*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 10*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 11*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 12*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 13*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 14*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 15*64(%[addr]) \n" \

#define WRITE_NT_2048_ASM \
    "vmovntdq   %%zmm0, 0*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 1*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 2*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 3*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 4*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 5*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 6*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 7*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 8*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 9*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 10*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 11*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 12*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 13*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 14*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 15*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 16*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 17*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 18*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 19*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 20*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 21*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 22*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 23*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 24*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 25*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 26*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 27*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 28*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 29*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 30*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 31*64(%[addr]) \n"

#define WRITE_NT_4096_ASM \
    "vmovntdq   %%zmm0, 0*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 1*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 2*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 3*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 4*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 5*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 6*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 7*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 8*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 9*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 10*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 11*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 12*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 13*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 14*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 15*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 16*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 17*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 18*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 19*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 20*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 21*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 22*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 23*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 24*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 25*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 26*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 27*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 28*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 29*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 30*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 31*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 32*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 33*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 34*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 35*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 36*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 37*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 38*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 39*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 40*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 41*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 42*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 43*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 44*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 45*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 46*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 47*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 48*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 49*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 50*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 51*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 52*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 53*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 54*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 55*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 56*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 57*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 58*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 59*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 60*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 61*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 62*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 63*64(%[addr]) \n"

#define WRITE_NT_8192_ASM \
    "vmovntdq   %%zmm0, 0*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 1*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 2*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 3*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 4*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 5*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 6*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 7*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 8*64(%[addr]) \n" \
    "vmovntdq   %%zmm0, 9*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 10*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 11*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 12*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 13*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 14*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 15*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 16*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 17*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 18*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 19*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 20*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 21*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 22*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 23*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 24*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 25*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 26*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 27*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 28*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 29*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 30*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 31*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 32*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 33*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 34*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 35*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 36*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 37*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 38*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 39*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 40*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 41*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 42*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 43*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 44*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 45*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 46*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 47*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 48*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 49*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 50*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 51*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 52*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 53*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 54*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 55*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 56*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 57*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 58*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 59*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 60*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 61*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 62*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 63*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 64*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 65*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 66*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 67*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 68*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 69*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 70*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 71*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 72*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 73*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 74*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 75*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 76*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 77*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 78*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 79*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 80*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 81*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 82*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 83*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 84*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 85*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 86*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 87*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 88*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 89*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 90*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 91*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 92*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 93*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 94*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 95*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 96*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 97*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 98*64(%[addr]) \n" \
    "vmovntdq  %%zmm0, 99*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 100*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 101*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 102*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 103*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 104*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 105*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 106*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 107*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 108*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 109*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 110*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 111*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 112*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 113*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 114*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 115*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 116*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 117*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 118*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 119*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 120*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 121*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 122*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 123*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 124*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 125*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 126*64(%[addr]) \n" \
    "vmovntdq %%zmm0, 127*64(%[addr]) \n"

ASMIOWrapper::ASMIOWrapper(struct IOWrapperConfig& config) {
    bufferFilename = std::string(config.directory) + BUFFER_FILE_BASENAME + std::string(config.file_suffix);
    fd = open(bufferFilename.c_str(), open_flags(), 0666);
    mempagesize = getpagesize();
    if (fd == -1) {
        perror("open");       
        crash(std::string("could not open buffer file ") + bufferFilename);
    }
#ifdef __linux
    if (config.use_fadvise) {
        if (posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED) != 0) {
            perror("posix_fadvise");
            bmlog::warning("posix_fadvise did not succeed");
        }
    }
#endif
    map_length = get_filesize();
    
    auto map_flag = MAP_SHARED;
    if (config.use_map_sync)
#ifdef __linux
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0)
#if __GLIBC__ >= 2
#if __GLIBC__ > 2	    
        map_flag = MAP_SHARED_VALIDATE | MAP_SYNC;
#else
#if __GLIBC_MINOR__ == 27
#define MAP_SHARED_VALIDATE 0x3
#define MAP_SYNC 0x80000
#endif
#if __GLIBC_MINOR__ >= 27
        map_flag = MAP_SHARED_VALIDATE | MAP_SYNC;
#else
        crash(std::string("linux kernel version is >= 4.15, but glibc is too old (< 2.27)! map_sync is unsupported."));
#endif	
#endif
#else
        crash(std::string("linux kernel version is >= 4.15, but glibc is too old (< 2)! map_sync is unsupported."));
#endif
#else
        crash(std::string("linux kernel version too old for map_sync flag!"));
#endif
#else
        bmlog::warning("MAP_SYNC not available on this system!");
#endif

    map_addr = (char*)mmap(NULL, map_length, PROT_READ | PROT_WRITE, map_flag, fd, 0);
    if (map_addr == MAP_FAILED) {
        perror("mmap");
        crash("IOWrapper: mmap failed");
    }
}


ASMIOWrapper::~ASMIOWrapper() {
    if (munmap(map_addr, map_length) == -1) {
        perror("munmap");
        crash("IOWrapper: could not munmap");
    }
    if (close(fd) != 0) {
        perror("close");
        crash(std::string("could not close buffer file ") + bufferFilename);
    }
}

int ASMIOWrapper::read(void *dest, std::size_t position, std::size_t len) {
    char *memaddr = map_addr + position;
    if (len == 4096) {
        asm volatile(
        READ_NT_4096_ASM
        : [addr] "+r" (memaddr)
        :
        : "%zmm0"
        );

    } else if (len == 8192) {
        asm volatile(
        READ_NT_8192_ASM
        : [addr] "+r" (memaddr)
        :
        : "%zmm0"
        );
   } else if (len == 16384) {
        asm volatile(
        READ_NT_8192_ASM
        "add $8192, %[addr]\n"
        READ_NT_8192_ASM
		"add $8192, %[addr]\n"
        : [addr] "+r" (memaddr)
        :
        : "%zmm0"
        );
   }  else if (len == 32768) {
               asm volatile(
        READ_NT_8192_ASM
        "add $8192, %[addr]\n"
		READ_NT_8192_ASM
        "add $8192, %[addr]\n"
		READ_NT_8192_ASM
        "add $8192, %[addr]\n"
		READ_NT_8192_ASM
        "add $8192, %[addr]\n"
        : [addr] "+r" (memaddr)
        :
        : "%zmm0"
        );
   }
   else if (len == 65536) {
     asm volatile(
        READ_NT_8192_ASM
        "add $8192, %[addr]\n"
		READ_NT_8192_ASM
        "add $8192, %[addr]\n"
		READ_NT_8192_ASM
        "add $8192, %[addr]\n"
		READ_NT_8192_ASM
        "add $8192, %[addr]\n"
		READ_NT_8192_ASM
        "add $8192, %[addr]\n"
		READ_NT_8192_ASM
        "add $8192, %[addr]\n"
		READ_NT_8192_ASM
        "add $8192, %[addr]\n"
		READ_NT_8192_ASM
        "add $8192, %[addr]\n"
        : [addr] "+r" (memaddr)
        :
        : "%zmm0"
        );
    } else if (len > 65536 && len % 8192 == 0) {
        for (int i = 0; i < len / 8192; i++) {
            asm volatile(
            READ_NT_8192_ASM
            : [addr] "+r" (memaddr)
            :
            : "%zmm0"
            );
            }
            memaddr += 8192;
    } else {
        crash("page size not supported by ASMIOWrapper!");
    }
    return 0;
}

int ASMIOWrapper::write(void *src, std::size_t position, std::size_t len) {
    bmlog::warning("not implemented for ASM Wrapper");
    return 0;
}

const char* ASMIOWrapper::get_filename() const {
    return bufferFilename.c_str();
}

int ASMIOWrapper::open_flags() {
    return O_RDWR | O_CREAT;
}
