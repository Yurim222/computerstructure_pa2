/**********************************************************************
 * Copyright (c) 2019-2022
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>

/*====================================================================*/
/*          ****** DO NOT MODIFY ANYTHING FROM THIS LINE ******       */

/* To avoid security error on Visual Studio */
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable : 4996)

#define MAX_NR_TOKENS	32	/* Maximum length of tokens in a command */
#define MAX_TOKEN_LEN	64	/* Maximum length of single token */
#define MAX_COMMAND	256 /* Maximum length of command string */

typedef unsigned char bool;
#define true	1
#define false	0

const char *__color_start = "[1;32;40m";
const char *__color_end = "[0m";

/**
 * memory[] emulates the memory of the machine
 */
static unsigned char memory[1 << 20] = {	/* 1MB memory at 0x0000 0000 -- 0x0100 0000 */
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	0xde, 0xad, 0xbe, 0xef, 0x00, 0x00, 0x00, 0x00,
	'h',  'e',  'l',  'l',  'o',  ' ',  'w',  'o',
	'r',  'l',  'd',  '!',  '!',  0x00, 0x00, 0x00,
	'a',  'w',  'e',  's',  'o',  'm',  'e',  ' ',
	'c',  'o',  'm',  'p',  'u',  't',  'e',  'r',
	' ',  'a',  'r',  'c',  'h',  'i',  't',  'e',
	'c',  't',  'u',  'r',  'e',  '!',  0x00, 0x00,
};

#define INITIAL_PC	0x1000	/* Initial value for PC register */
#define INITIAL_SP	0x8000	/* Initial location for stack pointer */

/**
 * Registers of the machine
 */
static unsigned int registers[32] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0x10, INITIAL_PC, 0x20, 3, 0xbadacafe, 0xcdcdcdcd, 0xffffffff, 7,
	0, 0, 0, 0, 0, INITIAL_SP, 0, 0,
};

/**
 * Names of the registers. Note that $zero is shorten to zr
 */
const char *register_names[] = {
	"zr", "at", "v0", "v1", "a0", "a1", "a2", "a3",
	"t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
	"s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
	"t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"
};

/**
 * Program counter register
 */
static unsigned int pc = INITIAL_PC;

/**
 * strmatch()
 *
 * DESCRIPTION
 *   Compare strings @str and @expect and return 1 if they are the same.
 *   You may use this function to simplify string matching :)
 *
 * RETURN
 *   1 if @str and @expect are the same
 *   0 otherwise
 */
static inline bool strmatch(char * const str, const char *expect)
{
	return (strlen(str) == strlen(expect)) && (strncmp(str, expect, strlen(expect)) == 0);
}

/*          ****** DO NOT MODIFY ANYTHING UP TO THIS LINE ******      */
/*====================================================================*/


/**********************************************************************
 * process_instruction
 *
 * DESCRIPTION
 *   Execute the machine code given through @instr. The following table lists
 *   up the instructions to support. Note that a pseudo instruction 'halt'
 *   (0xffffffff) is added for the testing purpose. Also '*' instrunctions are
 *   the ones that are newly added to PA2.
 *
 * | Name   | Format    | Opcode / opcode + funct |
 * | ------ | --------- | ----------------------- |
 * | `add`  | r-format  | 0 + 0x20                |
 * | `addi` | i-format  | 0x08                    |
 * | `sub`  | r-format  | 0 + 0x22                |
 * | `and`  | r-format  | 0 + 0x24                |
 * | `andi` | i-format  | 0x0c                    |
 * | `or`   | r-format  | 0 + 0x25                |
 * | `ori`  | i-format  | 0x0d                    |
 * | `nor`  | r-format  | 0 + 0x27                |
 * | `sll`  | r-format  | 0 + 0x00                |
 * | `srl`  | r-format  | 0 + 0x02                |
 * | `sra`  | r-format  | 0 + 0x03                |
 * | `lw`   | i-format  | 0x23                    |
 * | `sw`   | i-format  | 0x2b                    |
 * | `slt`  | r-format* | 0 + 0x2a                |
 * | `slti` | i-format* | 0x0a                    |
 * | `beq`  | i-format* | 0x04                    |
 * | `bne`  | i-format* | 0x05                    |
 * | `jr`   | r-format* | 0 + 0x08                |
 * | `j`    | j-format* | 0x02                    |
 * | `jal`  | j-format* | 0x03                    |
 * | `halt` | special*  | @instr == 0xffffffff    |
 *
 * RETURN VALUE
 *   1 if successfully processed the instruction.
 *   0 if @instr is 'halt' or unknown instructions
 */
