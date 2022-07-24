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
#include <string.h>
#include <inttypes.h>
#include <ctype.h>

/*====================================================================*/
/*          ****** DO NOT MODIFY ANYTHING FROM THIS LINE ******       */
/* To avoid security error on Visual Studio */
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable : 4996)

typedef unsigned char bool;
#define true  1
#define false 0

enum cache_simulator_constants {
	CACHE_HIT = 0,
	CACHE_MISS,

	CB_INVALID = 0,	/* Cache block is invalid */
	CB_VALID = 1,	/* Cache block is valid */

	CB_CLEAN = 0,	/* Cache block is clean */
	CB_DIRTY = 1,	/* Cache block is dirty */

	BYTES_PER_WORD = 4,	/* This is 32 bit machine (1 word is 4 bytes) */
	MAX_NR_WORDS_PER_BLOCK = 32,	/* Maximum cache block size */
};


/* 8 KB Main memory */
static unsigned char memory[8 << 10] = {
	0xde, 0xad, 0xbe, 0xef, 0xba, 0xda, 0xca, 0xfe,
	0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
	'h',  'e',  'l',  'l',  'o',  ' ' , 'w' , 'o',
	'r',  'l',  'd',  '!',  0x89, 0xab, 0xcd, 0xef,
	0x50, 0x52, 0x54, 0x56, 0x58, 0x5a, 0x5c, 0x5e,
	0x60, 0x62, 0x64, 0x66, 0x68, 0x6a, 0x6c, 0x6e,
	0x70, 0x72, 0x74, 0x76, 0x78, 0x7a, 0x7c, 0x7e,
	0x80, 0x82, 0x84, 0x86, 0x88, 0x8a, 0x8c, 0x8e,
};

/* Cache block */
struct cache_block {
	bool valid;				/* Whether the block is valid or invalid.
							   Use CB_INVALID or CB_VALID macro above  */
	bool dirty;				/* Whether the block is updated or not.
							   Use CB_CLEAN or CB_DIRTY defined above */
	unsigned int tag;		/* Tag */
	unsigned int timestamp;	/* Timestamp or clock cycles to implement LRU */
	unsigned char data[BYTES_PER_WORD * MAX_NR_WORDS_PER_BLOCK];
							/* Each block can hold multiple words */
};

/* An 1-D array for cache blocks. */
static struct cache_block *cache = NULL;

/* The size of cache block. The value is set during the initialization */
static int nr_words_per_block = 4;

/* Number of cache blocks. The value is set during the initialization */
static int nr_blocks = 16;

/* Number of ways for the cache. Note @nr_ways == 1 means the direct mapped
 * cache and @nr_ways == nr_blocks implies the fully associative cache */
static int nr_ways = 2;

/* Number of @nr_ways-way sets in the cache. This value will be set according to
 * @nr_blocks and @nr_ways values */
static int nr_sets = 8;

/* Clock cycles */
const int cycles_hit = 1;
const int cycles_miss = 100;

/* Elapsed clock cycles so far */
static unsigned int cycles = 0;


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

/**
 * log2_discrete
 *
 * DESCRIPTION
 *   Return the integer part of log_2(@n). FREE TO USE IN YOUR IMPLEMENTATION.
 *   Will be useful for calculating the length of tag for a given address.
 */
static int log2_discrete(int n)
{
	int result = -1;
	do {
		n = n >> 1;
		result++;
	} while (n);

	return result;
}
/*          ****** DO NOT MODIFY ANYTHING UP TO THIS LINE ******      */
/*====================================================================*/


/**************************************************************************
 * load_word(addr)
 *
 * DESCRIPTION
 *   Simulate the case when the processor is handling a lw instruction for @addr.
 *   To that end, you should first look up the cache blocks to find the block
 *   containing the target address @addr. If exists, it's cache hit; return
 *   CACHE_HIT after updating the cache block's timestamp with @cycles.
 *   Otherwise, replace the LRU cache block in the set. Should handle dirty
 *   blocks properly according to the write-back semantic.
 *
 * PARAMETERS
 *   @addr: Target address to load
 *
 * RETURN
 *   CACHE_HIT on cache hit, CACHE_MISS otherwise
 *
 */

