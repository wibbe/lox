
#ifndef LOX_VALUE_H
#define LOX_VALUE_H

#include "common.h"


typedef struct sObj Obj;
typedef struct sObjString ObjString;

typedef enum {
	VAL_BOOL,
	VAL_NIL,
	VAL_NUMBER,
	VAL_OBJ,
} ValueType;


typedef struct {
	ValueType type;
	union {
		bool boolean;
		double number;
		Obj * obj;
	} as;
} Value;


#define IS_BOOL(value)		((value).type == VAL_BOOL)
#define IS_NIL(value)		((value).type == VAL_NIL)
#define IS_NUMBER(value)	((value).type == VAL_NUMBER)
#define IS_OBJ(value)		((value).type == VAL_OBJ)

#define AS_BOOL(value)		((value).as.boolean)
#define AS_NUMBER(value)	((value).as.number)
#define AS_OBJ(value)		((value).as.obj)

#define BOOL_VAL(value)		((Value){ VAL_BOOL, { .boolean = (value) } })
#define NIL_VAL				((Value){ VAL_NIL, { .number = 0 } })
#define NUMBER_VAL(value)	((Value){ VAL_NUMBER, { .number = (value) }})
#define OBJ_VAL(value)		((Value){ VAL_OBJ, { .obj = (Obj *)(value) }})


bool valuesEqual(Value a, Value b);

typedef struct {
	int count;
	int capacity;
	Value * values;
} ValueArray;


void initValueArray(ValueArray * array);
void freeValueArray(ValueArray * array);
void writeValueArray(ValueArray * array, Value value);

void printValue(Value value);

#endif