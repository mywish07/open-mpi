#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "pi.h"
#include "wordcount.h"
#include "grep.h"
#include "sum.h"
#include "HantonSequence.h"
#include "nQueen.hpp"
#include "bfs.hpp"
#include <iostream>
#include <fstream>

using namespace std;

/*
 * My first OpenMPI program
 */
int main(int argc, char* argv[]) {

    if (argc > 1) {
        int sl = atoi(argv[1]);
        switch (sl) {
            case 1:
            {
                WordCount c;
                c.count(argc, argv);
            }
                break;
            case 2:
            {
                Grep g;
                g.grep(argc, argv);
            }
                break;
            case 3: Sum::sum(argc, argv);
                break;
            case 4:
                NQueens q;
                q.run(argc, argv);
                break;
            case 5:
                BFS bfs;
                bfs.run(argc, argv);
                break;
            default:
                Pi pi;
                pi.run(argc, argv);
                break;
        }
    } else {
        HaltonSequence h(1);
        cout << h.nextPoint()[1];
    }
}

