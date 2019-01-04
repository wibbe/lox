
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "object.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,    // =
    PREC_OR,            // or
    PREC_AND,           // and
    PREC_EQUALITY,      // == !=
    PREC_COMPARISON,    // < > <= >=
    PREC_TERM,          // + -
    PREC_FACTOR,        // * /
    PREC_UNARY,         // ! - +
    PREC_CALL,          // . () []
    PREC_PRIMARY,
} Precedence;

typedef struct Compiler_ Compiler;

typedef void (*ParseFn)(Compiler * compiler);

typedef struct Parser_ {
    Token current;
    Token previous;
    bool hasError;
    bool panicMode;
} Parser;

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

struct Compiler_ {
    Scanner scanner;
    Parser parser;
    Chunk * compilingChunk;
};


static Chunk * currentChunk(Compiler * compiler)
{
    return compiler->compilingChunk;
}

static void errorAt(Compiler * compiler, Token * token, const char * message)
{
    if (compiler->parser.panicMode)
        return;

    compiler->parser.panicMode = true;

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF)
    {
        fprintf(stderr, " at end");
    }
    else if (token->type == TOKEN_ERROR)
    {
        // Nothing
    }
    else
    {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    compiler->parser.hasError = true;
}

static void error(Compiler * compiler, const char * message)
{
    errorAt(compiler, &compiler->parser.previous, message);
}

static void errorAtCurrent(Compiler * compiler, const char * message)
{
    errorAt(compiler, &compiler->parser.current, message);
}

static void advance(Compiler * compiler)
{
    printf("Advance\n");

    compiler->parser.previous = compiler->parser.current;

    printf("  Previous: %d (%.*s)\n", compiler->parser.previous.type, compiler->parser.previous.length, compiler->parser.previous.start);

    for (;;)
    {
        compiler->parser.current = scanToken(&compiler->scanner);
        if (compiler->parser.current.type != TOKEN_ERROR)
            break;

        errorAtCurrent(compiler, compiler->parser.current.start);
    }
    
    printf("  Current:  %d (%.*s)\n", compiler->parser.current.type, compiler->parser.current.length, compiler->parser.current.start);   
}

static void consume(Compiler * compiler, TokenType type, const char * message)
{
    printf("Consuming token %d (%s)\n", type, message);

    if (compiler->parser.current.type == type)
    {
        advance(compiler);
        return;
    }

    errorAtCurrent(compiler, message);
}

static void emitByte(Compiler * compiler, uint8_t byte)
{
    writeChunk(currentChunk(compiler), byte, compiler->parser.previous.line);
}

static void emitBytes(Compiler * compiler, uint8_t byte1, uint8_t byte2)
{
    emitByte(compiler, byte1);
    emitByte(compiler, byte2);
}

static void emitReturn(Compiler * compiler)
{
    emitByte(compiler, OP_RETURN);
}

static int makeConstant(Compiler * compiler, Value value)
{
    int constant = addConstant(currentChunk(compiler), value);
    if (constant > UINT8_MAX)
    {
        error(compiler, "Too many constants in one chunk.");
        return 0;
    }

    return constant;
}

static void emitConstant(Compiler * compiler, Value value)
{
    int constant = makeConstant(compiler, value);

    if (constant < 256)
    {
        emitBytes(compiler, OP_CONSTANT, (uint8_t)constant);
    }
    else
    {
        emitByte(compiler, OP_CONSTANT_LONG);
        emitBytes(compiler, (uint8_t)((constant & 0x0000ff00) >> 8), (uint8_t)(constant & 0x000000ff));
    }
}

static void endCompiler(Compiler * compiler)
{
    emitReturn(compiler);

#ifdef DEBUG_PRINT_CODE
    if (!compiler->parser.hasError)
    {
        dissassembleChunk(currentChunk(compiler), "code");
    }
#endif
}

static void expression(Compiler * compiler);
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Compiler * compiler, Precedence precedence);


