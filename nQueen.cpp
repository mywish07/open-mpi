#include <mpi.h>

#include "nQueen.hpp"
#include <queue>
#include <fstream>
#include <string>

#define DATA 1
#define BOARD_SIZE 14

const int REQUEST = -1, NO_MORE_WORK = -1;
const int NEW = -2;

using namespace std;

typedef struct {
    int size;
    int* board;
} iboard;

void push(queue<iboard>& q, int len, int* board) {
    iboard b;
    b.size = len;
    b.board = board;
    q.push(b);
}

bool NQueens::safe(int len, int board[], int row) {
    int currentColumn = len;
    for (int i = 1; i <= currentColumn; i++) {
        int preRow = board[currentColumn - i];
        if (preRow == row || preRow == row - i || preRow == row + i)
            return false;
    }
    return true;
}

bool NQueens::safeAtColumn(int* board, int column) {
    int row = board[column];
    for (int i = 1; i <= column; i++) {
        int preRow = board[column - i];
        if (preRow == row || preRow == row - i || preRow == row + i)
            return false;
    }
    return true;

}
/**
 * Master piece
 */
void NQueens::nqueen_master(int nWorker, long& total) {
    int msg, workerid, size;
    int* board;
    MPI_Status status;
    queue<iboard> workQueue;
    queue<int> freeWorker;
    push(workQueue, 0, NULL);
    long unsigned int remaining;
    bool listen = true;

    //breadth first search for first 5 columns
    while (listen) {
        MPI_Recv(&msg, 1, MPI_INT, MPI_ANY_SOURCE, DATA, MPI_COMM_WORLD, &status);
        workerid = status.MPI_SOURCE;

        if (msg == REQUEST) {
            remaining = workQueue.size();
            if (remaining > 0) {
                iboard b = workQueue.front();
                MPI_Send((void*) &NEW, 1, MPI_INT, workerid, DATA, MPI_COMM_WORLD);
                MPI_Send(&b.size, 1, MPI_INT, workerid, DATA, MPI_COMM_WORLD);
                if (b.size > 0)
                    MPI_Send(b.board, b.size, MPI_INT, workerid, DATA, MPI_COMM_WORLD);
                workQueue.pop();
                delete []b.board;
            } else {               
                // store free worker, then assign work for it later
                freeWorker.push(workerid);
                if (freeWorker.size() == nWorker) // all workers are free
                {
                    listen = false;
                }
            }

        } else if (msg == NEW) {
            
            MPI_Recv(&size, 1, MPI_INT, workerid, DATA, MPI_COMM_WORLD, MPI_STATUSES_IGNORE);
            board = new int[size];
            MPI_Recv(board, size, MPI_INT, workerid, DATA, MPI_COMM_WORLD, MPI_STATUSES_IGNORE);

            if (size < board_size) {
                // send this new work to free worker
                if (freeWorker.size() > 0) {
                    int id = freeWorker.front();
                    MPI_Send((void*) &NEW, 1, MPI_INT, id, DATA, MPI_COMM_WORLD);
                    MPI_Send(&size, 1, MPI_INT, id, DATA, MPI_COMM_WORLD);
                    MPI_Send(board, size, MPI_INT, id, DATA, MPI_COMM_WORLD);
                    delete []board;
                    freeWorker.pop();
                } else { // store work and assign when requested
                    iboard b;
                    b.size = size;
                    b.board = board;
                    workQueue.push(b);
                }
            } 
        }
    }

    
    for (int i = 1; i <= nWorker; i++) {
        // stop signal to worker
        MPI_Send((void *) &NO_MORE_WORK, 1, MPI_INT, i, DATA, MPI_COMM_WORLD);
    }

    long count = 0;
    MPI_Reduce(&count, &total, 1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);   
}

