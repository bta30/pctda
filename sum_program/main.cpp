#include "args.h"
#include "summariser.h"

#include <iostream>

int main(int argc, char *argv[]) {
    args progArgs(argc, argv);
    if (!progArgs.isValid()) return 0;

    summariser summ(progArgs.numThreads, progArgs.fileNames);
    summ.doWork();
    summ.printSummary();
    
    return 0;
}
