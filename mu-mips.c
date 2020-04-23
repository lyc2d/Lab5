#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "mu-mips.h"
int ENABLE_FORWARDING = 0;
int stall = 0;
uint32_t ID_EX_rs = 0;
uint32_t ID_EX_rt = 0;
uint32_t EX_MEM_RegisterRd = 0;
uint32_t EX_MEM_RegisterRt = 0;
uint32_t MEM_WB_RegisterRt = 0;
uint32_t MEM_WB_RegisterRd = 0;
int EX_MEM_RegWrite = 1;
int MEM_WB_RegWrite = 1;
int forwardA = 0;
int forwardB = 0;
int branch = 0;

/***************************************************************/
/* Print out a list of commands available                                                                  */
/***************************************************************/
void help() {
    printf("------------------------------------------------------------------\n\n");
    printf("\t**********MU-MIPS Help MENU**********\n\n");
    printf("sim\t-- simulate program to completion \n");
    printf("run <n>\t-- simulate program for <n> instructions\n");
    printf("rdump\t-- dump register values\n");
    printf("reset\t-- clears all registers/memory and re-loads the program\n");
    printf("input <reg> <val>\t-- set GPR <reg> to <val>\n");
    printf("mdump <start> <stop>\t-- dump memory from <start> to <stop> address\n");
    printf("high <val>\t-- set the HI register to <val>\n");
    printf("low <val>\t-- set the LO register to <val>\n");
    printf("print\t-- print the program loaded into memory\n");
    printf("show\t-- print the current content of the pipeline registers\n");
    printf("?\t-- display help menu\n");
    printf("quit\t-- exit the simulator\n\n");
    printf("------------------------------------------------------------------\n\n");
}

/***************************************************************/
/* Read a 32-bit word from memory                                                                            */
/***************************************************************/
uint32_t mem_read_32(uint32_t address)
{
    int i;
    for (i = 0; i < NUM_MEM_REGION; i++) {
        if ( (address >= MEM_REGIONS[i].begin) &&  ( address <= MEM_REGIONS[i].end) ) {
            uint32_t offset = address - MEM_REGIONS[i].begin;
            return (MEM_REGIONS[i].mem[offset+3] << 24) |
            (MEM_REGIONS[i].mem[offset+2] << 16) |
            (MEM_REGIONS[i].mem[offset+1] <<  8) |
            (MEM_REGIONS[i].mem[offset+0] <<  0);
        }
    }
    return 0;
}

/***************************************************************/
/* Write a 32-bit word to memory                                                                                */
/***************************************************************/
void mem_write_32(uint32_t address, uint32_t value)
{
    int i;
    uint32_t offset;
    for (i = 0; i < NUM_MEM_REGION; i++) {
        if ( (address >= MEM_REGIONS[i].begin) && (address <= MEM_REGIONS[i].end) ) {
            offset = address - MEM_REGIONS[i].begin;
            
            MEM_REGIONS[i].mem[offset+3] = (value >> 24) & 0xFF;
            MEM_REGIONS[i].mem[offset+2] = (value >> 16) & 0xFF;
            MEM_REGIONS[i].mem[offset+1] = (value >>  8) & 0xFF;
            MEM_REGIONS[i].mem[offset+0] = (value >>  0) & 0xFF;
        }
    }
}

/***************************************************************/
/* Execute one cycle                                                                                                              */
/***************************************************************/
void cycle() {
    handle_pipeline();
    CURRENT_STATE = NEXT_STATE;
    CYCLE_COUNT++;
}

/***************************************************************/
/* Simulate MIPS for n cycles                                                                                       */
/***************************************************************/
void run(int num_cycles) {
    
    if (RUN_FLAG == FALSE) {
        printf("Simulation Stopped\n\n");
        return;
    }
    
    printf("Running simulator for %d cycles...\n\n", num_cycles);
    int i;
    for (i = 0; i < num_cycles; i++) {
        if (RUN_FLAG == FALSE) {
            printf("Simulation Stopped.\n\n");
            break;
        }
        cycle();
    }
}

/***************************************************************/
/* simulate to completion                                                                                               */
/***************************************************************/
void runAll() {
    if (RUN_FLAG == FALSE) {
        printf("Simulation Stopped.\n\n");
        return;
    }
    
    printf("Simulation Started...\n\n");
    while (RUN_FLAG){
        cycle();
    }
    printf("Simulation Finished.\n\n");
}

/***************************************************************/
/* Dump a word-aligned region of memory to the terminal                              */
/***************************************************************/
void mdump(uint32_t start, uint32_t stop) {
    uint32_t address;
    
    printf("-------------------------------------------------------------\n");
    printf("Memory content [0x%08x..0x%08x] :\n", start, stop);
    printf("-------------------------------------------------------------\n");
    printf("\t[Address in Hex (Dec) ]\t[Value]\n");
    for (address = start; address <= stop; address += 4){
        printf("\t0x%08x (%d) :\t0x%08x\n", address, address, mem_read_32(address));
    }
    printf("\n");
}

/***************************************************************/
/* Dump current values of registers to the teminal                                              */
/***************************************************************/
void rdump() {
    int i;
    printf("-------------------------------------\n");
    printf("Dumping Register Content\n");
    printf("-------------------------------------\n");
    printf("# Instructions Executed\t: %u\n", INSTRUCTION_COUNT);
    printf("# Cycles Executed\t: %u\n", CYCLE_COUNT);
    printf("PC\t: 0x%08x\n", CURRENT_STATE.PC);
    printf("-------------------------------------\n");
    printf("[Register]\t[Value]\n");
    printf("-------------------------------------\n");
    for (i = 0; i < MIPS_REGS; i++){
        printf("[R%d]\t: 0x%08x\n", i, CURRENT_STATE.REGS[i]);
    }
    printf("-------------------------------------\n");
    printf("[HI]\t: 0x%08x\n", CURRENT_STATE.HI);
    printf("[LO]\t: 0x%08x\n", CURRENT_STATE.LO);
    printf("-------------------------------------\n");
}