int load_word(unsigned int addr)
{
	
	int Bytes_per_block=nr_words_per_block*4;

	int offset_bit=log2_discrete(Bytes_per_block); //offset bit 수
	printf("offset_bit: %d\n", offset_bit);

	int index_bit=log2_discrete(nr_sets); //cache index 비트 수
	printf("index_bit: %d\n", index_bit);
	
	int tag_bit=32-(index_bit+offset_bit); //tag bit 수
	printf("tag_bit: %d\n", tag_bit);

	int index_bitmask=(1<<index_bit)-1; 
	int index=(addr>>offset_bit)&(index_bitmask);
	printf("index: %d\n", index);

	int tag=addr>>(index_bit+offset_bit);
	printf("tag: %d\n", tag);

	int offset=(addr<<(tag_bit+index_bit))>>(tag_bit+index_bit);
	printf("offset: %d\n", offset);

	int cache_loc = index*nr_ways;
	int block_addr = (addr>>offset_bit)<<offset_bit;
	printf("block addr: %d\n", block_addr);

	int last_clock=0xffffffff;
	int empty_check=0;

	for(int i=0; i<nr_ways; i++){
		empty_check++;

		if((cache[cache_loc+i].valid==1)){
			//valid==1일 때
			printf("valid=1\n");
			if(cache[cache_loc+i].tag== tag){
				printf("tag 같음 Hit!\n");
				//hit 일때(valid가 1인데다가 tag까지 같은 경우)
				cache[cache_loc+i].timestamp=cycles;
				cache[cache_loc+i].tag=tag;
				cache[cache_loc+i].valid=CB_VALID;
				return CACHE_HIT;
			}
			else if(empty_check==nr_ways){
				//valid는 1이지만 tag가 같지 않고 꽉 차있음.(miss일때)
				//time stamp 가장 작은것을 선택해서 write back
				//printf("공간없음\n");
				//time stamp가 가장 작은 것을 선택하는 과정
				
				int least_time_clock;
				int LRU_block_addr = 0;
				//LRU
				for(int j=0; j<nr_ways; j++){
					if(cache[cache_loc+j].timestamp<last_clock){
						last_clock=cache[cache_loc+j].timestamp;
						LRU_block_addr = cache_loc+j; // 젤 작은 애 발견함. => 얘 내쫓아야 함.
					}			
				}
				//내쫓아야하는 애에 dirty가 붙어있으면
				if(cache[LRU_block_addr].dirty==CB_DIRTY){
					int mem_addr= (cache[LRU_block_addr].tag<<(index_bit+offset_bit))|(index<<offset_bit);
					for(int j=0; j<Bytes_per_block; j++){
						//내쫓을 애
						memory[mem_addr+j]=cache[LRU_block_addr].data[j];
					}
					cache[LRU_block_addr].dirty=CB_INVALID; //valid없애주고
				}
				cache[LRU_block_addr].tag=tag;
				cache[LRU_block_addr].timestamp=cycles;

				for(int j=0; j<Bytes_per_block; j++){
					cache[LRU_block_addr].data[j]=memory[block_addr+j];
				}

				return CACHE_MISS;
			}
			
		}
		else{
			//valid==0, 
			//비워져있을 때. (miss) 그냥 채워주기만 하면됨
			printf("valid=0, nothing inside\n");
			
			for(int j=0; j<Bytes_per_block; j++){
				cache[cache_loc+i].data[j]=memory[block_addr+j];
			}
			cache[cache_loc+i].timestamp=cycles;
			cache[cache_loc+i].tag=tag;
			cache[cache_loc+i].valid=CB_VALID;
			return CACHE_MISS;
		}

	}
}

