#include <math.h> 
#include "mpi.h" 
#include <iostream>
#include "pi.h"
#include "HantonSequence.h"

using namespace std;


int Pi::run(int argc, char *argv[]) {
    int size, rank;
    long totalin, totalout;
    long num_point = 100000;
    if (argc == 3)
        num_point = atol(argv[2]);
    MPI::Init(argc, argv);
    size = MPI::COMM_WORLD.Get_size();
    rank = MPI::COMM_WORLD.Get_rank();

    double startTime = MPI::Wtime();
    long index = rank * num_point;
    HaltonSequence h(index);
    long numInside = 0, numOutside = 0;
    for (long i = 0; i < num_point; i++) {
        double* point = h.nextPoint();
        //count points inside/outside of the inscribed circle of the square
        double x = point[0] - 0.5; // normalize [0,1] -> [-0.5,0.5]
        double y = point[1] - 0.5;
        if (x * x + y * y > 0.25)
            numOutside++;
        else
            numInside++;
    }

    MPI_Reduce(&numInside, &totalin, 1, MPI_LONG, MPI_SUM, 0,
            MPI_COMM_WORLD);
    MPI_Reduce(&numOutside, &totalout, 1, MPI_LONG, MPI_SUM, 0,
            MPI_COMM_WORLD);
    if (rank == 0) {
        printf("Estimated Pi: %2.30f \n %ld %ld\n", 4.0 * totalin / (totalin + totalout), totalin, totalout);
        cout << "Total time: " << MPI::Wtime() - startTime << " seconds\n";
    }
    MPI_Finalize();
    return 0; 
}

