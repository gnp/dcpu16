//
//  dcpu16.cpp
//  dcpu16
//
//  Created by Gregor N. Purdy, Sr. on 2012-06-06.
//  Copyright (c) 2012 Gregor N. Purdy, Sr. All rights reserved.
//

#include "dcpu16.h"

#include <cstring>
#include <cstdlib>
#include <iostream>
#include <iomanip> // setw(), setprecision()

using std::cout;
using std::endl;
using std::hex;
using std::dec;

using std::setw;
using std::setprecision;
using std::setfill;

const uint16_t HALT = 0;

dcpu16::dcpu16() {
    reset();
}

void dcpu16::reset() {
    bzero(reg_, sizeof(reg_));
    bzero(mem_, sizeof(mem_));
    pc_ = 0;
    sp_ = UINT16_MAX;
    o_ = 0;
    
    cycle_count_ = 0;
    state_ = CPU_STATE_READ_INSTRUCTION;
    
    // Set up instr_ as if we had read a SET 0x0, 0x0 instruction on cycle 0 from memory location 0.
    
    instr_.cycle_count = 0;
    instr_.address = 0;
    instr_.size = 1;
    
    instr_.opcode = 0x01; // SET -- will be a no-op because you cannot set a constant
    
    instr_.operand_a.code = 0x20;
    instr_.operand_a.ptr = &instr_.operand_a.val.word;
    instr_.operand_a.val.word = 0;
    
    instr_.operand_b.code = 0x20;
    instr_.operand_b.ptr = &instr_.operand_b.val.word;
    instr_.operand_b.val.word = 0;
}

/*
 * FIXME: This way, you can only load one less than the total number of words in memory...
 */
void dcpu16::load(addr_t addr, word_t * words, word_t count) {
    uint16_t max = (UINT16_MAX - addr);
    
    if (count > max) {
        count = max;
    }
    
    memcpy(mem_, words, count * sizeof(word_t));
}

/*
 * instr - the instruction from which the operand is being decoded
 * operand - the operand to decode
 */
inline void dcpu16::decode_operand(instr_t & instr, operand_info_t & operand) {
    if (operand.code < 0x08) { // reg
        cout << "  * reg" << endl;
        operand.ptr = &reg_[operand.code];
        operand.val.word = *operand.ptr;
    }
    else if (operand.code < 0x10) { // [reg]
        cout << "  * [reg]" << endl;
        word_t reg = reg_[operand.code - 0x08];
        
        operand.ptr = &(mem_[reg]);
        operand.val.word = *operand.ptr;
    }
    else if (operand.code < 0x18) { // [next + reg]
        cout << "  * [next + reg]" << endl;
        scratch16_t reg = { .word = reg_[operand.code - 0x10] };
        
        operand.next = mem_[pc_++];
        
        scratch16_t next = { .word = operand.next };
        
        scratch32_t addr32 = { .num = (int32_t)next.num + (int32_t)reg.num };
        addr_t addr = addr32.word & 0xffff;
        
        operand.ptr = &(mem_[addr]);
        operand.val.word = *operand.ptr;
    }
    else if (operand.code == 0x18) { // [sp++]: POP
        cout << "  * $POP" << endl;
        // FIXME: Detect wrap-around of SP?
        operand.ptr = &(mem_[sp_++]);
        operand.val.word = *operand.ptr;
    }
    else if (operand.code == 0x19) { // [sp]: PEEK
        cout << "  * $PEEK" << endl;
        operand.ptr = &(mem_[sp_]);
        operand.val.word = *operand.ptr;
    }
    else if (operand.code == 0x1a) { // [--sp]: PUSH
        cout << "  * $PUSH" << endl;
        --sp_;

        operand.ptr = &(mem_[sp_]); // but, cannot really get a pushed value
        operand.val.word = 0;
    }
    else if (operand.code == 0x1b) { // SP
        cout << "  * $SP" << endl;
        operand.ptr = &sp_;
        operand.val.word = *operand.ptr;
    }
    else if (operand.code == 0x1c) { // PC
        cout << "  * $PC" << endl;
        /*
         * FIXME: To *get* the value of the PC, should it be the value of the instruction we are in,
         * or the value of the start of the next instruction? If we don't do something careful, we could
         * be getting the value of the PC part way through a multi-word instruction...
         *
         * We will choose the current PC, so "SUB PC, 1" works, but that only works because that instruction
         * encoded in the most natual way is a single word long. If you do the multi-word one it will go
         * haywire. That is probably evidence it is supposed to be PC after completely decoding the current
         * instruction...
         */
        
        operand.ptr = &pc_;
        operand.val.word = *operand.ptr;
    }
    else if (operand.code == 0x1d) { // O
        cout << "  * $O" << endl;
        operand.ptr = &o_;
        operand.val.word = *operand.ptr;
    }
    else if (operand.code == 0x1e) { // [next]
        cout << "  * [next] = [0x";
        
        operand.next = mem_[pc_++];

        cout << setw(4) << setfill('0') << hex << operand.next << dec << "] = 0x";
        
        operand.ptr = &(mem_[operand.next]);
        operand.val.word = *operand.ptr;
        
        cout << setw(4) << setfill('0') << hex << operand.val.word << dec << endl;
    }
    else if (operand.code == 0x1f) { // next as a literal
        cout << "  * next = 0x";
        
        operand.next = mem_[pc_++];

        cout << setw(4) << setfill('0') << hex << operand.next << dec << endl;

        operand.ptr = &operand.val.word;
        operand.val.word = operand.next;
    }
    else { // operand as a literal
        cout << "  * lit" << endl;
        operand.ptr = &operand.val.word;
        operand.val.word = operand.code - 0x20;
    }
}

