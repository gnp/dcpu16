//
//  main.cpp
//  dcpu16
//
//  Created by Gregor N. Purdy, Sr. on 2012-06-06.
//  Copyright (c) 2012 Gregor N. Purdy, Sr. All rights reserved.
//

#include <iostream>
#include "dcpu16.h"

int main(int argc, const char * argv[])
{
    dcpu16 cpu;
    
    dcpu16::word_t words[33] = {
        /* 0x0000: */ 0x7c01, /* [011111 000000 0001] (b:0x1f,a:0x00,o:0x1) ; SET REG:A NEXT_WORD */
        /* 0x0001: */ 0xbeef,
        
        /* 0x0002: */ 0x01e1, /* [000000 011110 0001] (b:0x00,a:0x1e,o:0x1) ; SET [NEXT_WORD] REG:A */
        /* 0x0003: */ 0x1000,
        
        /* 0x0004: */ 0x780d, /* [011110 000000 1101] (b:0x1e,a:0x00,o:0xd) ; IFN REG:A [NEXT_WORD] */
        /* 0x0005: */ 0x1000,
        
        /* 0x0006: */ 0x7dc1, /* [011111 011100 0001] (b:0x1f,a:0x1c,o:0x1) ; SET PC NEXT_WORD */
        /* 0x0007: */ 0x0020, /* ; :end */
        
        /* 0x0008: */ 0x8061, /* [100000 000110 0001] (b:0x20,a:0x06,o:0x1) ; SET REG:I LIT:0 */
        
        /* :nextchar */
        /* 0x0009: */ 0x816c, /* [100000 010110 1100] (b:0x20,a:0x16,o:0xc) ; IFE [NEXT_WORD + REG:I] LIT:0 */
        /* 0x000a: */ 0x0013, /* :data */
        
        /* 0x000b: */ 0x7dc1, /* [011111 011100 0001] (b:0x1f,a:0x1c,o:0x1) ; SET PC NEXT_WORD */
        /* 0x000c: */ 0x0020, /* ; :end */
        
        /* 0x000d: */ 0x5961, /* [010110 010110 0001] (b:0x16,a:0x16,o:0x1) ; SET [NEXT_WORD + REG:I] [NEXT_WORD + REG:I] */
        /* 0x000e: */ 0x8000,
        /* 0x000f: */ 0x0013, /* ; :data */
        
        /* 0x0010: */ 0x8462, /* [100001 000110 0010] (b:0x21,a:0x06,o:0x2) ; ADD REG:I LIT:1 */
        
        /* 0x0011: */ 0x7dc1, /* [011111 011100 0001] (b:0x1f,a:0x1c,o:0x1) ; SET PC NEXT_WORD */
        /* 0x0012: */ 0x0009, /* ; :nextchar */
        
        /* :data */
        /* 0x0013: */ 0x0048, /* ; 'H' */
        /* 0x0014: */ 0x0065, /* ; 'e' */
        /* 0x0015: */ 0x006c, /* ; 'l' */
        /* 0x0016: */ 0x006c, /* ; 'l' */
        /* 0x0017: */ 0x006f, /* ; 'o' */
        /* 0x0018: */ 0x0020, /* ; ' ' */
        /* 0x0019: */ 0x0077, /* ; 'w' */
        /* 0x001a: */ 0x0064, /* ; 'o' */
        /* 0x001b: */ 0x0072, /* ; 'r' */
        /* 0x001c: */ 0x006c, /* ; 'l' */
        /* 0x001d: */ 0x0064, /* ; 'd' */
        /* 0x001e: */ 0x0021, /* ; '!' */
        /* 0x001f: */ 0x0000, /* ; 0 */
        
        /* :end */
        /* 0x0020: */ 0x85c3 /* [100001 011100 0011] (b:0x21,a:0x1c,0x3) ; SUB PC LIT:1 */
    };
    
    cpu.load(0, words, 33);
    
    for (int i = 0; i < 256; ++i) {
        cpu.tick();
    }

    return 0;
}

