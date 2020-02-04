#include <stdint.h>
#include <byteswap.h>
#include <stdarg.h>
#include "error.h"
#include "code_gen.h"

// Word length
typedef uint32_t Word;

static struct {
    FILE *outfile;
    int cur_label_id;
} gen_ctx;

static void gen_ast(AstNode *ast);

/* Emit */

#define EMIT_RIGHT(imm)   emit("right %d", imm)
#define EMIT_LEFT(imm)    emit("left %d", imm)
#define EMIT_ADD(imm)     emit("add %d", imm)
#define EMIT_SUB(imm)     emit("sub %d", imm)
#define EMIT_BNZ(label)   emit("bnz l%d", label)
#define EMIT_LABEL(label) emit("l%d:", label)


// Macro for commenting the generated code. This makes debugging the generated
// code *slightly* less painful.
#ifdef DEBUG
    #define EMIT_COMMENT(comment) do { emit("; "comment); } while (0)
#else
    #define EMIT_COMMENT(comment) do {} while (0)
#endif

static void emit(const char *instr, ...)
{
    va_list args;
    va_start(args, instr);
    vfprintf(gen_ctx.outfile, instr, args);
    fprintf(gen_ctx.outfile, "\n");
    va_end(args);
}

// Generate a new label id
static int new_label()
{
    gen_ctx.cur_label_id++;
    return gen_ctx.cur_label_id;
}

// Unconditional branch
// IMPORTANT: when using this function you must only branch to a label
// generated by gen_branch_label()
static void gen_branch(int label_id)
{
    EMIT_COMMENT("unconditional branch");

    EMIT_BNZ(label_id);
    EMIT_ADD(1);
    EMIT_BNZ(label_id);

    EMIT_COMMENT("/ unconditional branch");
}

// IMPORTANT: when using this function you must only branch to a label
// generated by gen_branch_label()
static void gen_branch_if_zero(int label_id)
{
    EMIT_COMMENT("branch if zero");

    int label = new_label();

    EMIT_BNZ(label);
    EMIT_ADD(1);
    EMIT_BNZ(label_id);
    EMIT_LABEL(label);

    EMIT_COMMENT("/ branch if zero");
}

// Gen label for unconditional branch
static void gen_branch_label(int label_id)
{
    EMIT_COMMENT("branch label");

    EMIT_ADD(1);
    EMIT_LABEL(label_id);
    EMIT_SUB(1);

    EMIT_COMMENT("/ branch label");
}

// Set the value at the data pointer to zero
static void zero_at_dp()
{
    EMIT_COMMENT("zero at dp");

    int label1 = new_label();
    int label2 = new_label();

    // Check if already zero
    EMIT_BNZ(label1);
    gen_branch(label2);

    // Subtract one untill zero
    EMIT_LABEL(label1);
    EMIT_SUB(1);
    EMIT_BNZ(label1);

    gen_branch_label(label2);

    EMIT_COMMENT("/ zero at dp");
}

/* Stack */

// TODO: currently currently the stack only works with word sized values
static void stack_push(Word value)
{
    EMIT_COMMENT("stack push");

    // It is assumed the data pointer is pointing to the top of the stack
    zero_at_dp();
    EMIT_ADD(value);
    // Move the data pointer to the top of the stack
    EMIT_RIGHT(sizeof(Word));

    EMIT_COMMENT("/ stack push");
}

static void stack_pop()
{
    EMIT_COMMENT("stack pop");
    EMIT_LEFT(sizeof(Word));
    EMIT_COMMENT("/ stack pop");
}

// Move the data pointer right by n stack objects.
// TODO: Currently the size of a stack object is assumed to be 32 bits but this
//       will change when I get around to implementing things like structures.
static void stack_right(int n)
{
    EMIT_COMMENT("stack right");
    EMIT_RIGHT(sizeof(Word) * n);
    EMIT_COMMENT("/ stack right");
}

// Move the data pointer left by n stack objects.
static void stack_left(int n)
{
    EMIT_COMMENT("stack left");
    EMIT_LEFT(sizeof(Word) * n);
    EMIT_COMMENT("/ stack left");
}

/* AST Gen */

static void gen_if_stmt(AstNode *if_stmt)
{
    EMIT_COMMENT("if stmt");

    int l_else = new_label();

    // Gen the code to evaluate the condition expression
    gen_ast(if_stmt->cond);
    stack_left(1);               // Point to last item on the stack
    gen_branch_if_zero(l_else);  // Check the result of the expression
    gen_ast(if_stmt->cond_body); // If stmt body

    // Else
    gen_branch_label(l_else);
    gen_ast(if_stmt->cond_else);

    EMIT_COMMENT("/ if stmt");
}

