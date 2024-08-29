#include "Math/gfp.h"
#include "Math/gf2n.h"
#include "Networking/sockets.h"
#include "Networking/ssl_sockets.h"
#include "Tools/int.h"
#include "Math/Setup.h"
#include "Protocols/fake-stuff.h"

#include "Math/gfp.hpp"
#include "Client.hpp"

#include <sodium.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <random>

void fill_random_values(std::vector<std::vector<int>>& matrix, int n, int dim, int min_val = 1, int max_val = 100)
{
    std::cout << "Filling random values in the matrix: " << n << " x " << dim << std::endl;
    std::mt19937 rng(std::time(nullptr));
    std::uniform_int_distribution<int> dist(min_val, max_val);
    matrix.resize(n, std::vector<int>(dim));

    for (int i = 0; i < n; ++i)
    {
        for (int d = 0; d < dim; ++d)
        {
            matrix[i][d] = dist(rng);
        }
    }

    std::cout << "Matrix filled with random values:" << std::endl;
    for (const auto& row : matrix)
    {
        for (int val : row)
        {
            std::cout << val << " ";
        }
        std::cout << std::endl;
    }
}

template<class T, class U>
void run(const std::vector<std::vector<int>>& n_points, size_t k_size, Client& client)
{
    std::cout << "Running MPC computation with " << n_points.size() << " data points and " << k_size << " centroids." << std::endl;

    // Send each row of the matrix as a separate input to the MPC cluster
    for (const auto& row : n_points)
    {
        std::vector<T> converted_row;
        converted_row.reserve(row.size());
        for (int value : row)
        {
            converted_row.push_back(static_cast<T>(value));
        }
        std::cout << "Sending row: ";
        for (const T& val : converted_row)
        {
            std::cout << val << " ";
        }
        std::cout << std::endl;

        client.send_private_inputs<T>(converted_row);
    }

    // Receive the centroids back from the MPC cluster
    // std::cout << "Receiving centroids from the MPC cluster..." << std::endl;
    // std::vector<U> centroids = client.receive_outputs<T>(k_size * n_points[0].size());

    // std::cout << "The k centroids are: " << std::endl;
    // for (size_t i = 0; i < k_size; ++i)
    // {
    //     for (size_t j = 0; j < n_points[0].size(); ++j)
    //     {
    //         std::cout << centroids[i * n_points[0].size() + j] << " ";
    //     }
    //     std::cout << std::endl;
    // }
}

int main(int argc, char** argv)
{
    std::cout << "Starting kmeans-client..." << std::endl;

    size_t client_id;
    int num_mpc_parties;
    size_t n_size;
    size_t dim;
    size_t k_size;
    size_t finish;
    std::vector<std::vector<int>> n_points;
    int port_base = 14000;

    if (argc < 7) {
        std::cout << "Usage is kmeans-client <client identifier> <number of spdz parties> "
                  << "<number of n data points> <dimensions of data points> <number of total k centroids> "
                  << "<finish (0 false; 1 true)> <optional host names> <optional port base>" << std::endl;
        exit(0);
    }

    client_id = std::atoi(argv[1]);
    num_mpc_parties = std::atoi(argv[2]);
    n_size = std::atoi(argv[3]);
    dim = std::atoi(argv[4]);
    k_size = std::atoi(argv[5]);
    finish = std::atoi(argv[6]);
    std::vector<std::string> hostnames(num_mpc_parties, "localhost");

    std::cout << "Client ID: " << client_id << std::endl;
    std::cout << "Number of parties: " << num_mpc_parties << std::endl;
    std::cout << "Data points (n_size): " << n_size << std::endl;
    std::cout << "Dimensions (dim): " << dim << std::endl;
    std::cout << "Number of centroids (k_size): " << k_size << std::endl;
    std::cout << "Finish flag: " << finish << std::endl;

    fill_random_values(n_points, n_size, dim);

    if (argc > 7)
    {
        if (argc < 7 + num_mpc_parties)
        {
            std::cerr << "Not enough hostnames specified; Must specify a host for each MP-SPDZ party." << std::endl;
            exit(1);
        }

        for (int i = 0; i < num_mpc_parties; i++) {
            hostnames[i] = argv[7 + i];
            std::cout << "Hostname for party " << i << ": " << hostnames[i] << std::endl;
        }
    }

    if (argc > 7 + num_mpc_parties)
    {
        port_base = std::atoi(argv[7 + num_mpc_parties]);
        std::cout << "Port base set to: " << port_base << std::endl;
    }

    bigint::init_thread();
    std::cout << "Initialized bigint threading." << std::endl;

    // Setup connections from this client to each MP-SPDZ party socket
    std::cout << "Setting up client connections..." << std::endl;
    Client client(hostnames, port_base, client_id);
    octetStream& specification = client.specification;
    std::vector<client_socket*>& sockets = client.sockets;

    // Send finish flag in the clear
    std::cout << "Sending finish flag..." << std::endl;
    for (int i = 0; i < num_mpc_parties; i++)
    {
        octetStream os;
        os.store(finish);
        os.Send(sockets[i]);
    }

    std::cout << "Finished setting up socket connections to MP-SPDZ engines." << std::endl;

    char type = specification.get<int>();
    std::cout << "Computation type received: " << char(type) << std::endl;

    switch (type)
    {
        case 'p':
        {
            gfp::init_field(specification.get<bigint>());
            std::cerr << "Using prime " << gfp::pr() << std::endl;
            run<gfp, gfp>(n_points, k_size, client);
            break;
        }
        case 'R':
        {
            int R = specification.get<int>();
            int R2 = specification.get<int>();
            std::cout << "Ring parameters: R = " << R << ", R2 = " << R2 << std::endl;
            if (R2 != 64)
            {
                std::cerr << R2 << "-bit ring not implemented" << std::endl;
                exit(1);
            }

            switch (R)
            {
                case 64:
                    run<Z2<64>, Z2<64>>(n_points, k_size, client);
                    break;
                case 104:
                    run<Z2<104>, Z2<104>>(n_points, k_size, client);
                    break;
                case 128:
                    run<Z2<128>, Z2<128>>(n_points, k_size, client);
                    break;
                default:
                    std::cerr << R << "-bit ring not implemented" << std::endl;
                    exit(1);
            }
            break;
        }
        default:
            std::cerr << "Type " << type << " not implemented" << std::endl;
            exit(1);
    }

    std::cout << "Kmeans client completed successfully." << std::endl;
    return 0;
}