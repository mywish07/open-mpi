#include "wordcount.h"
#include <iostream>
#include <fstream>
#include "mpi.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>



using namespace std;

#define MIN(A,B) ((A) < (B)) ? (A) : (B)

#define BUFF_SIZE               256000000 // 256MB of data

int WordCount::count(int argc, char* argv[]) {

    typedef struct {
        int count;
        char word[30];
    } WordStruct;
    int blocks[2] = {1, 30};
    MPI_Datatype types[2] = {MPI_INT, MPI_CHAR};
    MPI_Aint displacements[2];
    MPI_Datatype obj_type;
    MPI_Aint charex, intex;

    int nTasks, rank;
    MPI::Init(argc, argv);
    nTasks = MPI::COMM_WORLD.Get_size();
    rank = MPI::COMM_WORLD.Get_rank();

    MPI_Type_extent(MPI_INT, &intex);
    MPI_Type_extent(MPI_CHAR, &charex);
    displacements[0] = static_cast<MPI_Aint> (0);
    displacements[1] = intex;
    MPI_Type_struct(2, blocks, displacements, types, &obj_type);
    MPI_Type_commit(&obj_type);

    // When creating out buffer, we use the new operator to avoid stack overflow.
    // If you are programming in C instead of C++, you'll want to use malloc.
    // Please note that this is not the most efficient way to use memory.
    // We are sacrificing memory now in order to gain more performance later.

    long long int *pLineStartIndex; // keep track of which lines start where. Mainly for debugging
    char *pszFileBuffer; 
    int nTotalLines = 0;

    if (argc >= 3)
        buff_size = atoll(argv[2]);
    else
        buff_size = BUFF_SIZE;
    if (argc == 4)
        debug = 1;
    double startTime = 0;
    startTime = MPI::Wtime();
    FILE* file;
    size_t total_size;
    if (rank == 0) {
        file = fopen("input.txt", "r+");
        struct stat filestatus;
        stat("input.txt", &filestatus);
        total_size = filestatus.st_size;
    }
    map<string, int> totalwordcount;
    double totalTime_noRead = 0;
    double totalTime_NoDist = 0;
    while (1) {
        int cont = 1;
        if (rank == 0) {
            pLineStartIndex = new long long int[buff_size / 10]; // assume number of line is equal to number of character / 10
            pLineStartIndex[0] = 0;
            pszFileBuffer = readFile(file, total_size);
            if (pszFileBuffer == NULL)
                cont = 0;
        }
        nTotalLines = 0;
        MPI::COMM_WORLD.Bcast(&cont, 1, MPI::INT, 0);
        if (cont == 0) 
            break;
        if (rank == 0) {
            char* ptr = strchr(pszFileBuffer, 10); // find a new line character
            while (ptr != NULL) {
                nTotalLines++;
                pLineStartIndex[nTotalLines] = (ptr - pszFileBuffer + 1);
                ptr = strchr(ptr + 1, 10);
            }
            pLineStartIndex[nTotalLines + 1] = strlen(pszFileBuffer);
        }

        double startTime_noRead = MPI::Wtime();
        // Thread zero needs to distribute data to other threads.
        // Because this is a relatively large amount of data, we SHOULD NOT send the entire dataset to all threads.
        // Instead, it's best to intelligently break up the data, and only send relevant portions to each thread.
        // Data communication is an expensive resource, and we have to minimize it at all costs.

        char *buffer = NULL;
        int totalChars = 0;
        int portion = 0;
        int startNum = 0;
        int endNum = 0;

        
        if (rank == 0) {
            portion = nTotalLines / nTasks;
            startNum = 0;
            endNum = portion;
            buffer = new char[pLineStartIndex[endNum] + 1];
            strncpy(buffer, pszFileBuffer, pLineStartIndex[endNum]);
            buffer[pLineStartIndex[endNum]] = '\0';
            for (int i = 1; i < nTasks; i++) {
                // calculate the data for each thread.
                int curStartNum = i * portion;
                int curEndNum = (i + 1) * portion - 1;
                if (i == nTasks - 1) {
                    curEndNum = nTotalLines;
                }
                if (curStartNum < 0) {
                    curStartNum = 0;
                }

                // we need to send a thread the number of characters it will be receiving.
                int curLength = pLineStartIndex[curEndNum + 1] - pLineStartIndex[curStartNum];
                MPI_Send(&curLength, 1, MPI_INT, i, 1, MPI_COMM_WORLD);
                if (curLength > 0)
                    MPI_Send(pszFileBuffer + pLineStartIndex[curStartNum], curLength, MPI_CHAR, i, 2, MPI_COMM_WORLD);
            }
            delete []pszFileBuffer;
            delete []pLineStartIndex;
            if (debug)
                cout << "Sent data successfully.\n";
        } else {
            // We are not the thread that read the file.
            // We need to receive data from whichever thread 
            MPI_Status status;
            MPI_Recv(&totalChars, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);
            if (totalChars > 0) {
                buffer = new char[totalChars + 1];
                MPI_Recv(buffer, totalChars, MPI_CHAR, 0, 2, MPI_COMM_WORLD, &status);
                buffer[totalChars] = '\0';
                // Thread 0 is responsible for making sure the startNum and endNum calculated here are valid.
                // This is because thread 0 tells us exactly how many characters we were send.
                if (debug)
                    cout << "Process: " << rank << " received data.\n";
            }
        }

        double startTime_noDist = MPI::Wtime();
        // Do the search
        map<string, int> wordcount;
        int size = 0;
        WordStruct* words = NULL;
        if (buffer != NULL) {
            char* word = strtok(buffer, " ,.\r\n");
            while (word != NULL) {
                if (wordcount.find(word) == wordcount.end())
                    wordcount[word] = 1;
                else
                    wordcount[word]++;
                word = strtok(NULL, " ,.\r\n");
            }
            delete []buffer;

            size = wordcount.size();
            if (size > 0) {
                words = new WordStruct[size];
                int i = 0;
                for (map<string, int>::iterator it = wordcount.begin(); it != wordcount.end(); it++) {

                    strcpy(words[i].word, (it->first).c_str());
                    words[i].count = it->second;
                    i++;
                }
            }
        }
        
        
        // At this point, all threads need to communicate their results to thread 0.

        if (rank == 0) {
            // The master thread will need to receive all computations from all other threads.
            MPI_Status status;

            // We need to go and receive the data from all other threads.

            // aggregate the result for current chunk of data
            for (int i = 1; i < nTasks; i++) {
                int sz;
                MPI_Recv(&sz, 1, MPI_INT, i, 3, MPI_COMM_WORLD, &status);
                if (sz > 0) {
                    WordStruct* local_words = new WordStruct[sz];
                    MPI_Recv(local_words, sz, obj_type, i, 4, MPI_COMM_WORLD, &status);

                    for (int j = 0; j < sz; j++) {
                        totalwordcount[local_words[j].word] += local_words[j].count;
                    }
                    delete []local_words;
                }
            }

            for (map<string, int>::iterator it = wordcount.begin(); it != wordcount.end(); it++) {
                totalwordcount[it->first] += it->second;
            }
            if (debug)
                cout << "Received final result for this chunk.\n";
        } else {
            // We are finished with the results in this thread, and need to send the data to thread 1.
            // The destination is thread 0

            MPI_Send(&size, 1, MPI_INT, 0, 3, MPI_COMM_WORLD);
            if (size > 0) {
                MPI_Send(words, size, obj_type, 0, 4, MPI_COMM_WORLD);
                if (debug)
                    cout << "Process: " << rank << " sent partial result.\n";
            }
        }
        wordcount.clear();
        if (words != NULL && size > 0)
            delete []words;
        cout << "Process: " << rank << " released memory.\n";
        totalTime_noRead += (MPI::Wtime() - startTime_noRead);
        totalTime_NoDist += (MPI::Wtime() - startTime_noDist);
    }
    if (rank == 0) {
        if (debug)
            cout << "Generating output...\n";
        double t = MPI::Wtime();
        fclose(file);   
        // Display the final calculated value
        ofstream out("output.txt");
        for (map<string, int>::iterator it = totalwordcount.begin(); it != totalwordcount.end(); it++) {
            out << it->first << ": " << it->second << "\n";
        }
        totalwordcount.clear();
        out.close();
        double endTime = MPI::Wtime();
        cout << "Time: " << endTime - startTime << " seconds" << endl;
        cout << "Time (no reads): " << totalTime_noRead + endTime - t << " seconds" << endl;
        cout << "Time (no data distribution): " << totalTime_NoDist + endTime - t << " seconds" << endl;
    }
    MPI_Finalize();
    return 0;
}

