#include "LineReader.hpp"
#include <stdio.h>
#include <cstring>

#define DEFAULT_BUFF_SIZE 128 * 1024
#define MAX_LEN 2 * 1024 * 1024
#define MIN(a,b) ((a) < (b)) ? a : b

LineReader::LineReader(FILE* file, size_t fileSize) {
    this->file = file;
    this->fileSize = fileSize;
    this->bufferSize = DEFAULT_BUFF_SIZE;
    this->buffer = new char[bufferSize];
    bufferLen = bufferPosn = 0;
}

bool LineReader::backfill() {
    bufferPosn = 0;
    bufferLen = fread(buffer, 1, bufferSize, file);
    return (bufferLen > 0);
}

int LineReader::readLine(char* &str, int maxLineLength, int maxBytesToConsume) {
    bool hadFinalNewline = false;
    bool hadFinalReturn = false;
    bool hitEndofFile = false;
    int startPosn = bufferPosn;
    long bytesConsumed = 0;
    vector<char> temp;
    temp.reserve(500);
    int lineLen = 0;
    while (true) {
        if (bufferPosn >= bufferLen) {
            if (!backfill()) {
                hitEndofFile = true;
                break;
            }
        }
        startPosn = bufferPosn;

        for (; bufferPosn < bufferLen; bufferPosn++) {
            switch (buffer[bufferPosn]) {
                case '\n':
                    hadFinalNewline = true;
                    bufferPosn += 1;
                    goto end_while;
                case '\r':
                    if (hadFinalReturn)
                        goto end_while;
                    hadFinalReturn = true;
                    break;
                default:
                    if (hadFinalReturn)
                        goto end_while;
            }
        }

        // only go here when \r at the end of buffer or not find \n,\r
        bytesConsumed += bufferPosn - startPosn;
        int length = bufferPosn - startPosn - (hadFinalReturn ? 1 : 0);
        length = MIN(length, maxLineLength - lineLen);
        if (length > 0) {
            lineLen += length;
            for (int i = 0; i < length; i++)
                temp.push_back(buffer[startPosn + i]);
        }
        if (bytesConsumed >= maxBytesToConsume) {
            if (str != NULL)
                delete []str;
            str = new char[lineLen + 1];
            str[lineLen] = '\0';
            for (int i = 0; i < lineLen; i++) {
                str[i] = temp[i];
            }
            return (int) MIN(bytesConsumed, (long) MAX_LEN);
        }
    }
end_while:
    int newLineLength = (hadFinalNewline ? 1 : 0) + (hadFinalReturn ? 1 : 0);
    if (!hitEndofFile) {
        bytesConsumed += bufferPosn - startPosn;
        int length = bufferPosn - startPosn - newLineLength;
        length = (int) MIN(length, maxLineLength - lineLen);
        if (length > 0) {
            lineLen += length;
            for (int i = 0; i < length; i++) 
                temp.push_back(buffer[startPosn + i]);
        }
    }

    if (lineLen > 0) {
        str = new char[lineLen + 1];
        str[lineLen] = '\0';
        for (int i = 0; i < lineLen; i++) {
            str[i] = temp[i];
        }
    } else {
        str = NULL;
    }
    temp.clear();
    return (int) MIN(bytesConsumed, (long) MAX_LEN);
}

int LineReader::readLine(char* &str, int maxLineLength) {
    return readLine(str, maxLineLength, MAX_LEN);
}

int LineReader::readLine(char* &str) {
    return readLine(str, MAX_LEN, MAX_LEN);
}


void LineReader::close() {
    delete buffer;
}

LineReader::~LineReader() {
}