static int process_instruction(unsigned int instr) //instr인자로 받아서 
{
	//int bit_mask = 0x0000FFFF;
	int op = instr>>26; //opcode 가져오는 part
	int bit_mask_func = 0b00000000000000000000000000111111; //function part만 가져오기위해
	int bit_mask_reg = 0b00000000000000000000000000011111; //function part만 가져오기위해
	int sign;

	if(instr == 0xffffffff) return 0; //halt
	if(op==0){
		//r-format
		int rs = (instr>>21) & bit_mask_reg;
		int rt = (instr>>16) & bit_mask_reg;
		int rd = (instr>>11) & bit_mask_reg;
		int shamt = (instr>>6)&bit_mask_reg;
		int func = instr&bit_mask_func;
		int a=0;
		int b=0;
		switch(func){
			case 0x20:
			//add
           // printf("add\n");
			registers[rd]=registers[rs]+registers[rt];
			break;

			case 0x22:
			//sub
            //printf("sub\n");
			registers[rd]=registers[rs]-registers[rt];
			break;

			case 0x24:
			//and
            //printf("and\n");
			registers[rd]=registers[rs]&registers[rt];
			break;
			
			case 0x25:
			//or
           // printf("or\n");
			registers[rd] = registers[rs]|registers[rt];
            break;
            
			case 0x27:
			//nor
           // printf("nor\n");
			registers[rd] = ~(registers[rs] | registers[rt]);
			break;

			case 0x00:
			//sll
           /// printf("sll\n");
			registers[rd] = registers[rt]<<shamt;
			break;

			case 0x02:
			//srl
            //printf("srl\n");
			registers[rd] = registers[rt]>>shamt;
			break;

			case 0x03:
			//sra
           // printf("sra\n");
			a=registers[rt]>>31;
            
			if(a==1){
			 //음수
			 int s_e=0xFFFFFFFF<<(32-shamt);
			registers[rd]=registers[rt]>>shamt;
			 registers[rd]=registers[rd]|s_e;		
			}else{
			registers[rd]=registers[rt]>>shamt;
			}		
			break;

			case 0x2a:
			//slt
           // printf("slt\n");
		   //registers[rs]랑 registers[rt]를 signed로 변환해야한다.
			if((signed)registers[rs]<(signed)registers[rt]){
				registers[rd]=1;
			}
			else registers[rd]=0;
			
			
			break;

			case 0x08:
			//jr
            //printf("jr\n");
			pc=registers[rs];
			break;
            
            default:
                return 0;
                
		}return 1;

	}else{

		int rs=(instr>>21)&bit_mask_reg; //rs
		int rt=(instr>>16)&bit_mask_reg; //rt
		int address=(instr<<6)>>6;
		int immidiate=(instr<<16)>>16;
		int source;

		int dec_sign=(instr>>15)&1;
		int sign_ex;
		if(dec_sign==0){
			//양수
			sign=0;
			sign_ex=(instr<<16)>>16;
		} else{
			//음수
			sign=1;
			sign_ex=(instr<<16)>>16;
			sign_ex=sign_ex|0xffff0000;//sign_ex 2의보수
		}
		
		switch(op){
			case 0x08:
			//addi
			registers[rt]=sign_ex+registers[rs];
			break;

			case 0x0c:
			//andi
			sign_ex=(instr<<16)>>16;//sign_ex 초기화
			registers[rt]=sign_ex&registers[rs];
			break;

			case 0x0d:
			//ori
			sign_ex=(instr<<16)>>16;//sign_ex 초기화
			registers[rt]=sign_ex|registers[rs];
			break;

			case 0x23:
			//lw
			source = registers[rs]+sign_ex;
			registers[rt] = (memory[source]<<24)+(memory[source+1]<<16)+(memory[source+2]<<8)+(memory[source+3]);
			break;

			case 0x2b:
			//sw
			source = registers[rs]+sign_ex;
			memory[source]=(registers[rt]&0xFF000000)>>24;//맨 뒷자리 8bit
			memory[source+1]=(registers[rt]&0x00FF0000)>>16;
			memory[source+2]=(registers[rt]&0x0000FF00)>>8;
			memory[source+3]=(registers[rt]&0x000000FF);
			break;

			case 0x0a:
			//slti
			if(registers[rs]<sign_ex) registers[rt]=1;
			else registers[rt]=0;
			break;

			case 0x04:
			//beq
			if(registers[rs]==registers[rt]){
				
				if((immidiate>>15)==0x00000001){
					//constant 값이 음수이면

					immidiate=(immidiate^0x0000FFFF);
					immidiate=immidiate|0x00000001;
					pc=pc-(immidiate<<2);
				}
				else pc=pc+(immidiate<<2);
			}
			break;

			case 0x05:
			//bne
			printf("%d",pc);
			if(registers[rs]!=registers[rt]){
				
				if((immidiate>>15)==0x00000001){
				//constant 값이 음수이면
					immidiate=(immidiate^0x0000FFFF);
					immidiate=immidiate|0x00000001;
					pc=pc-(immidiate<<2);
				}
				else pc=pc+(immidiate>>2);
			}
			break;

			case 0x02:
			//j
			pc=address<<2; //pc adress *4 증가
			break;

			case 0x03:
			//jal
			registers[31]=pc;
			pc=address<<2;
			break;

			 default:
                return 0;

		}return 1;

	}
	return 0;
}


