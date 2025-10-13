#include <stdio.h>
#include <stdbool.h>

#define COLOR_BOLD "\033[1m"
#define COLOR_RED "\033[31m"
#define COLOR_RESET "\033[0m"

static bool g_transaction_active = false;

static void print_prompt()
{
    if (g_transaction_active)
    {
        printf(COLOR_BOLD COLOR_RED "[TX]" COLOR_RESET COLOR_BOLD " csvdb> > " COLOR_RESET);
    }
    else
    {
        printf(COLOR_BOLD "csvdb> > " COLOR_RESET);
    }
    fflush(stdout);
}

int main()
{
    print_prompt();
    // ...existing code...
    return 0;
}