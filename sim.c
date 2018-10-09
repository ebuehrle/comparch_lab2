#include <stdio.h>
#include "shell.h"

#define OPCODE_MASK 0xFC000000
#define FUNCT_MASK  0x0000003F
#define OPCODE_SLL  26
#define FUNCT_SLL   0

/************ Instruction Representation ************/
union {
  uint32_t code;
  struct {
    unsigned int rest: 26;
    unsigned int opcode: 6;
  };
  struct {
    unsigned int funct: 6;
    unsigned int shamt: 5;
    unsigned int rd: 5;
    unsigned int rt: 5;
    unsigned int rs: 5;
    unsigned int opcode: 6;
  } RFMT;
  struct {
    unsigned int imm: 16;
    unsigned int rt: 5;
    unsigned int rs: 5;
    unsigned int opcode: 6;
  } IFMT;
  struct {
    unsigned int address: 26;
    unsigned int opcode: 6;
  } JFMT;
} current_instruction;

/************** Instruction Record Helper Types ****************/
/* Execute Function Format */
typedef void (*execute_fct_t)(void);

/* Record Format */
typedef struct {
  uint32_t pattern;
  uint32_t mask;
  execute_fct_t instruction_function;
} opcode_record_t;

/*************** Register Manipulation *****************/
void write_register_32(uint32_t reg_number, uint32_t value) {
  if (reg_number == 0) {
    /* Protected $zero register. Discard value. */
    return;
  } else {
    NEXT_STATE.REGS[reg_number] = value;
  }
}

uint32_t read_register_32(uint32_t reg_number) {
  if (reg_number == 0) {
    /* $zero is hardwired to 0 */
    return 0;
  } else {
    return CURRENT_STATE.REGS[reg_number];
  }
}

/*************** Program Counter Manipulation **********/
void advance_pc(uint32_t offset) {
  NEXT_STATE.PC += offset;
}

/*************** Functions *****************************/
/*************** R-Format ******************************/
void add(void) {
  int32_t rt = read_register_32(current_instruction.RFMT.rt);
  int32_t rs = read_register_32(current_instruction.RFMT.rs);
  write_register_32(current_instruction.RFMT.rd, rt + rs);
}

void addu(void) {
  uint32_t rt = read_register_32(current_instruction.RFMT.rt);
  uint32_t rs = read_register_32(current_instruction.RFMT.rs);
  write_register_32(current_instruction.RFMT.rd, rt + rs);
}

void sub(void) {
  int32_t rt = read_register_32(current_instruction.RFMT.rt);
  int32_t rs = read_register_32(current_instruction.RFMT.rs);
  write_register_32(current_instruction.RFMT.rd, rt - rs);
}

void subu(void) {
  uint32_t rt = read_register_32(current_instruction.RFMT.rt);
  uint32_t rs = read_register_32(current_instruction.RFMT.rs);
  write_register_32(current_instruction.RFMT.rd, rt - rs);
}

void jr(void) {
  uint32_t rs = read_register_32(current_instruction.IFMT.rs);
  NEXT_STATE.PC = rs;
}

/*************** I-Format ******************************/
void li(void) {
  /* This is a pseudoinstruction! */
}

void lui(void) {
  uint32_t result = current_instruction.IFMT.imm << 16;
  write_register_32(current_instruction.IFMT.rt, result);
}

void ori(void) {
  uint32_t rs = read_register_32(current_instruction.IFMT.rs);
  uint32_t result = rs | current_instruction.IFMT.imm;
  write_register_32(current_instruction.IFMT.rt, result);
}

void addi(void) {
  int32_t rs = read_register_32(current_instruction.IFMT.rs);
  int32_t result = rs + current_instruction.IFMT.imm;
  write_register_32(current_instruction.IFMT.rt, result);
}

void addiu(void) {
  uint32_t rs = read_register_32(current_instruction.IFMT.rs);
  uint32_t result = rs | current_instruction.IFMT.imm;
  write_register_32(current_instruction.IFMT.rt, result);
}

void lw(void) {
  uint32_t rs = read_register_32(current_instruction.IFMT.rs);
  uint32_t result = mem_read_32(rs);
  write_register_32(current_instruction.IFMT.rt, result);
}

void sw(void) {
  uint32_t rt = read_register_32(current_instruction.IFMT.rt);
  uint32_t rs = read_register_32(current_instruction.IFMT.rs);
  mem_write_32(rs, rt);
}

void bne(void) {
  uint32_t rt = read_register_32(current_instruction.IFMT.rt);
  uint32_t rs = read_register_32(current_instruction.IFMT.rs);
  if (rs != rt) {
    advance_pc((current_instruction.IFMT.imm - 1) << 2);
  }
}