/***************************************************************/
/* Read a command from standard input.                                                               */
/***************************************************************/
void handle_command() {
    char buffer[20];
    uint32_t start, stop, cycles;
    uint32_t register_no;
    int register_value;
    int hi_reg_value, lo_reg_value;
    
    printf("MU-MIPS SIM:> ");
    
    if (scanf("%s", buffer) == EOF){
        exit(0);
    }
    
    switch(buffer[0]) {
        case 'S':
        case 's':
            if (buffer[1] == 'h' || buffer[1] == 'H'){
                show_pipeline();
            }else {
                runAll();
            }
            break;
        case 'M':
        case 'm':
            if (scanf("%x %x", &start, &stop) != 2){
                break;
            }
            mdump(start, stop);
            break;
        case '?':
            help();
            break;
        case 'Q':
        case 'q':
            printf("**************************\n");
            printf("Exiting MU-MIPS! Good Bye...\n");
            printf("**************************\n");
            exit(0);
        case 'R':
        case 'r':
            if (buffer[1] == 'd' || buffer[1] == 'D'){
                rdump();
            }else if(buffer[1] == 'e' || buffer[1] == 'E'){
                reset();
            }
            else {
                if (scanf("%d", &cycles) != 1) {
                    break;
                }
                run(cycles);
            }
            break;
        case 'I':
        case 'i':
            if (scanf("%u %i", &register_no, &register_value) != 2){
                break;
            }
            CURRENT_STATE.REGS[register_no] = register_value;
            NEXT_STATE.REGS[register_no] = register_value;
            break;
        case 'H':
        case 'h':
            if (scanf("%i", &hi_reg_value) != 1){
                break;
            }
            CURRENT_STATE.HI = hi_reg_value;
            NEXT_STATE.HI = hi_reg_value;
            break;
        case 'L':
        case 'l':
            if (scanf("%i", &lo_reg_value) != 1){
                break;
            }
            CURRENT_STATE.LO = lo_reg_value;
            NEXT_STATE.LO = lo_reg_value;
            break;
        case 'P':
        case 'p':
            print_program();
            break;
        default:
            printf("Invalid Command.\n");
            break;
    }
}

/***************************************************************/
/* reset registers/memory and reload program                                                    */
/***************************************************************/
void reset() {
    int i;
    /*reset registers*/
    for (i = 0; i < MIPS_REGS; i++){
        CURRENT_STATE.REGS[i] = 0;
    }
    CURRENT_STATE.HI = 0;
    CURRENT_STATE.LO = 0;
    
    for (i = 0; i < NUM_MEM_REGION; i++) {
        uint32_t region_size = MEM_REGIONS[i].end - MEM_REGIONS[i].begin + 1;
        memset(MEM_REGIONS[i].mem, 0, region_size);
    }
    
    /*load program*/
    load_program();
    
    /*reset PC*/
    INSTRUCTION_COUNT = 0;
    CURRENT_STATE.PC =  MEM_TEXT_BEGIN;
    NEXT_STATE = CURRENT_STATE;
    RUN_FLAG = TRUE;
}

/***************************************************************/
/* Allocate and set memory to zero                                                                            */
/***************************************************************/
void init_memory() {
    int i;
    for (i = 0; i < NUM_MEM_REGION; i++) {
        uint32_t region_size = MEM_REGIONS[i].end - MEM_REGIONS[i].begin + 1;
        MEM_REGIONS[i].mem = malloc(region_size);
        memset(MEM_REGIONS[i].mem, 0, region_size);
    }
}

/**************************************************************/
/* load program into memory                                                                                      */
/**************************************************************/
void load_program() {
    FILE * fp;
    int i, word;
    uint32_t address;
    
    /* Open program file. */
    fp = fopen(prog_file, "r");
    if (fp == NULL) {
        printf("Error: Can't open program file %s\n", prog_file);
        exit(-1);
    }
    
    /* Read in the program. */
    
    i = 0;
    while( fscanf(fp, "%x\n", &word) != EOF ) {
        address = MEM_TEXT_BEGIN + i;
        mem_write_32(address, word);
        printf("writing 0x%08x into address 0x%08x (%d)\n", word, address, address);
        i += 4;
    }
    PROGRAM_SIZE = i/4;
    printf("Program loaded into memory.\n%d words written into memory.\n\n", PROGRAM_SIZE);
    fclose(fp);
}

/************************************************************/
/* maintain the pipeline                                                                                           */
/************************************************************/
void handle_pipeline()
{
    /*INSTRUCTION_COUNT should be incremented when instruction is done*/
    /*Since we do not have branch/jump instructions, INSTRUCTION_COUNT should be incremented in WB stage */
    
    WB();
    MEM();
    EX();
    ID();
    IF();
}

