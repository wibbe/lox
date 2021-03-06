
#ifndef LOX_VM_H
#define LOX_VM_H

#include "chunk.h"
#include "value.h"
#include "table.h"


#define STACK_MAX 256


typedef enum {
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR,
} InterpretResult;

typedef struct {
	Chunk * chunk;
	uint8_t * ip;
	Value stack[STACK_MAX];
	Value * stackTop;
	Table strings;

	Obj * objects;
} VM;


extern VM vm;


void initVM();
void freeVM();


InterpretResult interpret(const char * source);

void push(Value value);
Value pop();

#endif