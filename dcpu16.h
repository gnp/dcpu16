//
//  dcpu16.h
//  dcpu16
//
//  Based on version 1.1 of the spec at http://0x10c.com/doc/dcpu-16.txt as of 2012-06-30.
//
//  Created by Gregor N. Purdy, Sr. on 2012-06-06.
//  Copyright (c) 2012 Gregor N. Purdy, Sr. All rights reserved.
//

#ifndef dcpu16_dcpu16_h
#define dcpu16_dcpu16_h

#include <stdint.h>

class dcpu16 {
public:
    
    typedef uint16_t word_t;
    typedef word_t addr_t;
    
    typedef uint8_t opcode_t; // four-bit field for basic instruction, 16 + six-bit field for non-basic
    typedef uint8_t operand_code_t; // six-bit field from the decoded instruction
    
    typedef union {
        uint16_t word; // unsigned 16-bit integer
        int16_t num; // two's complement signed 16-bit integer
    } scratch16_t;
    
    typedef union {
        uint32_t word; // unsigned 32-bit integer
        int32_t num; // two's complement signed 32-bit integer
    } scratch32_t;
    
    typedef struct {
        operand_code_t code;
        word_t next; // for operands that fetch the "next" word, it is recorded here
        word_t * ptr; // pointer to where to get/set the value (will point to val for const)
        scratch16_t val; // the value fetched    
    } operand_info_t;
    
    typedef struct {
        /*
         * System state when the instruction was fetched:
         */
        
        uint64_t cycle_count; // cycle count when the instruction was read
        addr_t address; // pc at which it was found (so one or two following words can be found, if necessary)
        uint8_t size; // number of words long the instruction is
        
        /*
         * Decoded instruction:
         */
        
        opcode_t opcode; // basic opcode value, or non-basic opcode value + 16
        
        operand_info_t operand_a;
        operand_info_t operand_b;
    } instr_t;

private:
    
    typedef enum {
        CPU_STATE_READ_INSTRUCTION = 0, // Zero for startup
        CPU_STATE_SKIP_INSTRUCTION,
        CPU_STATE_HALTED,
        CPU_STATE_DECODE_2,
        CPU_STATE_DECODE_1,
        CPU_STATE_WAIT_2,
        CPU_STATE_WAIT_1,
        CPU_STATE_EXEC_OP,
    } state_t;
    
private:
    
    /*
     * State per the spec:
     */
    
    word_t reg_[8]; // Register file, with registers in this order: A, B, C, X, Y, Z, I, J
    word_t mem_[UINT16_MAX + 1]; // System Memory
    word_t pc_; // Program Counter
    word_t sp_; // Stack Pointer
    word_t o_; // Overflow
    
    /*
     * Additional state for the simulator:
     */
    
    uint64_t cycle_count_; // How many cycles the cpu has run
    state_t state_; // CPU State
    instr_t instr_; // Current Instruction
    
public:
    
    dcpu16();
    
    void load(addr_t addr, word_t * words, word_t count);
    void reset();
    
    void tick();
    
    uint64_t cycle_count() const;
    
private:

    static void dump_opcode(opcode_t opcode);
    static void dump_register(operand_code_t reg);
    void dump_operand(operand_info_t operand);
    void dump_instruction(instr_t instr);
    
    void read_instruction(instr_t & instr);
    void decode_operand(instr_t & instr, operand_info_t & operand);

    word_t get_value(addr_t address, operand_code_t value);
    void set_value(addr_t address, operand_code_t operand, word_t word);
    state_t exec_op();

};

#endif