/************************************************************/
/* writeback (WB) pipeline stage:                                                                          */
/************************************************************/
void WB()
{
    // do system call here in the last instruction
    /*IMPLEMENT THIS*/
    uint32_t line = MEM_WB.IR;
    uint32_t op = 0;
    uint32_t funct;
    uint32_t rt = 0;
    uint32_t rd = 0;
    
    op = (MEM_WB.IR & 0xFC000000) >> 26;
	funct = MEM_WB.IR & 0x0000003F;
    rt = (line & 0x001F0000) >> 16;
    rd = (line & 0x0000F800) >> 11;
    
    if (op == 0x00) {
		switch (funct) {
		case 0x00: //SLL, ALU Instruction
			NEXT_STATE.REGS[rd] = MEM_WB.ALUOutput;
			break;
		case 0x02: //SRL, ALU Instruction
			NEXT_STATE.REGS[rd] = MEM_WB.ALUOutput;
			break;
		case 0x03: //SRA, ALU Instruction
			NEXT_STATE.REGS[rd] = MEM_WB.ALUOutput;
			break;
		case 0x0C: //SYSCALL
			if (MEM_WB.ALUOutput == 0xA) {
				RUN_FLAG = FALSE;
				MEM_WB.ALUOutput = 0x0;
				break;
			}
			break;
		case 0x10: //MFHI, Load/Store Instruction
			NEXT_STATE.REGS[rd] = MEM_WB.HI;
			break;
		case 0x11: //MTHI, Load/Store Instruction
			NEXT_STATE.HI = MEM_WB.ALUOutput;
			break;
		case 0x12: //MFLO, Load/Store Instruction
			NEXT_STATE.REGS[rd] = MEM_WB.LO;
			break;
		case 0x13: //MTLO, Load/Store Instruction
			NEXT_STATE.LO = MEM_WB.ALUOutput;
			break;
		case 0x18: //MULT, ALU Instruction
			NEXT_STATE.LO = MEM_WB.LO;
			NEXT_STATE.HI = MEM_WB.HI;
			break;
		case 0x19: //MULTU, ALU Instruction
			NEXT_STATE.LO = MEM_WB.LO;
			NEXT_STATE.HI = MEM_WB.HI;
			break;
		case 0x1A: //DIV, ALU Instruction
			NEXT_STATE.LO = MEM_WB.LO;
			NEXT_STATE.HI = MEM_WB.HI;
			break;
		case 0x1B: //DIVU, ALU Instruction
			NEXT_STATE.LO = MEM_WB.LO;
			NEXT_STATE.HI = MEM_WB.HI;
			break;
		case 0x20: //ADD, ALU Instruction
			NEXT_STATE.REGS[rd] = MEM_WB.ALUOutput;
			break;
		case 0x21: //ADDU, ALU Instruction
			NEXT_STATE.REGS[rd] = MEM_WB.ALUOutput;
			break;
		case 0x22: //SUB, ALU Instruction
			NEXT_STATE.REGS[rd] = MEM_WB.ALUOutput;
			break;
		case 0x23: //SUBU, ALU Instruction
			NEXT_STATE.REGS[rd] = MEM_WB.ALUOutput;
			break;
		case 0x24: //AND, ALU Instruction
			NEXT_STATE.REGS[rd] = MEM_WB.ALUOutput;
			break;
		case 0x25: //OR, ALU Instruction
			NEXT_STATE.REGS[rd] = MEM_WB.ALUOutput;
			break;
		case 0x26: //XOR, ALU Instruction
			NEXT_STATE.REGS[rd] = MEM_WB.ALUOutput;
			break;
		case 0x27: //NOR, ALU Instruction
			NEXT_STATE.REGS[rd] = MEM_WB.ALUOutput;
			break;
		case 0x2A: //SLT, ALU Instruction
			NEXT_STATE.REGS[rd] = MEM_WB.ALUOutput;
			break;
		case 0x08: //JR
			break;
		case 0x09: //JALR
			break;
		default:
			printf("Funct instruction at 0x%x is not implemented!\n", funct);
			break;
		}
	} else {
		switch (op) {
		case 0x08: //ADDI, ALU Instruction
			NEXT_STATE.REGS[rt] = MEM_WB.ALUOutput;
			break;
		case 0x09: //ADDIU, ALU Instruction
			NEXT_STATE.REGS[rt] = MEM_WB.ALUOutput;
			break;
		case 0x0A: //SLTI, ALU Instruction
			NEXT_STATE.REGS[rt] = MEM_WB.ALUOutput;
			break;
		case 0x0C: //ANDI, ALU Instruction
			NEXT_STATE.REGS[rt] = MEM_WB.ALUOutput;
			break;
		case 0x0D: //ORI, ALU Instruction
			NEXT_STATE.REGS[rt] = MEM_WB.ALUOutput;
			break;
		case 0x0E: //XORI, ALU Instruction
			NEXT_STATE.REGS[rt] = MEM_WB.ALUOutput;
			break;
		case 0x0F: //LUI, Load/Store Instruction
			NEXT_STATE.REGS[rt] = MEM_WB.LMD;
			break;
		case 0x20: //LB, Load/Store Instruction
			NEXT_STATE.REGS[rt] = MEM_WB.LMD;
			break;
		case 0x21: //LH, Load/Store Instruction
			NEXT_STATE.REGS[rt] = MEM_WB.LMD;
			break;
		case 0x23: //LW, Load/Store Instruction
			NEXT_STATE.REGS[rt] = MEM_WB.LMD;
			break;
		case 0x28: //SB, Load/Store Instruction
			// do nothing
			break;
		case 0x29: //SH, Load/Store Instruction
			// do nothing
			break;
		case 0x2B: //SW, Load/Store Instruction
			// do nothing
			break;
		case 0x01: //BLTZ and BGEZ
			break;
		case 0x02: //J
			break;
		case 0x03: //JAL
			break;
		case 0x04: //BEQ
			break;
		case 0x05: //BNE
			break;
		case 0x06: //BLEZ
			break;
		case 0x07: //BGTZ
			break;
		default:
			// put more things here
			printf("Opcode instruction at 0x%x is not implemented!\n", opcode);
			break;
		}
	}
	if (stall != 0) {
		stall--;
	}
	INSTRUCTION_COUNT++;
}

/************************************************************/
/* memory access (MEM) pipeline stage:                                                          */
/************************************************************/
void MEM()
{
    MEM_WB = passRegs(EX_MEM);
    /*IMPLEMENT THIS*/
    MEM_WB.IR = EX_MEM.IR;
    MEM_WB.ALUOutput = EX_MEM.ALUOutput;
    uint32_t line = MEM_WB.IR;
    uint32_t op = 0;
    if ((line | 0x03FFFFFF) !=  0x03FFFFFF){
        op = line & 0xFC000000;
        switch (op) {
            case 0x80000000:
                //LB
                MEM_WB.LMD = mem_read_32(EX_MEM.ALUOutput) & 0x000000FF;
                break;
            case 0x81000000:
                //LH
                MEM_WB.LMD = mem_read_32(EX_MEM.ALUOutput) & 0x0000FFFF;
                break;
            case 0x8C000000:
                //LW
                MEM_WB.LMD = mem_read_32(EX_MEM.ALUOutput);
                break;
            case 0xA0000000:
                //SB
                mem_write_32(EX_MEM.ALUOutput, CURRENT_STATE.REGS[EX_MEM.B] & 0x000000FF);
                break;
            case 0xA1000000:
                //SH
                mem_write_32(EX_MEM.ALUOutput, CURRENT_STATE.REGS[EX_MEM.B] & 0x0000FFFF);
                break;
            case 0xAC000000:
                //SW
                printf("Store word called\n");
                mem_write_32(EX_MEM.ALUOutput, CURRENT_STATE.REGS[EX_MEM.B]);
                break;
                
            default:
                break;
        }
    }
}

