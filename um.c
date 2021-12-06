
/**************************************************************
 *                     um.c
 * 
 *     Assignment: Homework 6 - Universal Machine Program
 *     Authors: Katie Yang (zyang11) and Pamela Melgar (pmelga01)
 *     Date: November 24, 2021
 *
 *     Purpose: This C file will hold the main driver for our Universal
 *              MachineProgram (HW6). 
 *     
 *     Success Output:
 *              
 *     Failure output:
 *              1. 
 *              2. 
 *                  
 **************************************************************/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "assert.h"
#include "uarray.h"
#include "seq.h"
#include "bitpack.h"
#include <sys/stat.h>

#define BYTESIZE 8


/* Instruction retrieval */
typedef enum Um_opcode {
        CMOV = 0, SLOAD, SSTORE, ADD, MUL, DIV,
        NAND, HALT, ACTIVATE, INACTIVATE, OUT, IN, LOADP, LV
} Um_opcode;


struct Info
{
    // uint32_t op;
    uint32_t rA;
    uint32_t rB;
    uint32_t rC;
    uint32_t value;
};


typedef struct Info Info;
///////////////////////////////////////////////////////////////////////////////////////////

struct Array_T
{
    uint32_t *array;
    uint32_t size;
};

typedef struct Array_T Array;


// struct Memory
// {
//     Array *segments;
//     Seq_T map_queue;
// };

// typedef struct Memory Memory;



#define two_pow_32 4294967296
uint32_t MIN = 0;   
uint32_t MAX = 255;
uint32_t registers_mask = 511;
uint32_t op13_mask = 268435455;

uint32_t seg_size;
uint32_t seg_capacity;

//Memory
Array *all_segments;
Seq_T map_queue;


// typedef struct q_node
// {
//     int value;
//     q_node *next;
// }q_node;
///////////////////////////////////////////////////////////////////////////////////////////

static inline uint32_t get_register_value(uint32_t *all_registers, uint32_t num_register);

/* Memory Manager */
//uint32_t memorylength(Memory memory);         not used
static inline uint32_t segmentlength(uint32_t segment_index);
static inline uint32_t get_word(uint32_t segment_index, uint32_t word_in_segment);
static inline void set_word(uint32_t segment_index, uint32_t word_index, uint32_t word);
static inline uint32_t map_segment(uint32_t num_words);
static inline void unmap_segment(uint32_t segment_index);
static inline void duplicate_segment(uint32_t segment_to_copy);
static inline void free_segments();
//////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////////


/* Instruction retrieval */
// static inline Info get_Info(uint32_t instruction);

static inline void instruction_executer(uint32_t *all_registers, uint32_t *counter);
//////////////////////////////////////////////////////////////////////////////////////////////////////

/*Making Helper functions for our ARRAYLIST*/

static inline uint32_t array_size(Array array);
static inline uint32_t element_at(Array array, uint32_t index);
static inline void replace_at(Array *array, uint32_t insert_value, uint32_t old_value_index);
static inline void push_at_back(Array *array, uint32_t insert_value);

static inline Array* array_at(Array *segments, uint32_t segment_index);
static inline void seg_push_at_back(Array *insert_seg);
static inline void replace_array(Array *insert_array, uint32_t old_array_index);

//////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
    /*Progam can only run with two arguments [execute] and [machinecode_file]*/
    
    if (argc != 2) {
        fprintf(stderr, "Usage: ./um [filename] < [input] > [output]\n");
        exit(EXIT_FAILURE);
    }

    FILE *fp = fopen(argv[1], "r");
    assert(fp != NULL);
    struct stat sb;

    const char *filename = argv[1];

    if (stat(filename, &sb) == -1) {
        perror("stat");
        exit(EXIT_FAILURE); // TODO what should the behavior be
    }

    int seg_0_size = sb.st_size / 4;

////////////////////////////////////////////////////////////////////////////
    /* EXECUTION.C MODULE  */
    Array segment0;
    segment0.array = malloc(sizeof(uint32_t) * seg_0_size);
    segment0.size = 0;

