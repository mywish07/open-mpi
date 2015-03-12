/* 
 * File:   LineReader.hpp
 * Author: novpla
 *
 * Created on February 12, 2015, 11:52 AM
 */

#ifndef LINEREADER_HPP
#define	LINEREADER_HPP

#include <string>
#include <vector>
#include <fstream>

#include <stdio.h>

using namespace std;

class LineReader {
private:
    FILE* file;
    size_t fileSize;
    int bufferSize;
    char* buffer;
    // the number of bytes of real data in the buffer
    int bufferLen;
    // the current position in the buffer
    int bufferPosn;
public:
    LineReader(FILE* file, size_t fileSize);
    bool backfill();
    int readLine(char* &str,int maxLineLength, int maxBytesToConsume);
    int readLine(char* &str,int maxLineLength);
    int readLine(char* &str);
    void close();
    ~LineReader();
};

#endif	/* LINEREADER_HPP */