/************************************************************/
/* execution (EX) pipeline stage:                                                                          */
/************************************************************/
void EX()
{
   EX_MEM.IR = ID_EX.IR;
	uint32_t opcode;
	uint32_t funct;
	uint32_t sa;
	uint32_t rt;
	uint32_t rd;
	uint32_t immediate;
	uint64_t product, p1, p2;
	sa = (EX_MEM.IR & 0x000007C0) >> 6;
	opcode = (EX_MEM.IR & 0xFC000000) >> 26;
	funct = EX_MEM.IR & 0x0000003F;
	rt = (EX_MEM.IR & 0x001F0000) >> 16;
	rd = (EX_MEM.IR & 0x0000F800) >> 11;
	immediate = EX_MEM.IR & 0x0000FFFF;

	EX_MEM_RegisterRt = rt;
	EX_MEM_RegisterRd = rd;
	if (EX_MEM.IR == 0) {
		return;
	}

	if (ENABLE_FORWARDING) {
		ID_EX.A = CURRENT_STATE.REGS[ID_EX_rs];
		ID_EX.B = CURRENT_STATE.REGS[ID_EX_rt];
		ID_EX.imm = immediate;
		if (forwardA == 0x10) {
			if (((MEM_WB.IR & 0xFC000000) >> 26) == 0x23) {
				ID_EX.A = MEM_WB.LMD; // LW
			} else {
				ID_EX.A = EX_MEM.ALUOutput;
			}
		} else if (forwardB == 0x10) {
			if (((MEM_WB.IR & 0xFC000000) >> 26) == 0x23) {
				ID_EX.B = MEM_WB.LMD; // LW
			} else {
				ID_EX.B = EX_MEM.ALUOutput;
			}
		}

		if (forwardA == 0x01) {
			if (((MEM_WB.IR & 0xFC000000) >> 26) == 0x23) {
				ID_EX.A = MEM_WB.LMD; //LW
			} else {
				ID_EX.A = MEM_WB.ALUOutput;
			}
		} else if (forwardB == 0x01) {
			if (((MEM_WB.IR & 0xFC000000) >> 26) == 0x23) {
				ID_EX.B = MEM_WB.LMD; //LW
			} else {
				ID_EX.B = MEM_WB.ALUOutput;
			}
		}
	}
	forwardA = 0x00;
	forwardB = 0x00; //reset

	//Different operation according to different instruction
	if (opcode == 0x00) {
		switch (funct) {
		case 0x00: //SLL, ALU Instruction
			EX_MEM.ALUOutput = ID_EX.B << sa;
			break;
		case 0x02: //SRL, ALU Instruction
			EX_MEM.ALUOutput = ID_EX.B >> sa;
			break;
		case 0x03: //SRA, ALU Instruction
			if ((ID_EX.B & 0x80000000) == 1) {
				EX_MEM.ALUOutput = ~(~ID_EX.B >> sa);
			} else {
				EX_MEM.ALUOutput = ID_EX.B >> sa;
			}
			break;
		case 0x0C: //SYSCALL
			EX_MEM.ALUOutput = 0xA;
			break;
		case 0x10: //MFHI, Load/Store Instruction
			EX_MEM.HI = CURRENT_STATE.HI;
			break;
		case 0x11: //MTHI, Load/Store Instruction
			EX_MEM.ALUOutput = ID_EX.A;
			break;
		case 0x12: //MFLO, Load/Store Instruction
			EX_MEM.LO = CURRENT_STATE.LO;
			break;
		case 0x13: //MTLO, Load/Store Instruction
			EX_MEM.ALUOutput = ID_EX.A;
			break;
		case 0x18: //MULT, ALU Instruction
			if ((ID_EX.A & 0x80000000) == 0x80000000) {
				p1 = 0xFFFFFFFF00000000 | ID_EX.A;
			} else {
				p1 = 0x00000000FFFFFFFF & ID_EX.A;
			}
			if ((ID_EX.B & 0x80000000) == 0x80000000) {
				p2 = 0xFFFFFFFF00000000 | ID_EX.B;
			} else {
				p2 = 0x00000000FFFFFFFF & ID_EX.B;
			}
			product = p1 * p2;
			EX_MEM.LO = (product & 0X00000000FFFFFFFF);
			EX_MEM.HI = (product & 0XFFFFFFFF00000000) >> 32;
			break;
		case 0x19: //MULTU, ALU Instruction
			product = (uint64_t) ID_EX.A * (uint64_t) ID_EX.B;
			EX_MEM.LO = (product & 0X00000000FFFFFFFF);
			EX_MEM.HI = (product & 0XFFFFFFFF00000000) >> 32;
			break;
		case 0x1A: //DIV, ALU Instruction
			if (ID_EX.B != 0) {
				EX_MEM.LO = (int32_t) ID_EX.A / (int32_t) ID_EX.B;
				EX_MEM.HI = (int32_t) ID_EX.A % (int32_t) ID_EX.B;
			}
			break;
		case 0x1B: //DIVU, ALU Instruction
			if (ID_EX.B != 0) {
				EX_MEM.LO = ID_EX.A / ID_EX.B;
				EX_MEM.HI = ID_EX.A % ID_EX.B;
			}
			break;
		case 0x20: //ADD, ALU Instruction
			EX_MEM.ALUOutput = ID_EX.A + ID_EX.B;

			break;
		case 0x21: //ADDU, ALU Instruction
			EX_MEM.ALUOutput = ID_EX.A + ID_EX.B;

			break;
		case 0x22: //SUB, ALU Instruction
			EX_MEM.ALUOutput = ID_EX.A - ID_EX.B;

			break;
		case 0x23: //SUBU, ALU Instruction
			EX_MEM.ALUOutput = ID_EX.A - ID_EX.B;

			break;
		case 0x24: //AND, ALU Instruction
			EX_MEM.ALUOutput = ID_EX.A & ID_EX.B;

			break;
		case 0x25: //OR, ALU Instruction
			EX_MEM.ALUOutput = ID_EX.A | ID_EX.B;

			break;
		case 0x26: //XOR, ALU Instruction
			EX_MEM.ALUOutput = ID_EX.A ^ ID_EX.B;

			break;
		case 0x27: //NOR, ALU Instruction
			EX_MEM.ALUOutput = ~(ID_EX.A | ID_EX.B);
			break;
		case 0x2A: //SLT, ALU Instruction
			if (ID_EX.A < ID_EX.B) {
				EX_MEM.ALUOutput = 0x1;
			} else {
				EX_MEM.ALUOutput = 0x0;
			}
			break;
		case 0x08: //JR
			CURRENT_STATE.PC = ID_EX.A;
			branch = 1;
			break;
		case 0x09: //JALR
			CURRENT_STATE.PC = ID_EX.A;
			NEXT_STATE.REGS[31] = ID_EX.PC;
			branch = 1;
			break;
		default:
			printf("Funct Instruction case 0x%x is not implemented!\n", funct);
			break;
		}
	} else {
		switch (opcode) {
		case 0x01:
			if (rt == 0) { //BLTZ, Jump, branch instruction
				if ((ID_EX.A & 0x80000000) > 0) {
					ID_EX.imm = ID_EX.imm << 2;
					CURRENT_STATE.PC = (ID_EX.PC
							+ ((ID_EX.imm & 0x8000) > 0 ?
									(ID_EX.imm | 0xFFFF0000) :
									(ID_EX.imm & 0x0000FFFF)) - 4);
					branch = 1;

				}
			} else if (rt == 1) { //BGEZ, Jump, branch instruction
				if ((ID_EX.A & 0x80000000) == 0x0) {
					ID_EX.imm = ID_EX.imm << 2;
					CURRENT_STATE.PC = (ID_EX.PC
							+ ((ID_EX.imm & 0x8000) > 0 ?
									(ID_EX.imm | 0xFFFF0000) :
									(ID_EX.imm & 0x0000FFFF)) - 4);
					branch = 1;
				}
			}
			break;
		case 0x02: //J, Jump, branch instruction
			CURRENT_STATE.PC = (ID_EX.PC & 0xF0000000)
					| ((ID_EX.IR & 0x03FFFFFF) << 2);
			branch = 1;
			break;
		case 0x03: //JAL, Jump, branch instruction
			CURRENT_STATE.PC = (ID_EX.PC & 0xF0000000)
					| ((ID_EX.IR & 0x03FFFFFF) << 2);
			NEXT_STATE.REGS[31] = ID_EX.PC;
			branch = 1;
			break;
		case 0x04: //BEQ, Jump, branch instruction
			if (ID_EX.A == ID_EX.B) {
				ID_EX.imm = ID_EX.imm << 2;
				CURRENT_STATE.PC = (ID_EX.PC
						+ ((ID_EX.imm & 0x8000) > 0 ?
								(ID_EX.imm | 0xFFFF0000) :
								(ID_EX.imm & 0x0000FFFF)) - 4);
				branch = 1;
			}
			break;
		case 0x05: //BNE, Jump, branch instruction
			if (ID_EX.A != ID_EX.B) {
				ID_EX.imm = ID_EX.imm << 2;
				CURRENT_STATE.PC = (ID_EX.PC
						+ ((ID_EX.imm & 0x8000) > 0 ?
								(ID_EX.imm | 0xFFFF0000) :
								(ID_EX.imm & 0x0000FFFF)) - 4);
				branch = 1;
			}
			break;
		case 0x06: //BLEZ, Jump, branch instruction
			if (ID_EX.A <= 0) {
				ID_EX.imm = ID_EX.imm << 2;
				CURRENT_STATE.PC = (ID_EX.PC
						+ ((ID_EX.imm & 0x8000) > 0 ?
								(ID_EX.imm | 0xFFFF0000) :
								(ID_EX.imm & 0x0000FFFF)) - 4);
				branch = 1;
			}
			break;
		case 0x07: //BGTZ, Jump, branch instruction
			if (ID_EX.A > 0) {
				ID_EX.imm = ID_EX.imm << 2;
				CURRENT_STATE.PC = (ID_EX.PC
						+ ((ID_EX.imm & 0x8000) > 0 ?
								(ID_EX.imm | 0xFFFF0000) :
								(ID_EX.imm & 0x0000FFFF)) - 4);
				branch = 1;
			}
			break;
		case 0x08: //ADDI, ALU Instruction
			EX_MEM.ALUOutput =
					ID_EX.A
							+ ((ID_EX.imm & 0x8000) > 0 ?
									(ID_EX.imm | 0xFFFF0000) :
									(ID_EX.imm & 0x0000FFFF));
			break;
		case 0x09: //ADDIU, ALU Instruction
			EX_MEM.ALUOutput =
					ID_EX.A
							+ ((ID_EX.imm & 0x8000) > 0 ?
									(ID_EX.imm | 0xFFFF0000) :
									(ID_EX.imm & 0x0000FFFF));
			break;
		case 0x0A: //SLTI, ALU Instruction
			if (((int32_t) ID_EX.A
					- (int32_t) (
							(ID_EX.imm & 0x8000) > 0 ?
									(ID_EX.imm | 0xFFFF0000) :
									(ID_EX.imm & 0x0000FFFF))) < 0) {
				EX_MEM.ALUOutput = 0x1;
			} else {
				EX_MEM.ALUOutput = 0x0;
			}
			break;
		case 0x0C: //ANDI, ALU Instruction
			EX_MEM.ALUOutput = ID_EX.A & (ID_EX.imm & 0x0000FFFF);
			break;
		case 0x0D: //ORI, ALU Instruction
			EX_MEM.ALUOutput = ID_EX.A | (ID_EX.imm & 0x0000FFFF);
			break;
		case 0x0E: //XORI, ALU Instruction
			EX_MEM.ALUOutput = ID_EX.A ^ (ID_EX.imm & 0x0000FFFF);
			break;
		case 0x0F: //LUI, Load/Store Instruction
			EX_MEM.ALUOutput = ID_EX.imm << 16;
			break;
		case 0x20: //LB, Load/Store Instruction
			EX_MEM.ALUOutput = ID_EX.A + ID_EX.imm;
			EX_MEM.B = ID_EX.B;
			break;
		case 0x21: //LH, Load/Store Instruction
			EX_MEM.ALUOutput = ID_EX.A + ID_EX.imm;
			EX_MEM.B = ID_EX.B;
			break;
		case 0x23: //LW, Load/Store Instruction
			EX_MEM.ALUOutput =
					ID_EX.A
							+ ((ID_EX.imm & 0x8000) > 0 ?
									(ID_EX.imm | 0xFFFF0000) :
									(ID_EX.imm & 0x0000FFFF));
			//EX_MEM.B = ID_EX.B;
			break;
		case 0x28: //SB, Load/Store Instruction
			EX_MEM.ALUOutput = ID_EX.A + ID_EX.imm;
			EX_MEM.B = ID_EX.B;
			break;
		case 0x29: //SH, Load/Store Instruction
			EX_MEM.ALUOutput = ID_EX.A + ID_EX.imm;
			EX_MEM.B = ID_EX.B;
			break;
		case 0x2B: //SW, Load/Store Instruction
			EX_MEM.ALUOutput =
					ID_EX.A
							+ ((ID_EX.imm & 0x8000) > 0 ?
									(ID_EX.imm | 0xFFFF0000) :
									(ID_EX.imm & 0x0000FFFF));
			EX_MEM.B = ID_EX.B;
			EX_MEM_RegisterRt = 0;
			break;
		default:
			// put more things here
			printf("Opcode instruction at 0x%x is not implemented!\n", opcode);
			break;
		}
	}
}