static void binary(Compiler * compiler)
{
    printf("Binary\n");

    TokenType operatorType = compiler->parser.previous.type;

    ParseRule * rule = getRule(operatorType);
    parsePrecedence(compiler, (Precedence)(rule->precedence + 1));

    switch (operatorType)
    {
        case TOKEN_BANG_EQUAL:      emitBytes(compiler, OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL:     emitByte(compiler, OP_EQUAL); break;
        case TOKEN_GREATER:         emitByte(compiler, OP_GREATER); break;
        case TOKEN_GREATER_EQUAL:   emitBytes(compiler, OP_LESS, OP_NOT); break;
        case TOKEN_LESS:            emitByte(compiler, OP_LESS); break;
        case TOKEN_LESS_EQUAL:      emitBytes(compiler, OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS:            emitByte(compiler, OP_ADD); break;
        case TOKEN_MINUS:           emitByte(compiler, OP_SUBTRACT); break;
        case TOKEN_STAR:            emitByte(compiler, OP_MULTIPLY); break;
        case TOKEN_SLASH:           emitByte(compiler, OP_DIVIDE); break;
        default:
            return; // Unreachable.
    }
}

static void literal(Compiler * compiler)
{
    printf("Literal\n");
    switch (compiler->parser.previous.type)
    {
        case TOKEN_FALSE:   emitByte(compiler, OP_FALSE); break;
        case TOKEN_NIL:     emitByte(compiler, OP_NIL); break;
        case TOKEN_TRUE:    emitByte(compiler, OP_TRUE); break;
        default:
            return; // Unreachable.
    }
}

static void grouping(Compiler * compiler)
{
    printf("Grouping\n");
    expression(compiler);
    consume(compiler, TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
}

static void number(Compiler * compiler)
{
    printf("Number\n");
    double value = strtod(compiler->parser.previous.start, NULL);
    emitConstant(compiler, NUMBER_VAL(value));
}

static void string(Compiler * compiler)
{
    printf("String\n");
    emitConstant(compiler, OBJ_VAL(copyString(compiler->parser.previous.start + 1,
                                              compiler->parser.previous.length - 2)));
}

static void unary(Compiler * compiler)
{
    printf("Unary\n");
    TokenType operatorType = compiler->parser.previous.type;

    // Compile the operand.
    parsePrecedence(compiler, PREC_UNARY);

    // Emit the operator instruction.
    switch (operatorType)
    {
        case TOKEN_BANG: emitByte(compiler, OP_NOT); break;
        case TOKEN_MINUS: emitByte(compiler, OP_NEGATE); break;
        default:
            return; // Unreachable.
    }
}

ParseRule rules[] = {
    { grouping,     NULL,       PREC_CALL },        // TOKEN_LEFT_PAREN
    { NULL,         NULL,       PREC_NONE },        // TOKEN_RIGHT_PAREN
    { NULL,         NULL,       PREC_NONE },        // TOKEN_LEFT_BRACE
    { NULL,         NULL,       PREC_NONE },        // TOKEN_RIGHT_BRACE     
    { NULL,         NULL,       PREC_NONE },        // TOKEN_COMMA           
    { NULL,         NULL,       PREC_CALL },        // TOKEN_DOT             
    { unary,        binary,     PREC_TERM },        // TOKEN_MINUS           
    { NULL,         binary,     PREC_TERM },        // TOKEN_PLUS            
    { NULL,         NULL,       PREC_NONE },        // TOKEN_SEMICOLON       
    { NULL,         binary,     PREC_FACTOR },      // TOKEN_SLASH           
    { NULL,         binary,     PREC_FACTOR },      // TOKEN_STAR            
    { unary,        NULL,       PREC_NONE },        // TOKEN_BANG            
    { NULL,         binary,     PREC_EQUALITY },    // TOKEN_BANG_EQUAL      
    { NULL,         NULL,       PREC_NONE },        // TOKEN_EQUAL           
    { NULL,         binary,     PREC_EQUALITY },    // TOKEN_EQUAL_EQUAL     
    { NULL,         binary,     PREC_COMPARISON },  // TOKEN_GREATER         
    { NULL,         binary,     PREC_COMPARISON },  // TOKEN_GREATER_EQUAL   
    { NULL,         binary,     PREC_COMPARISON },  // TOKEN_LESS            
    { NULL,         binary,     PREC_COMPARISON },  // TOKEN_LESS_EQUAL      
    { NULL,         NULL,       PREC_NONE },        // TOKEN_IDENTIFIER      
    { string,       NULL,       PREC_NONE },        // TOKEN_STRING          
    { number,       NULL,       PREC_NONE },        // TOKEN_NUMBER          
    { NULL,         NULL,       PREC_AND },         // TOKEN_AND             
    { NULL,         NULL,       PREC_NONE },        // TOKEN_CLASS           
    { NULL,         NULL,       PREC_NONE },        // TOKEN_ELSE            
    { literal,      NULL,       PREC_NONE },        // TOKEN_FALSE           
    { NULL,         NULL,       PREC_NONE },        // TOKEN_FOR             
    { NULL,         NULL,       PREC_NONE },        // TOKEN_FUN             
    { NULL,         NULL,       PREC_NONE },        // TOKEN_IF              
    { literal,      NULL,       PREC_NONE },        // TOKEN_NIL             
    { NULL,         NULL,       PREC_OR },          // TOKEN_OR              
    { NULL,         NULL,       PREC_NONE },        // TOKEN_PRINT           
    { NULL,         NULL,       PREC_NONE },        // TOKEN_RETURN          
    { NULL,         NULL,       PREC_NONE },        // TOKEN_SUPER           
    { NULL,         NULL,       PREC_NONE },        // TOKEN_THIS            
    { literal,      NULL,       PREC_NONE },        // TOKEN_TRUE            
    { NULL,         NULL,       PREC_NONE },        // TOKEN_VAR             
    { NULL,         NULL,       PREC_NONE },        // TOKEN_WHILE           
    { NULL,         NULL,       PREC_NONE },        // TOKEN_ERROR           
    { NULL,         NULL,       PREC_NONE },        // TOKEN_EOF       
};

static void parsePrecedence(Compiler * compiler, Precedence precedence)
{
    printf("ParsePrecedence: %d\n", precedence);
    printf("  Previous: %d (%.*s)\n", compiler->parser.previous.type, compiler->parser.previous.length, compiler->parser.previous.start);
    printf("  Current:  %d (%.*s)\n", compiler->parser.current.type, compiler->parser.current.length, compiler->parser.current.start);

    advance(compiler);
    ParseFn prefixRule = getRule(compiler->parser.previous.type)->prefix;
    if (prefixRule == NULL)
    {
        error(compiler, "Expected expression.");
        return;
    }

    printf("  PrefixRule\n");
    prefixRule(compiler);

    while (precedence <= getRule(compiler->parser.current.type)->precedence)
    {
        advance(compiler);
        ParseFn infixRule = getRule(compiler->parser.previous.type)->infix;
        printf("  InfixRule\n");
        infixRule(compiler);
    }
}

static ParseRule * getRule(TokenType type)
{
    return &rules[type];
}

static void expression(Compiler * compiler)
{
    printf("Expression\n");
    parsePrecedence(compiler, PREC_ASSIGNMENT);
}

bool compile(const char * source, Chunk * chunk)
{
    Compiler compiler;
    initScanner(&compiler.scanner, source);

    compiler.compilingChunk = chunk; 
    compiler.parser.hasError = false;
    compiler.parser.panicMode = false;

    advance(&compiler);
    expression(&compiler);
    consume(&compiler, TOKEN_EOF, "Expected end of expression.");
    endCompiler(&compiler);
    return !compiler.parser.hasError;
}
