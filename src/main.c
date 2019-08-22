#include <stdio.h>

#include "ast.h"
#include "ir.h"

extern AstNode *parse(char *filename);

int main(int argc, char *argv[])
{
    AstNode *ast;
    if (argc == 2)
        ast = parse(argv[1]);
    print_ast(ast);
    printf("\nAST Printing done.\n");
    //ast_to_ir(ast);
    //ir_print();
}
