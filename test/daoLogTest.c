#include "daoBase.h"

int main() 
{
    daoLogSetLevel(LOG_LEVEL_TRACE);

    daoLogError("An error occurred: %s\n", "file not found");
    daoLogWarning("This program may not work correctly on some systems.\n");
    daoLogInfo("The program has started.\n");
    daoLogDebug("Variable x has a value of %d.\n", 42);
    daoLogTrace("The function foo() was called.\n");

    daoSetLogLevel(DAO_TRACE);
    daoE rror("An error occurred: %s", "file not found\n");
    daoWarning("This program may not work correctly on some systems.\n");
    daoInfo("The program has started.\n");
    daoDebug("Variable x has a value of %d.\n", 42);
    daoTrace("The function foo() was called.\n");
    return 0;
}
