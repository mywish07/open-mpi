/* 
 * File:   nQueen.hpp
 * Author: novpla
 *
 * Created on February 27, 2015, 1:59 PM
 */

#ifndef NQUEEN_HPP
#define	NQUEEN_HPP

class NQueens {
private:
    bool safe(int len , int board[], int row);
    bool safeAtColumn (int* board, int column); 
    void nqueen_master(int nWorker, long& total);
    void nqueen_worker(int rank, long& total);
    int board_size;
    int store;
public:
    int run(int argc, char* argv[]);
};

#endif	/* NQUEEN_HPP */

