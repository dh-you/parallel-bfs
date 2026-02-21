#include <mpi.h>
#include <iostream>
#include <cstdlib>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int points_per_rank = (1e9-1) / (size - 1); 
    int points = points_per_rank * (size - 1);
    int num_circle = 0;

    std::pair<float, float> circle_coord = {0.25, 0.25};
    float radius = 0.25;

    if (rank == 0) {
        for (int i = 1; i < size; i++) {
            int in_circle;
            MPI_Recv(&in_circle, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            num_circle += in_circle;
        }
    } else {
        std::srand(time(0) + rank);

        for (int i = 0; i < points_per_rank; i++) {
            float x = static_cast<float>(std::rand()) / RAND_MAX * 0.5f;
            float y = static_cast<float>(std::rand()) / RAND_MAX * 0.5f;
                
            float dx = x - circle_coord.first;
            float dy = y - circle_coord.second;
            float dist_squared = dx*dx + dy*dy;
            if (dist_squared <= radius*radius) num_circle++;        
        }

        MPI_Send(&num_circle, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }

    if (rank == 0) {
        std::cout << 4 * static_cast<double>(num_circle) / points << std::endl;
    }

    MPI_Finalize();
    return 0;
}
