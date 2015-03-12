/* 
 * File:   wordcount.h
 * Author: novpla
 *
 * Created on November 3, 2014, 11:39 AM
 */

#ifndef WORDCOUNT_H
#define	WORDCOUNT_H

#include <sys/types.h>
#include <stdio.h>
class WordCount{
public:
    size_t lastPosition = 0;
    long long int buff_size;
    int debug = 0;
    int count(int argc, char *argv[]);
private:
    void print(char* str, int start, int len);
    char* readFile(FILE* file, size_t fileSize);
};


#endif	/* WORDCOUNT_H */