void beq(void) {
  uint32_t rt = read_register_32(current_instruction.IFMT.rt);
  uint32_t rs = read_register_32(current_instruction.IFMT.rs);
  if (rs == rt) {
    advance_pc((current_instruction.IFMT.imm - 1) << 2);
  }
}

void bgtz(void) {
  uint32_t rs = read_register_32(current_instruction.IFMT.rs);
  if (rs > 0) {
    advance_pc((current_instruction.IFMT.imm - 1) << 2);
  }
}

void slti(void) {
  uint32_t rs = read_register_32(current_instruction.IFMT.rs);
  if (rs < current_instruction.IFMT.imm) {
    write_register_32(current_instruction.IFMT.rt, 1);
  } else {
    write_register_32(current_instruction.IFMT.rt, 0);
  }
}

void lb(void) {
  uint32_t rs = read_register_32(current_instruction.IFMT.rs);
  write_register_32(current_instruction.IFMT.rt, (mem_read_32(rs) & 0x000000FF));
}

void sb(void) {
  uint32_t rs = read_register_32(current_instruction.IFMT.rs);
  uint32_t rt = read_register_32(current_instruction.IFMT.rt);
  uint32_t current_mem_content = mem_read_32(rs);
  mem_write_32(rs, (current_mem_content & 0xFFFFFF00) | (rt & 0x000000FF));
}

/*************** J-Format ******************************/
void j(void) {
  NEXT_STATE.PC = (CURRENT_STATE.PC & 0xF0000000) | (current_instruction.JFMT.address << 2);
}

void jal(void) {
  write_register_32(31, CURRENT_STATE.PC + 4);
  NEXT_STATE.PC = (CURRENT_STATE.PC & 0xF0000000) | (current_instruction.JFMT.address << 2);
}

/*************** Other *********************************/
void halt(void) {
  RUN_BIT = 0;
}

/*************** Execution Pointer *********************/
execute_fct_t instruction_execution = &halt;

/*************** Function Records **********************/
opcode_record_t INSTRUCTION_RECORD[] = {
  {(0x00 << OPCODE_SLL) | (0x20 << FUNCT_SLL), OPCODE_MASK | FUNCT_MASK, &add},
  {(0x00 << OPCODE_SLL) | (0x21 << FUNCT_SLL), OPCODE_MASK | FUNCT_MASK, &addu},
  {(0x00 << OPCODE_SLL) | (0x22 << FUNCT_SLL), OPCODE_MASK | FUNCT_MASK, &sub},
  {(0x00 << OPCODE_SLL) | (0x23 << FUNCT_SLL), OPCODE_MASK | FUNCT_MASK, &subu},
  {(0x00 << OPCODE_SLL) | (0x08 << FUNCT_SLL), OPCODE_MASK | FUNCT_MASK, &jr},
  {0x0F << OPCODE_SLL, OPCODE_MASK, &lui},
  {0x0D << OPCODE_SLL, OPCODE_MASK, &ori},
  {0x08 << OPCODE_SLL, OPCODE_MASK, &addi},
  {0x09 << OPCODE_SLL, OPCODE_MASK, &addiu},
  {0x23 << OPCODE_SLL, OPCODE_MASK, &lw},
  {0x2B << OPCODE_SLL, OPCODE_MASK, &sw},
  {0x05 << OPCODE_SLL, OPCODE_MASK, &bne},
  {0x04 << OPCODE_SLL, OPCODE_MASK, &beq},
  {0x07 << OPCODE_SLL, OPCODE_MASK, &bgtz},
  {0x0A << OPCODE_SLL, OPCODE_MASK, &slti},
  {0x02 << OPCODE_SLL, OPCODE_MASK, &j},
  {0x03 << OPCODE_SLL, OPCODE_MASK, &jal},
  {0x28 << OPCODE_SLL, OPCODE_MASK, &sb},
  {0x20 << OPCODE_SLL, OPCODE_MASK, &lb},
};

int num_commands(void) {
  return (sizeof(INSTRUCTION_RECORD) / sizeof(opcode_record_t));
}

/*************** Fetch-Decode-Execute Cycle ************/
void fetch()
{
  current_instruction.code = mem_read_32(CURRENT_STATE.PC);
  NEXT_STATE.PC = CURRENT_STATE.PC + 4;
}

void decode()
{
  instruction_execution = &halt;
  for (int i = 0; i < num_commands(); i++) {
    if ((current_instruction.code & INSTRUCTION_RECORD[i].mask) == INSTRUCTION_RECORD[i].pattern) {
      instruction_execution = INSTRUCTION_RECORD[i].instruction_function;
    }
  }
}

void execute()
{
  (*instruction_execution)();
}

void process_instruction()
{
    /* execute one instruction here. You should use CURRENT_STATE and modify
     * values in NEXT_STATE. You can call mem_read_32() and mem_write_32() to
     * access memory. */
    fetch();
    decode();
    execute();
}