/**********************************************************************
 * load_program(filename)
 *
 * DESCRIPTION
 *   Load the instructions in the file @filename onto the memory starting at
 *   @INITIAL_PC. Each line in the program file looks like;
 *
 *	 [MIPS instruction started with 0x prefix]  // optional comments
 *
 *   For example,
 *
 *   0x8c090008
 *   0xac090020	// sw t1, zero + 32
 *   0x8c080000
 *
 *   implies three MIPS instructions to load. Each machine instruction may
 *   be followed by comments like the second instruction. However you can simply
 *   call strtoimax(linebuffer, NULL, 0) to read the machine code while
 *   ignoring the comment parts.
 *
 *	 The program DOES NOT include the 'halt' instruction. Thus, make sure the
 *	 'halt' instruction is appended to the loaded instructions to terminate
 *	 your program properly.
 *
 *	 Refer to the @main() for reading data from files. (fopen, fgets, fclose).
 *
 * RETURN
 *	 0 on successfully load the program
 *	 any other value otherwise
 */

static int load_program(char * const filename)
{
	FILE *read_file=NULL;
	read_file=fopen(filename,"r");//파일 열어서 읽기
	char *end;

	int location = 0x1000;
	//char arr[256];//buffer를 위한 char
	char file[MAX_COMMAND];

	if(read_file!=NULL){
		//file이 0이 아니면
		//int flag = 1;
		//int count = 0;
		
		while(fgets(file,MAX_COMMAND,read_file)){
			//printf("file : %s\n",file);

			int instruction=strtol(file,&end,16);
			//16진법으로 표기된 문자열을 16진수로 변환
			//16진법으로 잘 변환됐나? printf
			//printf("instruction!: %x\n",instruction);

			memory[location]=(instruction>>24);
			//printf("instruction0: %x\n",instruction>>24);

			memory[location+1]=(instruction>>16);
			//printf("instrution1: %x\n",instruction>>16);

			memory[location+2]=(instruction>>8);
			//printf("instruction2: %x\n",instruction>>8);

			memory[location+3]=instruction;
			//printf("location3: %x\n",instruction);


			location+=4;
			//printf("last location: %x\n", location);
		}

		memory[location]=0xFF;
		memory[location+1]=0xFF;
		memory[location+2]=0xFF;
		memory[location+3]=0xFF;

		
	}else{	
		//file 이 0
		fprintf(stderr, "There is no file that match.\n", filename);
		return -EINVAL;
	}

	return 0;	
		
}
	


/**********************************************************************
 * run_program
 *
 * DESCRIPTION
 *   Start running the program that is loaded by @load_program function above.
 *   If you implement @load_program() properly, the first instruction is placed
 *   at @INITIAL_PC. Using @pc, which is the program counter of this processor,
 *   you can emulate the MIPS processor by
 *
 *   1. Read instruction from @pc
 *   2. Increment @pc by 4
 *   3. Call @process_instruction(instruction)
 *   4. Repeat until @process_instruction() returns 0
 *
 * RETURN
 *   0
 */
static int run_program(void)
{
	//초기 pc값 설정
	pc=INITIAL_PC;
	//printf("%x",pc);
	int instruction;
	int flag=1;

	while(flag!=0)
	{
		if((memory[pc]==0xFF)&&(memory[pc+1]==0xFF)&&(memory[pc+2]==0xFF)&&(memory[pc+3]==0xFF)){
			pc+=4;
			flag=0;	
		}
		else{
			instruction=(memory[pc]<<24)+(memory[pc+1]<<16)+(memory[pc+2]<<8)+(memory[pc+3]);
			//printf("instruction1: %x\n",instruction);
			pc+=4;
			flag=process_instruction(instruction);
			printf("pc:%x\n",pc);

			printf("registers[t0]: %x\n",registers[8]);
			printf("registers[t1]: %x\n",registers[9]);

			printf("registers[a0]: %x\n",registers[4]);
			printf("registers[a1]: %x\n",registers[5]);

		}
	}
	return 0;
}