void WordCount::print(char* str, int start, int len) {
    cout << "@";
    for (int i = 0; i < len; i++) {
        if (str[start + i] == '\r')
            cout << "EOL";
        else if (str[start + i] == '\n')
            cout << "NL";
        else
        cout << str[start + i];
    }
    cout << "@" << endl;
}

char* WordCount::readFile(FILE* file, size_t fileSize) {
    long long readsize = MIN(buff_size, fileSize - lastPosition);
    if (readsize <= 0)
        return NULL;

    char* str = new char[readsize + 1];
    fseek(file, lastPosition, SEEK_SET); // read file from last position
    fread(str, 1, readsize, file);

    // trim the end of string: remove part of string after the last space
    long start = 0;
    if (readsize > 50)
        start = readsize - 50;
    // check from last 50 characters
    char* ptr = strchr(&str[start], ' ');

    if (ptr == NULL || readsize < buff_size) { // return entire string
        lastPosition = fileSize;
        str[readsize] = '\0';
        return str;
    }
    char* pre = NULL;
    while (ptr != NULL) {
        pre = ptr;
        ptr = strchr(ptr + 1, ' ');
    }

    int rSize = pre - str + 1;

    if (pre != NULL) {
        *pre = '\0';
    }

    // update current read position
    lastPosition += rSize;
    if (debug)
        cout << "Read file succeeded." << endl;
    return str;
}
