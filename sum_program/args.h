#ifndef ARGS_H
#define ARGS_H

#include <string>
#include <vector>

// Parses program arguments
class args {
public:
    args(int argc, char *argv[]);
    
   /*
    * Returns true if given valid arguments,
    * returns false otherwise
    */
   bool isValid();

   int numThreads;
   std::vector<std::string> fileNames;

private:
    /*
     * Returns true if given a valid number of arguments,
     * returns false and prints an error otherwise
     */
    bool validNumberArgs(int argc);

    /*
     * Sets the number of threads
     * 
     * Returns true if the given number of threads is valid,
     * returns false and prints an error otherwise
     */
    bool setThreads(int argc, const char *threads);

    /*
     * Sets the list of file names
     */
    void setFileNames(int argc, char *argv[]);
};

#endif