/************************************************************/
/* instruction decode (ID) pipeline stage:                                                         */
/************************************************************/
void ID()
{
    ID_EX = passRegs(IF_ID);
    /*IMPLEMENT THIS*/
    //decode here
    if (stall != 0) {
		return;
	}
	if (branch == 1) {
		branch = 0;
		ID_EX.IR = 0;
		return;
	}
	/*IMPLEMENT THIS*/
	ID_EX.PC = IF_ID.PC;
	//ID_EX.IR = IF_ID.IR;
	uint32_t rs;
	uint32_t rt;
	uint32_t immediate;

	rs = (IF_ID.IR & 0x03E00000) >> 21;
	rt = (IF_ID.IR & 0x001F0000) >> 16;
	immediate = IF_ID.IR & 0x0000FFFF; // use bit mask
	ID_EX_rs = rs;
	ID_EX_rt = rt;

	// to forward from EX stage
	if ((EX_MEM_RegWrite && (EX_MEM_RegisterRd != 0))
			&& (EX_MEM_RegisterRd == ID_EX_rs)) {
		if (ENABLE_FORWARDING == 1) {
			forwardA = 0x10;
		} else {
			stall = 3;
		}
	}
	if ((EX_MEM_RegWrite && (EX_MEM_RegisterRd != 0))
			&& (EX_MEM_RegisterRd == ID_EX_rt)) {
		if (ENABLE_FORWARDING == 1) {
			forwardB = 0x10;
		} else {
			stall = 3;
		}
	}
	if ((EX_MEM_RegWrite && (EX_MEM_RegisterRt != 0))
			&& (EX_MEM_RegisterRt == ID_EX_rs)) {
		if (ENABLE_FORWARDING == 1) {
			forwardA = 0x10;
		} else {
			stall = 3;
		}
	}
	if ((EX_MEM_RegWrite && (EX_MEM_RegisterRt != 0))
			&& (EX_MEM_RegisterRt == ID_EX_rt)) {
		if (ENABLE_FORWARDING == 1) {
			forwardB = 0x10;
		} else {
			stall = 3;
		}
	}

	// to forward form MEM stage
	if ((MEM_WB_RegWrite && (MEM_WB_RegisterRt != 0))
			&& (MEM_WB_RegisterRt == ID_EX_rs)) {
		if (ENABLE_FORWARDING == 1) {
			forwardA = 0x01;
		} else {
			stall = 2;
		}
	}
	if ((MEM_WB_RegWrite && (MEM_WB_RegisterRt != 0))
			&& (MEM_WB_RegisterRt == ID_EX_rt)) {
		if (ENABLE_FORWARDING == 1) {
			forwardB = 0x01;
		} else {
			stall = 2;
		}
	}
	if ((MEM_WB_RegWrite && (MEM_WB_RegisterRd != 0))
			&& (MEM_WB_RegisterRd == ID_EX_rs)) {
		if (ENABLE_FORWARDING == 1) {
			forwardA = 0x01;
		} else {
			stall = 2;
		}
	}
	if ((MEM_WB_RegWrite && (MEM_WB_RegisterRd != 0))
			&& (MEM_WB_RegisterRd == ID_EX_rt)) {
		if (ENABLE_FORWARDING == 1) {
			forwardB = 0x01;
		} else {
			stall = 2;
		}
	}

	ID_EX.A = CURRENT_STATE.REGS[ID_EX_rs];
	ID_EX.B = CURRENT_STATE.REGS[ID_EX_rt];
	ID_EX.imm = immediate;

	int temp_OP = ((EX_MEM.IR & 0xFC000000) >> 26);
	if (temp_OP == 0x23 || temp_OP == 0x21 || temp_OP == 0x20) {
		// load case is special
		if (stall == 0) {
			stall = 2;
		}
	}
	if (stall == 0) {
		ID_EX.IR = IF_ID.IR;
	} else {
		ID_EX.IR = 0;
	}
    
    
}

