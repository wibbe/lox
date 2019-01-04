
#ifndef LOX_CHUNK_H
#define LOX_CHUNK_H

#include "common.h"
#include "memory.h"
#include "value.h"

typedef enum {
	OP_CONSTANT,
	OP_CONSTANT_LONG,
	OP_NIL,
	OP_TRUE,
	OP_FALSE,
	OP_EQUAL,
	OP_GREATER,
	OP_LESS,
	OP_ADD,
	OP_SUBTRACT,
	OP_MULTIPLY,
	OP_DIVIDE,
	OP_NOT,
	OP_NEGATE,
	OP_RETURN,
} OpCode;


typedef struct {
	int count;
	int capacity;
	uint8_t * code;
	int * lines;
	ValueArray constants;
} Chunk;


void initChunk(Chunk * chunk);
void freeChunk(Chunk * chunk);

void writeChunk(Chunk * chunk, uint8_t byte, int line);

int addConstant(Chunk * chunk, Value value);

#endif
