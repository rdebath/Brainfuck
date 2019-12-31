#include <stdio.h>
#include <malloc.h>

#define REG_PTR "rbx"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: bfc <code>\n");
        return 1;
    }

    printf(".intel_syntax noprefix\n");
    printf(".global main\n");
    printf("main:\n");
    printf("    mov esi, 1\n");
    printf("    mov edi, 30000\n");
    printf("    call calloc\n");
    printf("    mov rbx, rax\n");

    char *p = argv[1];
    int *loop = (int *)malloc(sizeof(int) * 256);
    int loop_id = 0;

    while (*p)
    {
        // printf("%d\n",*p);
        switch (*p++)
        {
        case '+':
            printf("    inc BYTE PTR [%s]\n", REG_PTR);
            break;
        case '-':
            printf("    dec BYTE PTR [%s]\n", REG_PTR);
            break;
        case '>':
            printf("    inc %s\n", REG_PTR);
            break;
        case '<':
            printf("    dec %s\n", REG_PTR);
            break;
        case '.':
            printf("    mov eax, [%s]\n", REG_PTR);
            printf("    movzb edi, al\n");
            printf("    call putchar\n");
            break;
        case ',':
            printf("    call getchar\n");
            printf("    movzb eax, al\n");
            printf("    mov BYTE PTR [%s], al\n", REG_PTR);
            break;
        case '[':
            *(++loop) = loop_id++;
            printf(".L%d:\n", *loop);
            break;
        case ']':
            printf("    mov eax, [%s]\n", REG_PTR);
            printf("    movzb eax, al\n");
            printf("    test al,al\n");
            printf("    jne .L%d\n", *(loop--));
            break;
        default:
            break;
        }
    }

    printf("    mov rax, 0\n");
    printf("    ret\n");
    return 0;
}