/*====================================================================*/
/*          ****** DO NOT MODIFY ANYTHING FROM THIS LINE ******       */
static void __show_registers(char * const register_name)
{
	int from = 0, to = 0;
	bool include_pc = false;

	if (strmatch(register_name, "all")) {
		from = 0;
		to = 32;
		include_pc = true;
	} else if (strmatch(register_name, "pc")) {
		include_pc = true;
	} else {
		for (int i = 0; i < sizeof(register_names) / sizeof(*register_names); i++) {
			if (strmatch(register_name, register_names[i])) {
				from = i;
				to = i + 1;
			}
		}
	}

	for (int i = from; i < to; i++) {
		fprintf(stderr, "[%02d:%2s] 0x%08x    %u\n", i, register_names[i], registers[i], registers[i]);
	}
	if (include_pc) {
		fprintf(stderr, "[  pc ] 0x%08x\n", pc);
	}
}

static void __dump_memory(unsigned int addr, size_t length)
{
    for (size_t i = 0; i < length; i += 4) {
        fprintf(stderr, "0x%08lx:  %02x %02x %02x %02x    %c %c %c %c\n",
				addr + i,
                memory[addr + i    ], memory[addr + i + 1],
                memory[addr + i + 2], memory[addr + i + 3],
                isprint(memory[addr + i    ]) ? memory[addr + i    ] : '.',
				isprint(memory[addr + i + 1]) ? memory[addr + i + 1] : '.',
				isprint(memory[addr + i + 2]) ? memory[addr + i + 2] : '.',
				isprint(memory[addr + i + 3]) ? memory[addr + i + 3] : '.');
    }
}

static void __process_command(int argc, char *argv[])
{
	if (argc == 0) return;

	if (strmatch(argv[0], "load")) {
		if (argc == 2) {
			load_program(argv[1]);
		} else {
			printf("Usage: load [program filename]\n");
		}
	} else if (strmatch(argv[0], "run")) {
		if (argc == 1) {
			run_program();
		} else {
			printf("Usage: run\n");
		}
	} else if (strmatch(argv[0], "show")) {
		if (argc == 1) {
			__show_registers("all");
		} else if (argc == 2) {
			__show_registers(argv[1]);
		} else {
			printf("Usage: show { [register name] }\n");
		}
	} else if (strmatch(argv[0], "dump")) {
		if (argc == 3) {
			__dump_memory(strtoimax(argv[1], NULL, 0), strtoimax(argv[2], NULL, 0));
		} else {
			printf("Usage: dump [start address] [length]\n");
		}
	} else {
#ifdef INPUT_ASSEMBLY
		/**
		 * You may hook up @translate() from pa1 here to allow assembly code input!
		 */
		unsigned int instr = translate(argc, argv);
		process_instruction(instr);
#else
		process_instruction(strtoimax(argv[0], NULL, 0));
#endif
	}
}

static int __parse_command(char *command, int *nr_tokens, char *tokens[])
{
	char *curr = command;
	int token_started = false;
	*nr_tokens = 0;

	while (*curr != '\0') {
		if (isspace(*curr)) {
			*curr = '\0';
			token_started = false;
		} else {
			if (!token_started) {
				tokens[*nr_tokens] = curr;
				*nr_tokens += 1;
				token_started = true;
			}
		}
		curr++;
	}

	/* Exclude comments from tokens */
	for (int i = 0; i < *nr_tokens; i++) {
		if (strmatch(tokens[i], "//") || strmatch(tokens[i], "#")) {
			*nr_tokens = i;
			tokens[i] = NULL;
		}
	}

	return 0;
}

int main(int argc, char * const argv[])
{
	char command[MAX_COMMAND] = {'\0'};
	FILE *input = stdin;

	if (argc > 1) {
		input = fopen(argv[1], "r");
		if (!input) {
			fprintf(stderr, "No input file %s\n", argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (input == stdin) {
		printf("%s", __color_start);
		printf("*****************************************************\n");
		printf(" Welcome to SCE212 MIPS Termlink v0.2\n");
		printf("\n");
		printf(" SCE212 Model 2022-S is the most reliable client\n");
		printf(" terminal ever developed to run MIPS programs in\n");
		printf(" Vault 212.\n");
		printf("\n");
		printf("- VALID :   May 20 (Fri)\n");
		printf("- MANUAL:   https://git.ajou.ac.kr/sslab/ca-pa2.git\n");
		printf("- SUBMIT:   https://sslab.ajou.ac.kr/pasubmit\n");
		printf("\n");
		printf("- QNA AT AJOUBB ENCOURAGED!!\n");
		printf("\n");
		printf("\n");
		printf("\n");
		printf(">> ");
		printf("%s", __color_end);
	}

	while (fgets(command, sizeof(command), input)) {
		char *tokens[MAX_NR_TOKENS] = { NULL };
		int nr_tokens = 0;

		for (size_t i = 0; i < strlen(command); i++) {
			command[i] = tolower(command[i]);
		}

		if (__parse_command(command, &nr_tokens, tokens) < 0)
			continue;

		__process_command(nr_tokens, tokens);

		if (input == stdin) printf("%s>> %s", __color_start, __color_end);
	}

	if (input != stdin) fclose(input);

	return EXIT_SUCCESS;
}