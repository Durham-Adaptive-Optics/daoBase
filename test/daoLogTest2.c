#include "daoLog2_wrapper.hpp"

int main() 
{

    daoLog_t * log = daoLog_create();

    daoLogDebug(log, "This is a test log %d %d", 1,2);
    daoLogInfo(log, "This is a test log %d %d", 1,2);
    daoLogWarning(log, "This is a test log %d %d", 1,2);
    daoLogError(log, "This is a test log %d %d", 1,2);
    daoLogCritical (log, "This is a test log %d %d", 1,2);

    daoLog_destroy(log);

    return 0;
}
