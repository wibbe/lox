
#ifndef LOX_DEBUG_H
#define LOX_DEBUG_H

#include "chunk.h"

void dissassembleChunk(Chunk * chunk, const char * name);
int dissassembleInstruction(Chunk * chunk, int offset);

#endif