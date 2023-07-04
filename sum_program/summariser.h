#ifndef SUMMARISER_H
#define SUMMARISER_H

#include <string>
#include <vector>
#include <map>
#include <thread>

// Summarises a given list of files with a given number of threads
class summariser {
public:
    summariser(int numThreads, const std::vector<std::string> &fileNames);

    /*
     * Starts and waits on all threads to perform their work
     */
    void doWork();

    /*
     * Prints the overall summary so far
     */
    void printSummary();
    
private:
    int numThreads;
    const std::vector<std::string> &fileNames;
    std::vector<std::thread> threads;

    /*
     * Starts executing each thread
     */
    void startThreads();

    /*
     * Waits for all threads to finish executing
     */
    void joinThreads();
};

#endif