inline void dcpu16::read_instruction(instr_t & instr) {
    memset(&instr, 0, sizeof(instr)); // We want to start with it completely zeroed out.
    
    uint16_t word = mem_[pc_++]; // pc_ will wrap around automatically
    
    instr.cycle_count = cycle_count_;
    instr.address = pc_ - 1;
    
    /*
     * Least significant four bits are basic opcode number, or all zeros for non-basic opcode.
     * So, we copy them out and shift them off.
     */
    
    instr.opcode = word & 0x000f;
    word = word >> 4;
    
    if (instr.opcode == 0) {
        /*
         * Internal opcode values for non-basic opcodes are 16 greater than they are in the
         * spec so all opcodes, basic and non-basic, can be in a single contiguous range
         */
        
        instr.opcode = 16 + word & 0x003f;
        word = word >> 6;
        
        instr.operand_a.code = word & 0x003f;
        
        /*
         * We force operand b to be "literal 0x00" so we can treat the operand decoding
         * uniformly -- it will automatically not take an extra cycle to decode, for example.
         */
        
        instr.operand_b.code = 0x20;
    }
    else {
        instr.operand_a.code = word & 0x003f;
        word = word >> 6;
        
        instr.operand_b.code = word & 0x003f;
    }
    
    cout << endl;
    dump_opcode(instr.opcode);
    cout << endl;
    
    decode_operand(instr_, instr_.operand_a);
    decode_operand(instr_, instr_.operand_b);

    instr.size = pc_ - instr.address;
}