void NQueens::nqueen_worker(int rank, long& total) {
    int msg, size;
    long count = 0;
    int* board;
    string name("nqueen");
    name.append(to_string(rank));
    name.append(".txt");
    ofstream out(name);
    double s = MPI::Wtime();
    double t = 0.0;
    while (true) {
        // send work request
        MPI_Send((void*) &REQUEST, 1, MPI_INT, 0, DATA, MPI_COMM_WORLD);

        
        // receive work
        MPI_Recv(&msg, 1, MPI_INT, 0, DATA, MPI_COMM_WORLD, MPI_STATUSES_IGNORE);

        if (msg == NO_MORE_WORK) {
            break;
        }

        int newSize;
        MPI_Recv(&size, 1, MPI_INT, 0, DATA, MPI_COMM_WORLD, MPI_STATUSES_IGNORE);
        if (size < 5)
            newSize = size + 1;
        else newSize = board_size;
        
        board = new int[newSize];
        if (size > 0)
            MPI_Recv(board, size, MPI_INT, 0, DATA, MPI_COMM_WORLD, MPI_STATUSES_IGNORE);

        //add a new queen, breadth first search
        if (size < 5) {
            for (int row = 0; row < board_size; row++) {
                if (safe(size, board, row)) {
                    board[size] = row;
                    if (newSize == board_size) { // found complete solution
                        if (store > 0) {
                            for (int i = 0; i < newSize - 1; i++) {
                                out << board[i] << "-";
                            }
                            out << board[size] << "\n";
                        }
                        count++;
                    } else {
                        MPI_Send((void*) &NEW, 1, MPI_INT, 0, DATA, MPI_COMM_WORLD);
                        MPI_Send(&newSize, 1, MPI_INT, 0, DATA, MPI_COMM_WORLD);
                        MPI_Send(board, newSize, MPI_INT, 0, DATA, MPI_COMM_WORLD);
                    }
                }
            }
        } else {// depth first search: find the complete board
            
            int column = size; // column 'size' + 1
            board[column] = -1;
            while (column >= size) {
                int row = -1;
                double s2 = MPI::Wtime();
                do {
                    row = board[column] = board[column] + 1;
                } while (row < board_size && !safeAtColumn(board, column));
                t += (MPI::Wtime() - s2);
                if (row < board_size) {
                    if (column < board_size - 1) {
                        board[++column] = -1;
                    } else { // found the board
                        //MPI_Send((void*) &NEW, 1, MPI_INT, 0, DATA, MPI_COMM_WORLD);
                        //MPI_Send(&newSize, 1, MPI_INT, 0, DATA, MPI_COMM_WORLD);
                        //MPI_Send(board, newSize, MPI_INT, 0, DATA, MPI_COMM_WORLD);
                        
                        if (store > 0) {
                            for (int i = 0; i < newSize - 1; i++) {
                                out << board[i] << "-";
                            }
                            out << board[newSize - 1] << "\n";
                        } 
                        count++;
                    }
                } else {
                    column--;
                }
            }
            
        }
        delete []board;
    }
    //out.close();
    cout << "slave " << rank << " " << t << "count = " << count << "\n";
    //cout << "Total time of slave " << rank << " " << MPI::Wtime() - s << " seconds\n";
    MPI_Reduce(&count, &total, 1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);   
}

int NQueens::run(int argc, char* argv[]) {
    board_size = BOARD_SIZE;
    store = 1; // store data
    int size, rank;
    long total;
    MPI::Init(argc, argv);
    size = MPI::COMM_WORLD.Get_size();
    rank = MPI::COMM_WORLD.Get_rank();

    if (argc >= 3)
        board_size = atol(argv[2]);
    if (argc == 4)
        store = atol(argv[3]);

    double startTime = MPI::Wtime();
     
    if (rank == 0) {
        nqueen_master(size - 1, total);
    } else { 
        nqueen_worker(rank, total);
    }

    if (rank == 0) {
        cout << "Number of solutions: " << total << "\n";
        cout << "Total time: " << MPI::Wtime() - startTime << " seconds\n";
    }
    MPI::Finalize();
    return 0;
}

