#include "grep.h"
#include <iostream>
#include <fstream>
#include "mpi.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <regex>

using namespace std;
#define MAX_ERROR_MSG 0x1000
#define C_MAX_NUM_LINES		10000000		// define the maximum number of lines we support.
#define C_MAX_LINE_WIDTH	512             // define the maximum number of characters per line we support.

int grep(int argc, char* argv[]) {

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

    int *pLineStartIndex; // = new int[C_MAX_NUM_LINES]; // keep track of which lines start where. Mainly for debugging
    char *pszFileBuffer; // = new char[C_MAX_NUM_LINES * C_MAX_LINE_WIDTH];
    int nTotalLines = 0;

    char* reg = new char[128];
    int regex_len;
    int startTime = 0;
    if (rank == 0) {
        cout << "Please input the string you would like to search for: ";
        cin >> reg;
        regex_len = strlen(reg);

        // File I/O often takes as much time, or more time than the actual computation.
        // If you would like some exercise, modify this program so that one thread will
        // get user input, and another thread will read the file. Remember, the program
        // may be instantiated with 1 or more processors.

        startTime = MPI::Wtime();
        ifstream file("input.txt", ifstream::in);
        struct stat filestatus;
        stat("input.txt", &filestatus);
        size_t total_size = filestatus.st_size;
        pszFileBuffer = new char[total_size];
        pLineStartIndex = new int[C_MAX_NUM_LINES];
        pLineStartIndex[0] = 0;
        string line;
        while (getline(file, line)) {
            strcpy(pszFileBuffer + pLineStartIndex[nTotalLines], line.c_str()); // copy line into our buffer
            int length = line.length();
            nTotalLines++;
            // store where the next line will start
            pLineStartIndex[nTotalLines] = pLineStartIndex[nTotalLines - 1] + length;
        }
        file.close();

    }

    int startTime_noRead = MPI::Wtime();
    MPI_Bcast(&regex_len, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Now receive the Word. We're adding 1 to the length to allow for NULL termination.
    MPI_Bcast(reg, regex_len + 1, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Thread zero needs to distribute data to other threads.
    // Because this is a relatively large amount of data, we SHOULD NOT send the entire dataset to all threads.
    // Instead, it's best to intelligently break up the data, and only send relevant portions to each thread.
    // Data communication is an expensive resource, and we have to minimize it at all costs.
    // This is a key concept to learn in order to make high performance applications.
    char *buffer;
    int totalChars = 0;
    int portion = 0;
    int startNum = 0;
    int endNum = 0;

    if (rank == 0) {
        portion = nTotalLines / nTasks;
        startNum = 0;
        endNum = portion;
        
        buffer = new char[pLineStartIndex[endNum]];
        strncpy(buffer, pszFileBuffer, pLineStartIndex[endNum]);

        for (int i = 1; i < nTasks; i++) {
            // calculate the data for each thread.
            int curStartNum = i * portion;
            int curEndNum = (i + 1) * portion - 1;
            if (i == nTasks - 1) {
                curEndNum = nTotalLines - 1;
            }
            if (curStartNum < 0) {
                curStartNum = 0;
            }

            // we need to send a thread the number of characters it will be receiving.
            int curLength = pLineStartIndex[curEndNum + 1] - pLineStartIndex[curStartNum];
            MPI_Send(&curLength, 1, MPI_INT, i, 1, MPI_COMM_WORLD);
            MPI_Send(pszFileBuffer + pLineStartIndex[curStartNum], curLength, MPI_CHAR, i, 2, MPI_COMM_WORLD);
            //print(pszFileBuffer, pLineStartIndex[curStartNum], curLength);
        }
    } else {
        // We are not the thread that read the file.
        // We need to receive data from whichever thread 
        MPI_Status status;
        MPI_Recv(&totalChars, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);
        buffer = new char[totalChars];
        MPI_Recv(buffer, totalChars, MPI_CHAR, 0, 2, MPI_COMM_WORLD, &status);
        // Thread 0 is responsible for making sure the startNum and endNum calculated here are valid.
        // This is because thread 0 tells us exactly how many characters we were send.
    }

    // Do the search
    map<string, int> wordcount;

    string s = string(buffer);
    regex word_regex(reg);
    auto words_begin =
            sregex_iterator(s.begin(), s.end(), word_regex);
    auto words_end = sregex_iterator();
    for (sregex_iterator i = words_begin; i != words_end; ++i) {
        smatch match = *i;
        string match_str = match.str();
        wordcount[match_str]++;
    }


    int size = wordcount.size();
    WordStruct* words = new WordStruct[size];
    int i = 0;
    for (map<string, int>::iterator it = wordcount.begin(); it != wordcount.end(); it++) {
        strcpy(words[i].word, (it->first).c_str());
        words[i].count = it->second;
        i++;
    }


    // At this point, all threads need to communicate their results to thread 0.

    if (rank == 0) {
        // The master thread will need to receive all computations from all other threads.
        MPI_Status status;

        // MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status)
        // We need to go and receive the data from all other threads.
        // The arbitrary tag we choose is 1, for now.

        for (int i = 1; i < nTasks; i++) {
            int sz;
            MPI_Recv(&sz, 1, MPI_INT, i, 3, MPI_COMM_WORLD, &status);
            WordStruct* local_words = new WordStruct[sz];
            MPI_Recv(local_words, sz, obj_type, i, 4, MPI_COMM_WORLD, &status);

            for (int j = 0; j < sz; j++) {
                wordcount[local_words[j].word] += local_words[j].count;
            }
            delete local_words;
        }
    } else {
        // We are finished with the results in this thread, and need to send the data to thread 1.
        // The destination is thread 0, and the arbitrary tag we choose for now is 1.
        MPI_Send(&size, 1, MPI_INT, 0, 3, MPI_COMM_WORLD);
        MPI_Send(words, size, obj_type, 0, 4, MPI_COMM_WORLD);
        

    }

    if (rank == 0) {
        // Display the final calculated value
        ofstream out("output.txt");
        for (map<string, int>::iterator it = wordcount.begin(); it != wordcount.end(); it++) {
            out << it->first << ": " << it->second << "\n";
        }
        out.close();
        int endTime = MPI::Wtime();
        delete words;
        cout << "Time: " << endTime - startTime << " seconds" << endl;
        cout << "Time (no reads): " << endTime - startTime_noRead << " seconds";
    }

    MPI_Finalize();
    return 0;
}

void print(char* str, int start, int len) {
    cout << "@";
    for (int i = 0; i < len; i++) {
        if (str[start + i] == '\r')
            cout << "EOL";
        else if (str[start + i] == '\n')
            cout << "NL";
        else if (str[start + i] == '\0')
            cout << "ES";
        else
            cout << str[start + i];
    }
    cout << "@" << endl;
}