inline dcpu16::state_t dcpu16::exec_op() {
    /*
     * We cheat a little and perform the effects of the operation immediately and *then*
     * burn any extra cycles, rather than burning cycles and then making effects after that.
     * At this point, the in-between times are not observable in the world of the CPU anyway,
     * so that should be ok...
     */
    
    switch (instr_.opcode) {
        case 0: { // No Such Operation
            abort();
            break;
        }
        
        case 0x01: { // Basic op SET
            cout << "  # ";
            dump_operand(instr_.operand_a);
            cout << " <- 0x";
            cout << setw(4) << setfill('0') << hex << instr_.operand_b.val.word << dec << endl;
            
            *instr_.operand_a.ptr = instr_.operand_b.val.word;
            return CPU_STATE_READ_INSTRUCTION;
            break;
        }
            
        case 0x02: { // Basic op ADD
            int32_t a = (int32_t)instr_.operand_a.val.num;
            int32_t b = (int32_t)instr_.operand_b.val.num;
            scratch32_t res = { .num = a + b };
            word_t ovf = (res.num > INT16_MAX) ? 1 : 0;
            
            *instr_.operand_a.ptr = res.word & 0xffff;
            o_ = ovf;
            return CPU_STATE_WAIT_1;
            break;
        }
            
        case 0x03: { // Basic op SUB
            int32_t a = (int32_t)instr_.operand_a.val.num;
            int32_t b = (int32_t)instr_.operand_b.val.num;
            scratch32_t res = { .num = a - b };
            word_t ovf = (res.num < INT16_MIN) ? 0xffff : 0;
            
            *instr_.operand_a.ptr = res.word & 0xffff;
            o_ = ovf;
            return CPU_STATE_WAIT_1;
            break;
        }
            
        case 0x04: { // Basic op MUL
            int32_t a = (int32_t)instr_.operand_a.val.num;
            int32_t b = (int32_t)instr_.operand_b.val.num;
            scratch32_t res = { .num = a * b };
            word_t ovf = (res.word >> 16) & 0xffff;
            
            *instr_.operand_a.ptr = res.word & 0xffff;
            o_ = ovf;
            return CPU_STATE_WAIT_1;
            break;
        }
            
        case 0x05: { // Basic op DIV
            if (instr_.operand_b.val.word == 0) {
                *instr_.operand_a.ptr = 0;
                o_ = 0;
            }
            else {
                int32_t a = (int32_t)instr_.operand_a.val.num;
                int32_t b = (int32_t)instr_.operand_b.val.num;
                scratch32_t res = { .num = a / b };
                scratch32_t ovf = { .num = (a << 16) / b };
                
                *instr_.operand_a.ptr = res.word & 0xffff;
                o_ = ovf.word & 0xffff;
            }
            return CPU_STATE_WAIT_2;
            break;
        }
            
        case 0x06: { // Basic op MOD
            if (instr_.operand_b.val.word == 0) {
                *instr_.operand_a.ptr = 0;
            }
            else {
                int32_t a = (int32_t)instr_.operand_a.val.num;
                int32_t b = (int32_t)instr_.operand_b.val.num;
                scratch32_t res = { .num = a % b };
                
                *instr_.operand_a.ptr = res.word & 0xffff;
            }
            return CPU_STATE_WAIT_2;
            break;
        }
            
        case 0x07: { // Basic op SHL
            uint32_t a = (uint32_t)instr_.operand_a.val.word;
            uint32_t b = (uint32_t)instr_.operand_b.val.word;
            uint32_t res = a << b;
            word_t ovf = (res >> 16) & 0xffff;
            
            *instr_.operand_a.ptr = res & 0xffff;
            o_ = ovf;
            return CPU_STATE_WAIT_1;
            break;
        }
            
        case 0x08: { // Basic op SHR (FIXME: assuming logical shift right, not arithmetic?)
            uint32_t a = (uint32_t)instr_.operand_a.val.word;
            uint32_t b = (uint32_t)instr_.operand_b.val.word;
            uint32_t res = a >> b;
            word_t ovf = ((a << 16) >> b) & 0xffff;
            
            *instr_.operand_a.ptr = res & 0xffff;
            o_ = ovf;
            return CPU_STATE_WAIT_1;
            break;
        }
            
        case 0x09: { // Basic op AND
            *instr_.operand_a.ptr = instr_.operand_a.val.word & instr_.operand_b.val.word;
            return CPU_STATE_READ_INSTRUCTION;
            break;
        }
            
        case 0x0a: { // Basic op BOR
            *instr_.operand_a.ptr = instr_.operand_a.val.word | instr_.operand_b.val.word;
            return CPU_STATE_READ_INSTRUCTION;
            break;
        }
            
        case 0x0b: { // Basic op XOR
            *instr_.operand_a.ptr = instr_.operand_a.val.word ^ instr_.operand_b.val.word;
            return CPU_STATE_READ_INSTRUCTION;
            break;
        }
            
        case 0x0c: { // Basic op IFE
            if (instr_.operand_a.val.word == instr_.operand_b.val.word) {
                return CPU_STATE_WAIT_1;
            }
            else {
                return CPU_STATE_SKIP_INSTRUCTION;
            }
            break;
        }
            
        case 0x0d: { // Basic op IFN
            if (instr_.operand_a.val.word != instr_.operand_b.val.word) {
                return CPU_STATE_WAIT_1;
            }
            else {
                return CPU_STATE_SKIP_INSTRUCTION;
            }
            break;
        }
            
        case 0x0e: { // Basic op IFG
            if (instr_.operand_a.val.word > instr_.operand_b.val.word) {
                return CPU_STATE_WAIT_1;
            }
            else {
                return CPU_STATE_SKIP_INSTRUCTION;
            }
            break;
        }
        
        case 0x0f: { // Basic op IFB
            if (instr_.operand_a.val.word & instr_.operand_b.val.word) {
                return CPU_STATE_WAIT_1;
            }
            else {
                return CPU_STATE_SKIP_INSTRUCTION;
            }
            break;
        }
            
        case 0x10: { // Non-basic op "Reserved for future expansion"
            // no-op
            return CPU_STATE_READ_INSTRUCTION;
            break;
        }
            
        case 0x11: { // Non-basic op JSR
            mem_[--sp_] = pc_;
            pc_ = instr_.operand_a.val.word;
            return CPU_STATE_WAIT_1;
            break;
        }
            
        default: {
            // no-op
            return CPU_STATE_READ_INSTRUCTION;
            break;
        }
    }
}

static const char * reg_names[] = {
    "A", "B", "C", "X", "Y", "Z", "I", "J"
};