/**************************************************************************
 * store_word(addr, data)
 *
 * DESCRIPTION
 *   Simulate the case when the processor is handling the 'sw' instruction.
 *   Cache should be write-back and write-allocate. Note that the least
 *   recently used (LRU) block should be replaced in case of eviction.
 *
 * PARAMETERS
 *   @addr: Starting address for @data
 *   @data: New value for @addr. Assume that @data is 1-word in size
 *
 * RETURN
 *   CACHE_HIT on cache hit, CACHE_MISS otherwise
 *
 */
int store_word(unsigned int addr, unsigned int data)
{
	int Bytes_per_block=nr_words_per_block*4;

	int offset_bit=log2_discrete(Bytes_per_block); //offset bit 수
	printf("offset_bit: %d\n", offset_bit);

	int index_bit=log2_discrete(nr_sets); //cache index 비트 수
	printf("index_bit: %d\n", index_bit);
	
	int tag_bit=32-(index_bit+offset_bit); //tag bit 수
	printf("tag_bit: %d\n", tag_bit);

	int index_bitmask=(1<<index_bit)-1; 
	int index=(addr>>offset_bit)&(index_bitmask);
	printf("index: %d\n", index);

	int tag=addr>>(index_bit+offset_bit);
	printf("tag: %d\n", tag);

	int offset=(addr<<(tag_bit+index_bit))>>(tag_bit+index_bit);
	printf("offset: %d\n", offset);

	int cache_loc = index*nr_ways;
	int block_addr = (addr>>offset_bit)<<offset_bit;
	printf("block addr: %d\n", block_addr);

	int last_clock=0xffffffff;
	int empty_check;

	int to_write[4];
	to_write[0]=(data&0xFF000000)>>24;
	printf("to_write[0]: %d\n",to_write[0]);
	to_write[1]=(data&0x00FF0000)>>16;
	printf("to_write[1]: %d\n",to_write[1]);
	to_write[2]=(data&0x0000FF00)>>8;
	printf("to_write[2]: %d\n",to_write[2]);
	to_write[3]=(data&0x000000FF)>>0;
	printf("to_write[3]: %d\n",to_write[3]);

	for(int i=0; i<nr_ways;i++){
		empty_check++;

		if(cache[cache_loc+i].valid==1){
			//valid=1하고
			if(cache[cache_loc+i].tag==tag){
				//cache가 hit하는 경우
				printf("store hit!\n");
				cache[cache_loc+i].timestamp=cycles;
				cache[cache_loc+i].dirty=CB_DIRTY;
				//값을 다시 써줘야 함.
				for(int j=0; j<BYTES_PER_WORD; j++){
					cache[cache_loc+i].data[offset+j]=to_write[j];
				}
				return CACHE_HIT;
			}
			else if(empty_check==nr_ways){
				//valid 1이고 값들이 꽉 차 있을 때
				printf("valid is 1, but full\n");
				int least_time_clock = cache[cache_loc].timestamp;
				//int LRU = 0xffffffff;
				int LRU_block_addr;

				//LRU
				for(int j=0; j<nr_ways; j++){
					if(cache[cache_loc+j].timestamp<last_clock){
						last_clock=cache[cache_loc+j].timestamp;
						LRU_block_addr = cache_loc+j; // 젤 작은 애 발견함.
						printf("j의 값: %d", j);
					}			
				}
				int mem_addr= ((cache[LRU_block_addr].tag<<index_bit)|index)<<offset_bit;

				//젤 작은애가 dirty가 붙어있으면
				if(cache[LRU_block_addr].dirty==CB_DIRTY){
					for(int j=0; j<Bytes_per_block; j++){
						//메모리에 젤 작은 애의 값 update 해줘야 함.
						memory[mem_addr+j]=cache[LRU_block_addr].data[j];
					}
					//cache[LRU_block_addr].dirty=CB_VALID; //dirty없애주고
				}
				cache[LRU_block_addr].tag=tag;
				cache[LRU_block_addr].timestamp=cycles;
				cache[LRU_block_addr].dirty=1;
				//쫓아내버린 자리에 저장하려는 값 저장해야 함.
				/*for(int j=0; j<Bytes_per_block; j++){
					cache[LRU_block_addr].data[j]=memory[block_addr+j];
				}*/
				for(int j=0; j<Bytes_per_block; j++){
					cache[LRU_block_addr].data[j]=memory[block_addr+j];
				}

				for(int j=0; j<BYTES_PER_WORD; j++){
					//쫓아내버린 cache에 값을 다시 써줘야 함.
					cache[LRU_block_addr].data[offset+j]=to_write[j];
				}

				return CACHE_MISS;
			}			
		}
		else{
			//valid=0, 아무런 값도 들어있지 않음..
			//비워져 있으므로 채워주기만 하면 됨
			printf("empty\n");

			for(int j=0; j<Bytes_per_block; j++){
				//값이 바껴야하는데!

				cache[cache_loc+i].data[j]=memory[block_addr+j];
			}
			for(int j=0; j<BYTES_PER_WORD; j++){
					cache[cache_loc+i].data[offset+j]=to_write[j];
			}
			
			cache[cache_loc+i].tag=tag;
			cache[cache_loc+i].valid=CB_VALID;
			cache[cache_loc+i].dirty=CB_DIRTY;
			cache[cache_loc+i].timestamp=cycles;
			return CACHE_MISS;

		}
			
		
	}
}


