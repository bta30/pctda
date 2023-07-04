#include "args.h"

#include <iostream>

args::args(int argc, char *argv[]) {
    if (!validNumberArgs(argc)) return;
    if (!setThreads(argc, argv[1])) return;
    setFileNames(argc, argv);
}

bool args::isValid() {
    return numThreads > 0 && numThreads <= fileNames.size();
}

bool args::validNumberArgs(int argc) {
    if (argc < 3) {
        std::cerr << "Argument error: Too few arguments" << std::endl;
        return false;
    }

    return true;
}

bool args::setThreads(int argc, const char *threads) {
    try {
        numThreads = std::stoi(threads);
    } catch (const std::exception &e) {
        std::cerr << "Argument error: Exception in parsing number of threads - " << e.what() << std::endl;
        return false;
    }

    if (numThreads <= 0) {
        std::cerr << "Argument error: Number of threads must be a positive integer" << std::endl;
        return false;
    }

    if (numThreads > argc - 2) {
        std::cerr << "Argument error: Too many threads for given files" << std::endl;
        return false;
    }

    return true;
}

void args::setFileNames(int argc, char *argv[]) {
    fileNames.reserve(argc - 2);

    for (int i = 2; i < argc; i++) {
        fileNames.emplace_back(argv[i]);
    }
}
