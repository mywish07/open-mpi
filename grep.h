/* 
 * File:   rep.h
 * Author: novpla
 *
 * Created on November 10, 2014, 1:44 PM
 */

#ifndef GREP_H
#define	GREP_H

#include <sys/types.h>
#include <stdio.h>

class Grep{
public:
    size_t lastPosition = 0;
    long long int buff_size;
    int grep(int argc, char *argv[]);
private:
    void print(char* str, int start, int len);
    char* readFile(FILE* file, size_t fileSize);
};

#endif	/* GREP_H */