static void gen_binary_op(AstNode *binary_op)
{
    EMIT_COMMENT("binary operation");

    // It is assumed that the result of the lhs and rhs expression is pushed to
    // the stack.
    gen_ast(binary_op->binary_left);
    gen_ast(binary_op->binary_right);

    switch (binary_op->binary_op) {
    // Addition
    case OP_PLUS: {
        int label1 = new_label();
        int label2 = new_label();

        stack_left(1); // Move dp to the last item pushed to the stack

        // If the RHS is zero don't perform addition
        gen_branch_if_zero(label2);

        EMIT_LABEL(label1);

        // Subtract 1 from the rhs and add 1 to the lhs
        EMIT_SUB(1);
        stack_left(1);
        EMIT_ADD(1);
        stack_right(1);

        EMIT_BNZ(label1);

        gen_branch_label(label2);

        break;
    }
    case OP_MINUS: {
        int label1 = new_label();
        int label2 = new_label();

        stack_left(1); // Move dp to the last item pushed to the stack

        // If the RHS is zero don't perform subtraction
        gen_branch_if_zero(label2);

        EMIT_LABEL(label1);

        EMIT_SUB(1);
        stack_left(1);
        EMIT_SUB(1);
        stack_right(1);

        EMIT_BNZ(label1);

        gen_branch_label(label2);

        break;
    }
    case OP_MULT:
    case OP_DIV:
    case OP_MODULO:
    case OP_EQUAL:
    case OP_NOT_EQUAL:
    case OP_GREATER_THAN:
    case OP_LESS_THAN:
    case OP_LESS_THAN_EQUAL:
    case OP_GREATER_THAN_EQUAL:
    case OP_LOGICAL_AND:
    case OP_LOGICAL_OR:
    case OP_BITWISE_NOT:
    case OP_BITWISE_OR:
    case OP_BITWISE_XOR:
    case OP_BITWISE_AND:
    case OP_SHIFT_LEFT:
    case OP_SHIFT_RIGHT:
    case OP_ASSIGN_EQUAL:
    case OP_ASSIGN_MINUS:
    case OP_ASSIGN_PLUS:
    case OP_ASSIGN_MULT:
    case OP_ASSIGN_DIV:
    case OP_ASSIGN_MODULO:
    case OP_ASSIGN_AND:
    case OP_ASSIGN_OR:
    case OP_ASSIGN_XOR:
    case OP_ASSIGN_SHIFT_LEFT:
    case OP_ASSIGN_SHIFT_RIGHT:
    case OP_COMMA:
        break;
    }

    EMIT_COMMENT("/ binary operation");
}

// Recursive function for generating code from the ast
static void gen_ast(AstNode *ast)
{
    if (ast == NULL)
        return;

    switch (ast->node_type) {
    case AST_FUNCTION_DEF:
        // TODO
        gen_ast(ast->func_body);
        break;
    case AST_FUNC_DECLARATION:
        break;
    case AST_FUNC_CALL:
        break;
    case AST_RETURN_STMT:
        break;
    case AST_EXPR_STMT:
        gen_ast(ast->expression);
        break;
    case AST_COMPOUND_STMT:
        for (int i = 0; i < ast->statements->length; i++)
            gen_ast(ast->statements->items[i]);
        break;
    case AST_WHILE_STMT:
        break;
    case AST_DO_WHILE_STMT:
        break;
    case AST_IF_STMT:
        gen_if_stmt(ast);
        break;
    case AST_GOTO_STMT:
        break;
    case AST_BREAK_STMT:
        break;
    case AST_CONTINUE_STMT:
        break;
    case AST_LABEL_STMT:
        break;
    case AST_SWITCH_STMT:
        break;
    case AST_CASE_STMT:
        break;
    case AST_DEFAULT_STMT:
        break;
    case AST_DECLARATION:
        break;
    case AST_DECLARATOR_HEAD:
        break;
    case AST_TYPEDEF:
        break;
    case AST_DECL_LIST:
        break;
    case AST_CONDITIONAL_EXPR:
        break;
    case AST_STRUCT_MEMBER_ACCESS:
        break;
    case AST_INTEGER_CONST:
        stack_push(ast->integer_const);
        break;
    case AST_FLOAT_CONST:
        break;
    case AST_STR_LIT:
        break;
    case AST_IDENTIFIER:
        break;
    case AST_CAST_EXPR:
        break;
    case AST_BINARY_OP:
        gen_binary_op(ast);
        break;
    case AST_UNARY_OP:
        break;
    case AST_DATA_TYPE:
        break;
    }
}

void code_gen(AstNode *ast, const char *out_filename)
{
    // Set the output file
    gen_ctx.outfile = fopen(out_filename, "wb");
    if (gen_ctx.outfile == NULL)
        error(out_filename, -1, "failed to open \"%s\".", out_filename);

    gen_ctx.cur_label_id = 0;

    // Move the data pointer to the top of the stack
    emit("right PROGRAM_SIZE");

    gen_ast(ast); // Start the code gen

    // Hang.
    int loop_label = new_label();
    gen_branch_label(loop_label);
    gen_branch(loop_label);

    fclose(gen_ctx.outfile);
}