/************************************************************/
/* instruction fetch (IF) pipeline stage:                                                              */
/************************************************************/
void IF()
{
    /*IMPLEMENT THIS*/
    if(stall==0){
    IF_ID.IR = mem_read_32(CURRENT_STATE.PC);
    IF_ID.PC = CURRENT_STATE.PC+4;
    NEXT_STATE.PC = CURRENT_STATE.PC+4;
    }
}


/************************************************************/
/* Initialize Memory                                                                                                    */
/************************************************************/
void initialize() {
    init_memory();
    CURRENT_STATE.PC = MEM_TEXT_BEGIN;
    NEXT_STATE = CURRENT_STATE;
    RUN_FLAG = TRUE;
}

/************************************************************/
/* Print the program loaded into memory (in MIPS assembly format)    */
/************************************************************/
void print_program(){
    /*IMPLEMENT THIS*/
    int i;
    uint32_t addr;
    
    for(i=0; i<PROGRAM_SIZE; i++){
        addr = MEM_TEXT_BEGIN + (i*4);
        printf("[0x%x]\t", addr);
        print_instruction(mem_read_32(addr));
    }
}
/************************************************************/
/* Print the instruction at given memory address (in MIPS assembly format)    */
/************************************************************/
void print_instruction(uint32_t line){
    /*IMPLEMENT THIS*/
    uint32_t op;
    uint32_t rs;
    uint32_t rt;
    uint32_t rd;
    uint32_t funct;
    uint32_t immediate;
    uint32_t target;
    uint32_t sa;
    uint32_t base;
    uint32_t offset;
    if (line == 12){
        CURRENT_STATE.REGS[2] = 0xA;
        if (CURRENT_STATE.REGS[2] == 0xA){
            RUN_FLAG = false;
            printf("SYSCALL\n");
        }
    }
    
    if ((line | 0x03FFFFFF) ==  0x03FFFFFF){
        funct = line & 0x3F;
        rs = (line & 0x03E00000) >> 21;
        rt = (line & 0x001F0000) >> 16;
        rd = (line & 0x0000F800) >> 11;
        sa = (line & 0x000007C0) >> 6;
        switch (funct) {
            case 0x20:
                //Add rd, rs, rt
                printf("ADD rd, rs, rt   ");
                printf("ADD %s, %s, %s\n", register_name[rd], register_name[rs], register_name[rt]);
                break;
                
            case 0x21:
                //AddU rd, rs, rt
                printf("ADDU rd, rs, rt   ");
                printf("ADD %s, %s, %s\n", register_name[rd], register_name[rs], register_name[rt]);
                break;
            case 0x22:
                //SUB rd, rs, rt
                printf("SUB rd, rs, rt   ");
                printf("SUB %s, %s, %s\n", register_name[rd], register_name[rs], register_name[rt]);
                break;
            case 0x23:
                //SUBU rd, rs, rt
                printf("SUBU rd, rs, rt   ");
                printf("SUBU %s, %s, %s\n", register_name[rd], register_name[rs], register_name[rt]);
                break;
            case 0x18:
                //MULT rs, rt
                printf("MULT rs, rt   ");
                printf("MULT %s, %s\n", register_name[rs], register_name[rt]);
                break;
            case 0x19:
                //MULTU rs, rt
                printf("MULTU rs, rt   ");
                printf("MULTU %s, %s\n", register_name[rs], register_name[rt]);
                break;
            case 0x1A:
                //DIV rs, rt
                printf("DIV rs, rt   ");
                printf("DIV %s, %s\n", register_name[rs], register_name[rt]);
                break;
            case 0x1B:
                //DIVU rs, rt
                printf("DIVU rs, rt   ");
                printf("DIVU %s, %s\n", register_name[rs], register_name[rt]);
                break;
            case 0x24:
                //AND rd, rs, rt
                printf("AND rd, rs, rt   ");
                printf("AND %s, %s, %s\n", register_name[rd], register_name[rs], register_name[rt]);
                break;
            case 0x25:
                // OR rd, rs, rt
                printf("OR rd, rs, rt   ");
                printf("OR %s, %s, %s\n", register_name[rd], register_name[rs], register_name[rt]);
                break;
            case 0x26:
                // XOR rd, rs, rt
                printf("XOR rd, rs, rt   ");
                printf("XOR %s, %s, %s\n", register_name[rd], register_name[rs], register_name[rt]);
                break;
            case 0x27:
                // NOR rd, rs, rt
                printf("NOR rd, rs, rt   ");
                printf("NOR %s, %s, %s\n", register_name[rd], register_name[rs], register_name[rt]);
                break;
            case 0x2A:
                //SLT rd, rs, rt
                printf("SLT rd, rs, rt   ");
                printf("SLT %s, %s, %s\n", register_name[rd], register_name[rs], register_name[rt]);
                break;
            case 0x00:
                //SLL rd, rt, sa
                printf("SLL rd, rs, sa   ");
                printf("SLL %s, %s, %x\n", register_name[rd], register_name[rs], sa);
                break;
            case 0x03:
                printf("SRA rd, rs, sa   ");
                //SRA rd, rt, sa
                printf("SRA %s, %s, %x\n", register_name[rd], register_name[rs], sa);
                break;
            case 0x02:
                //SRL rd, rt, sa
                printf("SRL rd, rs, sa   ");
                printf("SRL %s, %s, %x\n", register_name[rd], register_name[rs], sa);
                break;
            case 0x10:
                //MFHI rd
                printf("MFHI rd   ");
                printf("MFHI %s\n", register_name[rd]);
                break;
            case 0x11:
                //MTHI rs
                printf("MTHI rs   ");
                printf("MTHI %s\n", register_name[rs]);
                break;
            case 0x12:
                //MFLO rd
                printf("MFLO rd   ");
                printf("MFLO %s\n", register_name[rd]);
                break;
            case 0x13:
                //MTLO rs
                printf("MTLO rs   ");
                printf("MTLO %s\n", register_name[rs]);
                break;
            case 0x08:
                //JR rs
                printf("JR rs   ");
                printf("JR %s\n", register_name[rs]);
                break;
            case 0x09:
                //JALR rs
                //JALR rd, rs
                printf("JALR rs   ");
                printf("JALR %s\n", register_name[rs]);
                printf("JALR rd, rs   ");
                printf("JALR %s, %s\n", register_name[rd], register_name[rs]);
                break;
            default:
                break;
        }
    } else {
        op = line & 0xFC000000;
        rs = (line & 0x03E00000) >> 21;
        rt = (line & 0x001F0000) >> 16;
        rd = (line & 0x0000F800) >> 11;
        immediate = line & 0x0000FFFF;
        base = (line & 0x03E00000) >> 21;
        offset = line & 0x0000FFFF;
        switch (op) {
            case 0x20000000:
                //ADDI rt, rs, immediate
                immediate = sign_extension_32(immediate);
                printf("ADDI, rt, rs, immediate   ");
                printf("ADDI %s, %s, %x\n", register_name[rt], register_name[rs], immediate);
                break;
            case 0x24000000:
                //ADDIU rt, rs, immediate
                immediate = sign_extension_32(immediate);
                printf("ADDIU, rt, rs, immediate   ");
                printf("ADDIU %s, %s, %x\n", register_name[rt], register_name[rs], immediate);
                break;
            case 0x30000000:
                //ANDI rt, rs, immediate
                immediate = line & 0x00000FFF;
                printf("ANDI, rt, rs, immediate   ");
                printf("ANDI %s, %s, %x\n", register_name[rt], register_name[rs], immediate);
                break;
            case 0x34000000:
                //ORI rt, rs, immediate
                immediate = line & 0x00000FFF;
                printf("ORI, rt, rs, immediate   ");
                printf("ORI %s, %s, %x\n", register_name[rt], register_name[rs], immediate);
                break;
            case 0x38000000:
                //XORI rt, rs, immediate
                printf("XORI, rt, rs, immediate   ");
                printf("XORI %s, %s, %x\n", register_name[rt], register_name[rs], immediate);
                break;
            case 0x28000000:
                //SLTI rt, rs, immediate
                immediate = immediate << 16;
                immediate = sign_extension_32(immediate);
                printf("SLTI, rt, rs, immediate   ");
                printf("SLTI %s, %s, %x\n", register_name[rt], register_name[rs], immediate);
                break;
            case 0x8C000000:
                //LW rt, offset(base)
                offset = sign_extension_32(offset);
                printf("LW, rt, offset(base)   ");
                printf("LW %s, %x(%s)\n", register_name[rt], offset, register_name[base]);
                break;
            case 0x80000000:
                //LB rt, offset(base)
                offset = sign_extension_32(offset);
                printf("LB, rt, offset(base)   ");
                printf("LB %s, %x(%s)\n", register_name[rt], offset, register_name[base]);
                break;
            case 0x81000000:
                //LH rt, offset(base)
                offset = sign_extension_32(offset);
                printf("LH, rt, offset(base)   ");
                printf("LH %s, %x(%s)\n", register_name[rt], offset, register_name[base]);
                break;
                
            case 0x3C000000:
                //LUI rt, immediate
                immediate |= 0x00000000;
                printf("LUI rt, immediate   ");
                printf("LUI %s, %x\n", register_name[rt], immediate);
                break;
            case 0xAC000000:
                //SW rt, offset(base)
                offset = sign_extension_32(offset);
                printf("SW, rt, offset(base)   ");
                printf("SW %s, %x(%s)\n", register_name[rt], offset, register_name[base]);
                break;
            case 0xA0000000:
                //SB rt, offset(base)
                offset = sign_extension_32(offset);
                printf("SB, rt, offset(base)   ");
                printf("SB %s, %x(%s)\n", register_name[rt], offset, register_name[base]);
                break;
            case 0xA1000000:
                //SH rt, offset(base)
                offset = sign_extension_32(offset);
                printf("SH, rt, offset(base)   ");
                printf("SH %s, %x(%s)\n", register_name[rt], offset, register_name[base]);
                break;
            case 0x10000000:
                //BEQ rs, rt, offset
                offset = sign_extension_32(offset);
                printf("BWQ rs, rt, offset   ");
                printf("BEQ %s, %s, %x\n", register_name[rs], register_name[rt], (offset<<2));
                break;
            case 0x14000000:
                //BNE rs, rt, offset
                offset = sign_extension_32(offset);
                printf("BNE rs, rt, offset   ");
                printf("BNE %s, %s, %x\n", register_name[rs], register_name[rt], (offset<<2));
                break;
            case 0x18000000:
                //BLEZ rs, offset
                offset = sign_extension_32(offset);
                printf("BLEZ rs, rt, offset   ");
                printf("BLEZ %s,%x\n", register_name[rs], (offset<<2));
                break;
            case 0x1C000000:
                //BGTZ rs, offset
                offset = sign_extension_32(offset);
                printf("BGTZ rs, rt, offset   ");
                printf("BGTZ %s, %x\n", register_name[rs], (offset<<2));
                break;
            case 0x08000000:
                //J target
                target = (line & 0x03FFFFFF) << 2;
                printf("J target   ");
                printf("J %x\n", target);
                break;
            case 0x0C000000:
                //JAL target
                target = (line & 0x03FFFFFF) << 2;
                printf("JAL target   ");
                printf("JAL %x\n", target);
                break;
            default:
                break;
        }
    }
}

