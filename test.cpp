#include "LineReader.hpp"
#include <fstream>
#include <sys/stat.h>

int main2(int argc, char* argv[]) {
    FILE* file;
    size_t total_size;
    file = fopen("input.txt", "r+");
    struct stat filestatus;
    stat("input.txt", &filestatus);
    total_size = filestatus.st_size;

    LineReader reader(file, total_size);
    char* line = NULL;
    reader.readLine(line);
    printf("%s\n", line);
    return 0;
}