/////////////////////////////////////// READ FILE ///////////////////////////
    uint32_t byte = 0; 
    uint32_t word = 0;
    int c;
    int counter = 0;

    /*Keep reading the file and parsing through "words" until EOF*/
    /*Populate the sequence every time we create a new word*/
    while ((c = fgetc(fp)) != EOF) {
        word = Bitpack_newu(word, 8, 24, c);
        counter++;
        for (int lsb = 2 * BYTESIZE; lsb >= 0; lsb = lsb - BYTESIZE) {
            byte = fgetc(fp);
            word = Bitpack_newu(word, BYTESIZE, lsb, byte);
        }
        
        /* Populate sequence */
        push_at_back(&segment0, word);
    }
    
    fclose(fp);

    seg_size = 0;
    seg_capacity = 10000;

    all_segments = malloc((sizeof(Array)) * seg_capacity);
    map_queue = Seq_new(30);

    seg_push_at_back(&segment0);

    uint32_t all_registers[8] = { 0 };

   /////////////////////////////////////// execution /////////////////////

    uint32_t program_counter = 0; /*start program counter at beginning*/
    
    /*Run through all instructions in segment0, note that counter may loop*/
    while (program_counter < (segmentlength(0) )) {
        

        /*program executer is passed in to update (in loop) if needed*/
        instruction_executer(all_registers, &program_counter);
        //program_counter++;
    }

    free_segments();

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    return EXIT_FAILURE; /*failure mode, out of bounds of $m[0]*/
}


static inline void seg_push_at_back(Array *insert_seg)
{
    if (seg_size != seg_capacity) {
        // printf("size %d capacity %d\n", seg_size, seg_capacity);
        all_segments[seg_size] = *insert_seg;
        seg_size++;
    }
    else {
        uint32_t new_capacity = seg_capacity * 2;
        seg_capacity = new_capacity;
        
        Array *new_segments = malloc(sizeof(Array) * new_capacity);

        for (uint32_t i = 0; i < seg_size; i++) {
            new_segments[i] = all_segments[i]; //copying structs
        }

        free(all_segments);

        all_segments = new_segments;

        all_segments[seg_size] = *insert_seg;
        seg_size++;
    }
}

static inline uint32_t array_size(Array array1)
{
    return array1.size;
}

static inline uint32_t element_at(Array array1, uint32_t index)
{
    return array1.array[index];
}

static inline void replace_at(Array *array1, uint32_t insert_value, 
                                               uint32_t old_value_index)
{
    
    // printf("old_vale_index is %u insert_value is %u\n", old_value_index, insert_value);

    if (old_value_index == array1->size){
        push_at_back(array1, insert_value);
    }
    else{
        // int size = array1->size;
        // printf("size is %d\n", size);
        array1->array[old_value_index] = insert_value;
    }

    // printf("exit\n");
}

static inline void push_at_back(Array *array1, uint32_t insert_value)
{
    uint32_t size = array_size(*array1);
    array1->array[size] = insert_value;
    array1->size = size + 1;
}

static inline uint32_t segmentlength(uint32_t segment_index)
{
    return array_size(*(array_at(all_segments, segment_index)));
    //printf("What is this return size? : %u\n", return_size);
}

static inline Array *array_at(Array *arrays, uint32_t segment_index)
{
    return &(arrays[segment_index]);
}

static inline uint32_t get_word(uint32_t segment_index, 
                  uint32_t word_in_segment)
{
    // Seq_T find_segment = (Seq_T)(Seq_get(memory->segments, segment_index));

    // uint32_t *find_word = (uint32_t *)(Seq_get(find_segment, word_in_segment));

    Array target = *((array_at(all_segments, segment_index)));

    uint32_t find_word = element_at(target, word_in_segment);

    return find_word;
}

static inline void set_word(uint32_t segment_index, 
              uint32_t word_index, uint32_t word)
{
    Array *target = array_at(all_segments, segment_index);

    // printf("index is %u\n", segment_index);

    // printf("word is %u word_index is %u\n", word, word_index);
    replace_at(target, word, word_index);
}