/**************************************************************************
 * init_simulator
 *
 * DESCRIPTION
 *   This function is called before starting the simulation. This is the
 *   perfect place to put your initialization code. You may leave this function
 *   empty if you'd like.
 */
void init_simulator(void)
{
	/* TODO: You may place your initialization code here */


}



/*====================================================================*/
/*          ****** DO NOT MODIFY ANYTHING FROM THIS LINE ******       */
static void __show_cache(void)
{
	for (int i = 0; i < nr_blocks; i++) {
		fprintf(stderr, "[%3d] %c%c %8x %8u | ", i,
				cache[i].valid == CB_VALID ? 'v' : ' ',
				cache[i].dirty == CB_DIRTY ? 'd' : ' ',
				cache[i].tag, cache[i].timestamp);
		for (int j = 0; j < BYTES_PER_WORD * nr_words_per_block; j++) {
			fprintf(stderr, "%02x", cache[i].data[j]);
			if ((j + 1) % 4 == 0) fprintf(stderr, " ");
		}
		fprintf(stderr, "\n");
		if (nr_ways > 1 && ((i + 1) % nr_ways == 0)) printf("\n");
	}
}

static void __dump_memory(unsigned int start)
{
	for (int i = start; i < start + 64; i++) {
		if (i % 16 == 0) {
			fprintf(stderr, "[0x%08x] ", i);
		}
		fprintf(stderr, "%02x", memory[i]);
		if ((i + 1) % 4 == 0) fprintf(stderr, " ");
		if ((i + 1) % 16 == 0) fprintf(stderr, "\n");
	}
}

static void __init_cache(void)
{
	cache = malloc(sizeof(struct cache_block) * nr_blocks);

	for (int i = 0; i < nr_blocks; i++) {
		cache[i] = (struct cache_block) {
			.valid = CB_INVALID,
			.dirty = CB_CLEAN,
			.tag = 0,
			.timestamp = 0,
			.data = { 0x00 },
		};
	}
}