/************************************************************/
/* Print the current pipeline                                                                                    */
/************************************************************/
void show_pipeline(){
    /*IMPLEMENT THIS*/
    printf("Current PC:  %x\n", CURRENT_STATE.PC);
    printf("IF/ID.IR:    %x  ", IF_ID.IR);
    print_instruction(IF_ID.IR);
    printf("IF/ID.PC:    %x\n\n", IF_ID.PC);
    
    printf("ID/EX.IR:    %x  ", ID_EX.IR);
    print_instruction(ID_EX.IR);
    printf("ID/EX.A:     %x\n", ID_EX.A);
    printf("ID/EX.B:     %x\n", ID_EX.B);
    printf("ID/EX.IMM    %x\n\n", ID_EX.imm);
    
    printf("EX/MEM.IR:   %x  ", EX_MEM.IR);
    print_instruction(EX_MEM.IR);
    printf("EX/MEM.A:    %x\n", EX_MEM.A);
    printf("EX/MEM.B:    %x\n", EX_MEM.B);
    printf("EX/MEM.ALU:  %x\n\n", EX_MEM.ALUOutput);
    
    printf("MEM/WB.IR:   %x  ", MEM_WB.IR);
    print_instruction(MEM_WB.IR);
    printf("MEM/WB.ALU:  %x\n", MEM_WB.ALUOutput);
    printf("MEM/WB.LMD:  %x\n\n", MEM_WB.LMD);
    
}
/***************************************************************/
/* Pass register                                                                                                                                  */
/***************************************************************/

CPU_Pipeline_Reg passRegs(CPU_Pipeline_Reg from){
    CPU_Pipeline_Reg to;
    to.A = from.A;
    to.B = from.B;
    to.ALUOutput = from.ALUOutput;
    to.HI = from.HI;
    to.LO = from.LO;
    to.imm = from.imm;
    to.IR = from.IR;
    to.LMD = from.IR;
    to.PC = from.PC;
    to.sa = from.sa;
    return to;
}
/***************************************************************/
/* main                                                                                                                                   */
/***************************************************************/
int main(int argc, char *argv[]) {
    printf("\n**************************\n");
    printf("Welcome to MU-MIPS SIM...\n");
    printf("**************************\n\n");
    
    if (argc < 2) {
        printf("Error: You should provide input file.\nUsage: %s <input program> \n\n",  argv[0]);
        exit(1);
    }
    
    strcpy(prog_file, argv[1]);
    initialize();
    load_program();
    help();
    while (1){
        handle_command();
    }
    return 0;
}