static inline uint32_t map_segment(uint32_t num_words)
{
    // printf("^^^map seg\n");
    
    Array new_segment;
    new_segment.array = malloc((sizeof(uint32_t)) * num_words);
    new_segment.size = num_words;
    
    /*Initialize to ALL 0s*/
    for (uint32_t i = 0; i < num_words; i++) {
        replace_at(&new_segment, 0, i);
    }


    if (Seq_length(map_queue) != 0) {
        uint32_t *seq_index = (uint32_t *)Seq_remlo(map_queue);
        replace_array(&new_segment, *seq_index);

        uint32_t segment_index = *seq_index;

        // printf("  reuse index %u\n", segment_index);
        free(seq_index);
        // printf("~ - ~map complete\n\n");
        return segment_index;
    }
    else {
        // printf("map %u\n", num_words);
        seg_push_at_back(&new_segment);

        // printf("~ ~ ~map complete\n\n");
        return seg_size - 1;
    }
}

static inline void replace_array(Array *insert_array, uint32_t old_array_index)
{
    all_segments[old_array_index] = *insert_array;
}

static inline void unmap_segment(uint32_t segment_index)
{
    // printf("-------unmap %u\n", segment_index);
    /* can't un-map segment 0 */
    assert(segment_index > 0);

    Array *seg_to_unmap = array_at(all_segments, segment_index);
    /* can't un-map a segment that isn't mapped */

    free(seg_to_unmap->array);
    //free(seg_to_unmap);
    seg_to_unmap->array = NULL;


    uint32_t *num = malloc(sizeof(uint32_t));
    *num = segment_index;
    Seq_addhi(map_queue, num);
    // printf("-------unmap complete\n");
}


static inline void duplicate_segment(uint32_t segment_to_copy)
{
    if (segment_to_copy != 0) {

        Array *seg_0 = array_at(all_segments, 0);

        /*free all c array, that is  in segment 0*/
        free(seg_0->array);

        /*hard copy - duplicate array to create new segment 0*/
        Array *target = array_at(all_segments, segment_to_copy);
        
        uint32_t target_seg_length = array_size(*target);

        seg_0->array = malloc(sizeof(uint32_t) * target_seg_length);
        seg_0->size = 0;

        /*Willl copy every single word onto the duplicate segment*/
        for (uint32_t i = 0; i < target_seg_length; i++) {
            uint32_t word = element_at(*target, i);
            push_at_back(seg_0, word);
        }

        /*replace segment0 with the duplicate*/
        //replace_array(memory, &duplicate, 0);                             
    } else {
        /*don't replace segment0 with itself --- do nothing*/
        return;
    }
}


static inline void free_segments()
{
    // printf("seg_size is %d\n", seg_size);
    for (uint32_t i = 0; i < seg_size; i++)
    {
        Array *target = array_at(all_segments, i);

        if (target->array != NULL) {
            free(target->array);
        }
    }

    /* free map_queue Sequence that kept track of unmapped stuff*/
    uint32_t queue_length = Seq_length(map_queue);
    
    /*free all the indexes words */
    for (uint32_t i = 0; i < queue_length; i++) {
        uint32_t *index = (uint32_t*)(Seq_get(map_queue, i));
        free(index);
    }

    free(all_segments);
    Seq_free(&(map_queue));
}

// static inline Info get_Info(uint32_t instruction)
// {
//     Info info;
    
//     uint32_t op = instruction;
//     op = op >> 28;

//     info.op = op;

//     if (op != 13){
//         uint32_t registers_ins = instruction &= registers_mask;

//         info.rA = registers_ins >> 6;
//         info.rB = registers_ins << 26 >> 29;
//         info.rC = registers_ins << 29 >> 29;
//     }
//     else {

//         uint32_t registers_ins = instruction &= op13_mask;

//         info.rA = registers_ins >> 25;
//         info.value = registers_ins << 7 >> 7;
//     }

//     return info;
// }


