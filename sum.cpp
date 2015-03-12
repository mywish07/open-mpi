#include "sum.h"
#include <iostream>
#include "mpi.h"
#include <cmath>
#include <gmp.h>

using namespace std;


int Sum::sum(int argc, char* argv[]) {
    int rank, size;

    MPI::Init(argc, argv);
    size = MPI::COMM_WORLD.Get_size();
    rank = MPI::COMM_WORLD.Get_rank();

    int startTime = MPI::Wtime();

    mpz_t max;
    if (argc == 3) {
        mpz_init_set_str(max, argv[2], 10);
    } else {
        mpz_init_set_str(max, "1000", 10);
    }
    
    mpz_t nTasks, pSize, start, end, sum;
    mpz_init_set_ui(nTasks, size); // nTasks = size;
    mpz_init(end);

    mpz_init(pSize);
    mpz_cdiv_q(pSize, max, nTasks); // pSize = max / nTasks

    mpz_mul_si(start, pSize, rank); // start = pSize * rank

    if (rank == size - 1) {
        mpz_set(end, max);
    } else {
        mpz_mul_si(end, pSize, rank + 1); // end = pSize * (rank + 1)
        mpz_sub_ui(end, end, 1); // end = end + 1;
    }


    mpz_init(sum);
    mpz_t i;
    mpz_init_set(i, start);
    while (mpz_cmp(i, end) <= 0) {
        mpz_add(sum, sum, i); // sum = sum + i
        mpz_add_ui(i, i, 1);
    }


    // At this point, all threads need to communicate their results to thread 0.
    if (rank == 0) {
        // The master thread will need to receive all computations from all other threads.
        MPI_Status status;
       for (int i = 1; i < size; i++) {
            int sz;
            MPI_Recv(&sz, 1, MPI_INT, i, 1, MPI_COMM_WORLD, &status);
            char* value = new char[sz];
            MPI_Recv(value, sz + 1, MPI_CHAR, i, 2, MPI_COMM_WORLD, &status);
            mpz_t pSum;
            mpz_init_set_str(pSum, value, 10);

            mpz_add(sum, sum, pSum); // sum += pSum
        }

        int endTime = MPI::Wtime();
        char* num = new char[mpz_sizeinbase(sum, 10) + 1];
        mpz_get_str(num, 10, sum);
        cout << num << endl;
        cout << "Time: " << endTime - startTime << " seconds";
    } else {
        // The destination is thread 0.
        char* t;
        mpz_get_str(t, 10, sum);
        int len = strlen(t);
        MPI_Send(&len, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
        MPI_Send(t, len + 1, MPI_CHAR, 0, 2, MPI_COMM_WORLD);
    }
    MPI::Finalize();
    return 0;
}