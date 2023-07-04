#include "summariser.h"

#include <iostream>
#include <mutex>
#include <fstream>

static std::map<int, std::pair<int, double>> netSummary;
static std::mutex netSummaryMutex;

static std::mutex fileNamesMutex;

/*
 * Function called by each spawned thread to summarise their files
 */
static void summariserFunc(int id, int numThreads, int fileNamesSize,
                           const std::vector<std::string> &fileNames);

/*
 * Open a file at the given index in the list of file names
 */
static void openFile(std::ifstream &file, int index,
                     const std::vector<std::string> &fileNames);

/*
 * Gets the summary for a specific file
 */
static std::map<int, std::pair<int, double>> getSummary(std::ifstream &file);

/*
 * Parses a given line of a file
 */
static std::pair<int, double> parseLine(const std::string &line);

/*
 * Combines a given file's summary with the overall summary
 */
static void combineSummary(const std::map<int, std::pair<int, double>> &partSummary);

summariser::summariser(int numThreads, const std::vector<std::string> &fileNames) :
    numThreads(numThreads), fileNames(fileNames) {
}

void summariser::doWork() {
    startThreads();
    joinThreads();
}

void summariser::printSummary() {
    netSummaryMutex.lock();

    for (const auto &entry : netSummary) {
        double mean = entry.second.second / entry.second.first;
        std::cout << entry.first << ',' << mean << '\n';
    }
    std::cout << std::flush;

    netSummaryMutex.unlock();
}

void summariser::startThreads() {
    threads.reserve(numThreads);
    int fileNamesSize = fileNames.size();

    for(int i = 0; i < numThreads; i++) {
        threads.emplace_back(summariserFunc, i, numThreads, fileNamesSize,
                             fileNames);
    }
}

void summariser::joinThreads() {
    for(std::thread &thread : threads) {
        thread.join();
    }
}

static void summariserFunc(int id, int numThreads, int fileNamesSize,
                           const std::vector<std::string> &fileNames) {
    for (int i = id; i < fileNamesSize; i += numThreads) {
        std::ifstream file = std::ifstream();
        openFile(file, i, fileNames);
        if(!file.is_open()) continue;

        combineSummary(getSummary(file));
    }
}

static void openFile(std::ifstream &file, int index,
                     const std::vector<std::string> &fileNames) {
    fileNamesMutex.lock();

    file.open(fileNames[index]);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << fileNames[index];
    }

    fileNamesMutex.unlock();
}

static std::map<int, std::pair<int, double>> getSummary(std::ifstream &file) {
    std::map<int, std::pair<int, double>> currSummary;

    std::string line;
    while (std::getline(file, line)) {
        std::pair<int, double> entry = parseLine(line);
        if (entry.first < 0) continue;

        std::pair<int, double> tagSumm;
        if (currSummary.count(entry.first) != 0) {
            tagSumm = currSummary[entry.first];
            tagSumm.first++;
            tagSumm.second += entry.second;
        } else {
            tagSumm = std::pair(1, entry.second);
        }
        currSummary[entry.first] = tagSumm;
    }

    return currSummary;
}

static std::pair<int, double> parseLine(const std::string &line) {
    size_t commaPosition = line.find(',');
    if (commaPosition == std::string::npos) {
        return std::pair(-1, 0.0);
    }

    int tag;
    size_t tagSize;
    try {
        tag = std::stoi(line, &tagSize);
    } catch(...) {
        return std::pair(-1, 0.0);
    }
    if (tagSize != commaPosition) {
        return std::pair(-1, 0.0);
    }

    double value;
    size_t valueSize;
    try {
        value = std::stod(line.substr(commaPosition + 1), &valueSize);
    } catch(...) {
        return std::pair(-1, 0.0);
    }
    if (commaPosition + 1 + valueSize != line.size()) {
        return std::pair(-1, 0.0);
    }

    return std::pair(tag, value);
}


static void combineSummary(const std::map<int, std::pair<int, double>> &partSummary) {
    netSummaryMutex.lock();
    
    for (const auto &entry : partSummary) {
        if (netSummary.find(entry.first) == netSummary.end()) {
            netSummary[entry.first] = entry.second;
        } else {
            netSummary[entry.first].first += entry.second.first;
            netSummary[entry.first].second += entry.second.second;
        }
    }

    netSummaryMutex.unlock();
}