/* static */ void dcpu16::dump_register(operand_code_t reg) {
    if (reg > 0x07) {
        cout << "<error:no_such_register:" << reg << ">";
    }
    else {
        cout << reg_names[reg];
    }
}

/*
 * address is the address the operand *would* read the "next word" from, if it
 * were an operand that needed to read the next word.
 */
void dcpu16::dump_operand(operand_info_t operand) {
    if (operand.code < 0x08) {
        dump_register(operand.code);
    }
    else if (operand.code < 0x10) {
        cout << "[";
        dump_register(operand.code - 0x08);
        cout << "]";
    }
    else if (operand.code < 0x18) {
        cout << "[0x";
        cout << setw(4) << setfill('0') << hex << operand.next << dec;
        cout << " + ";
        dump_register(operand.code - 0x10);
        cout << "]";
    }
    else if (operand.code == 0x18) {
        cout << "POP";
    }
    else if (operand.code == 0x19) {
        cout << "PEEK";
    }
    else if (operand.code == 0x1a) {
        cout << "PUSH";
    }
    else if (operand.code == 0x1b) {
        cout << "SP";
    }
    else if (operand.code == 0x1c) {
        cout << "PC";
    }
    else if (operand.code == 0x1d) {
        cout << "O";
    }
    else if (operand.code == 0x1e) {
        cout << "[0x";
        cout << setw(4) << setfill('0') << hex << operand.next << dec;
        cout << "]";
    }
    else if (operand.code == 0x1f) {
        cout << "0x" << setw(4) << setfill('0') << hex << operand.val.word << dec;
    }
    else if (operand.code < 0x40) {
        cout << "0x" << setw(2) << setfill('0') << hex << (operand.code - 0x20) << dec;
    }
    else {
        abort(); // can't happen
    }
}

static const char * op_names[] = {
    NULL,
    "SET",
    "ADD",
    "SUB",
    "MUL",
    "DIV",
    "MOD",
    "SHL",
    "SHR",
    "AND",
    "BOR",
    "XOR",
    "IFE",
    "IFN",
    "IFG",
    "IFB",
    "<RES>",
    "JSR"
};

void dcpu16::dump_opcode(opcode_t opcode) {
    if (opcode < 0x01 || opcode > 0x11) {
        abort(); // can't happen
    }
    else {
        cout << op_names[opcode];
    }
}

void dcpu16::dump_instruction(instr_t instr) {
    cout << setw(8) << setfill('0') << instr.cycle_count << "  ";
    
    cout << "0x" << setw(4) << setfill('0') << hex << instr.address << dec << ": ";
    
    dump_opcode(instr.opcode);
    
    if (instr.opcode <= 0x0f) {
        cout << " ";
        dump_operand(instr.operand_a);
        cout << ", ";
        dump_operand(instr.operand_b);            
    }
    else if (instr.opcode == 0x10) {
        // no operands -- treating like 'no-op'
    }
    else if (instr.opcode == 0x11) {
        cout << " ";
        dump_operand(instr.operand_a);
    }
    else {
        abort(); // no other known opcodes
    }
    
    cout << endl;
}

void dcpu16::tick() {
    switch (state_) {
        case CPU_STATE_HALTED:
            return; // Don't even increment the cycle count
            break;
            
        case CPU_STATE_READ_INSTRUCTION:            
            read_instruction(instr_);
            dump_instruction(instr_);
            
            if (instr_.size == 3) {
                state_ = CPU_STATE_DECODE_2;
            }
            else if (instr_.size == 2) {
                state_ = CPU_STATE_DECODE_1;
            }
            else if (instr_.size == 1) {
                state_ = exec_op();
            }
            else {
                abort(); // can't happen
            }
            break;
            
        case CPU_STATE_SKIP_INSTRUCTION:
            read_instruction(instr_);
            
            /*
             * We wait an extra cycle because this state is used for failed IF_ operations
             * which need to burn an extra cycle. They need to total 3 cycles plus the cost
             * of the operands. The first was burned in initial decode. The second is then
             * burned by hitting the skip instruction state. The third is burned by the
             * wait state that we specify here.
             */
            
            state_ = CPU_STATE_WAIT_1;
            break;
            
        case CPU_STATE_DECODE_2:
            state_ = CPU_STATE_DECODE_1;
            break;
            
        case CPU_STATE_DECODE_1:
            state_ = exec_op();
            break;
            
        case CPU_STATE_WAIT_1:
            state_ = CPU_STATE_READ_INSTRUCTION;
            break;
            
        case CPU_STATE_WAIT_2:
            state_ = CPU_STATE_WAIT_1;
            break;
            
        default:
            // can't happen
            abort();
            break;
    }
    
    cycle_count_ += 1;
}