static void __fini_cache(void)
{
	free(cache);
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

/* scanf does not consume a newline character and feeds an empty line
 * to fgets in the very first loop. It is not a big deal but wanna
 * hide that. There are many cleaner ways to do this, but skipping
 * to the prompt in the first loop will do in the PA...
 */
static bool is_first = true;

static void __simulate_cache(FILE *input)
{
	int argc;
	char *argv[10];
	char command[80];

	unsigned int hits = 0, misses = 0;

	__init_cache();

	while (true) {
		char *line;
		unsigned int addr, value;
		int hit;
			
		if (input == stdin && !is_first) printf(">> ");
		is_first = false;

		if (!fgets(command, sizeof(command), input)) break;

		__parse_command(command, &argc, argv);

		if (argc == 0) continue;

		if (strmatch(argv[0], "quit")) break;

		if (strmatch(argv[0], "show")) {
			__show_cache();
			continue;
		} else if (strmatch(argv[0], "dump")) {
			addr = argc == 1 ? 0 : strtoimax(argv[1], NULL, 0) & 0xfffffffc;
			__dump_memory(addr);
			continue;
		} else if (strmatch(argv[0], "cycles")) {
			fprintf(stderr, "%3u %3u   %u\n", hits, misses, cycles);
			continue;
		} else if (strmatch(argv[0], "lw")) {
			if (argc == 1) {
				printf("Wrong input for lw\n");
				printf("Usage: lw <address to load>\n");
				continue;
			}
			addr = strtoimax(argv[1], NULL, 0);
			hit = load_word(addr);
		} else if (strmatch(argv[0], "sw")) {
			if (argc != 3) {
				printf("Wrong input for sw\n");
				printf("Usage: sw <address to store> <word-size value to store>\n");
				continue;
			}
			addr = strtoimax(argv[1], NULL, 0);
			value = strtoimax(argv[2], NULL, 0);
			hit = store_word(addr, value);
		} else if (strmatch(argv[0], "help")) {
			printf("- show         : Show cache\n");
			printf("- dump [addr]  : Dump memory from @addr to @addr+64\n");
			printf("- cycles       : Show elapsed cycles\n");
			printf("\n");
			printf("- lw <addr>    : Simulate loading a word at @addr\n");
			printf("- sw <addr> <value>\n");
			printf("               : Simulate storing @value at @addr\n");
			printf("\n");
		} else {
			continue;
		}

		if (hit == CACHE_HIT) {
			hits++;
			cycles += cycles_hit;
		} else {
			misses++;
			cycles += cycles_miss;
		}
	}

	__fini_cache();
}

int main(int argc, const char *argv[])
{
	FILE *input = stdin;

	if (argc > 1) {
		input = fopen(argv[1], "r");
		if (!input) {
			perror("Input file error");
			return EXIT_FAILURE;
		}
	}


	if (input == stdin) {
		printf("*****************************************************\n");
		printf("*                    _                              *\n");
		printf("*      ___ __ _  ___| |__   ___                     *\n");
		printf("*     / __/ _` |/ __| '_ \\ / _ \\                    *\n");
		printf("*    | (_| (_| | (__| | | |  __/                    *\n");
		printf("*     \\___\\__,_|\\___|_| |_|\\___|                    *\n");
		printf("*    	  _                 _       _               *\n");
		printf("*     ___(_)_ __ ___  _   _| | __ _| |_ ___  _ __   *\n");
		printf("*    / __| | '_ ` _ \\| | | | |/ _` | __/ _ \\| '__|  *\n");
		printf("*    \\__ \\ | | | | | | |_| | | (_| | || (_) | |     *\n");
		printf("*    |___/_|_| |_| |_|\\__,_|_|\\__,_|\\__\\___/|_|     *\n");
		printf("*                                                   *\n");
		printf("*                                   2022.06         *\n");
		printf("*****************************************************\n\n");
	}

#ifndef _USE_DEFAULT
	if (input == stdin) printf("- words per block:  ");
	fscanf(input, "%d", &nr_words_per_block);
	if (input == stdin) printf("- number of blocks: ");
	fscanf(input, "%d", &nr_blocks);
	if (input == stdin) printf("- number of ways:   ");
	fscanf(input, "%d", &nr_ways);

	nr_sets = nr_blocks / nr_ways;
#endif

	init_simulator();
	__simulate_cache(input);

	if (input != stdin) fclose(input);

	return EXIT_SUCCESS;
}