//change back for kcachegrind
static inline void instruction_executer(uint32_t *all_registers, uint32_t *counter)
{
    uint32_t instruction = get_word(0, *counter);
    
    uint32_t op = instruction;
    op = op >> 28;

    uint32_t code = op;
    uint32_t info_rA;
    uint32_t info_rB;
    uint32_t info_rC;
    uint32_t info_value;

    if (code != 13){
        uint32_t registers_ins = instruction &= registers_mask;

        info_rA = registers_ins >> 6;
        info_rB = registers_ins << 26 >> 29;
        info_rC = registers_ins << 29 >> 29;
        info_value = 0;
    }
    else {

        uint32_t registers_ins = instruction &= op13_mask;

        info_rA = registers_ins >> 25;
        info_value = registers_ins << 7 >> 7;
    }

    (*counter)++;

    /*We want a halt instruction to execute quicker*/
    if (code == HALT)
    {
        free_segments(all_segments);
        exit(EXIT_SUCCESS);
    }

    /*Rest of instructions*/
    if (code == CMOV)
    {
        ///////// conditional_move /////
        if (get_register_value(all_registers, info_rC) != 0)
        {
            uint32_t *word = &(all_registers[info_rA]);
            *word = get_register_value(all_registers, info_rB);
        }
    }
    else if (code == SLOAD)
    {
        uint32_t val = get_word(get_register_value(all_registers, info_rB), 
                       get_register_value(all_registers, info_rC));

        //set_register_value(all_registers, rA, val);
        uint32_t *word = &(all_registers[info_rA]);
        *word = val;  
    }
    else if (code == SSTORE)
    {
        set_word(get_register_value(all_registers, info_rA), 
                 get_register_value(all_registers, info_rB), 
                 get_register_value(all_registers, info_rC));
        
    }
    else if (code == ADD || code == MUL || code == DIV || code == NAND)
    {
        ////// arithetics /////

        uint32_t rB_val = get_register_value(all_registers, info_rB);
        uint32_t rC_val = get_register_value(all_registers, info_rC);

        uint32_t value = 0;

        /*Determine which math operation to perform based on 4bit opcode*/
        if (code == ADD)
        {
            value = (rB_val + rC_val) % two_pow_32;
        }
        else if (code == MUL)
        {
            value = (rB_val * rC_val) % two_pow_32;
        }
        else if (code == DIV)
        {
            value = rB_val / rC_val;
        }
        else if (code == NAND)
        {
            value = ~(rB_val & rC_val);
        }
        else
        {
            exit(EXIT_FAILURE);
        }

        uint32_t *word = &(all_registers[info_rA]);
        *word = value;
    }
    else if (code == ACTIVATE)
    {
        uint32_t *word = &(all_registers[info_rB]);
        *word = map_segment(get_register_value(all_registers, info_rC));
    }
    else if (code == INACTIVATE)
    {
        unmap_segment(get_register_value(all_registers, info_rC));
    }
    else if (code == OUT)
    {
        uint32_t val = get_register_value(all_registers, info_rC);

        assert(val >= MIN);
        assert(val <= MAX);

        printf("%c", val);
    }
    else if (code == IN)
    {
        uint32_t input_value = (uint32_t)fgetc(stdin);
        uint32_t all_ones = ~0;

        /*Check if input value is EOF...aka: -1*/
        if (input_value == all_ones)
        {
            uint32_t *word;
            word = &(all_registers[info_rC]);
            *word = all_ones;
            
            return;
        }

        /* Check if the input value is in bounds */
        assert(input_value >= MIN);
        assert(input_value <= MAX);

        uint32_t *word = &(all_registers[info_rC]);
        *word = input_value;
    }
    else if (code == LOADP)
    {
        duplicate_segment(all_registers[info_rB]);
        *counter = get_register_value(all_registers, info_rC);
    }
    else if (code == LV)
    {
        uint32_t *word = &(all_registers[info_rA]);
        *word = info_value;
    }
    else
    {
        exit(EXIT_FAILURE);
    }

    return;
}

static inline uint32_t get_register_value(uint32_t *all_registers, uint32_t num_register)
{
    return all_registers[num_register];
}
