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

/**
 * Fills a matrix with random values.
 *
 * @param matrix The matrix to be filled with random values.
 * @param n The number of rows in the matrix.
 * @param dim The number of columns (dimensions) in the matrix.
 * @param min_val The minimum value for the random numbers (default: 1).
 * @param max_val The maximum value for the random numbers (default: 100).
 */
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

    // Print the filled matrix for debugging purposes
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

/**
 * Runs the MPC computation by sending data points (N and K) to the MPC cluster.
 *
 * @tparam T The type used for sending inputs.
 * @tparam U The type used for receiving outputs.
 * @param n_points The matrix containing N data points.
 * @param k_points The matrix containing K centroids.
 * @param client The Client object used to communicate with the MPC cluster.
 */
template<class T, class U>
void run(const std::vector<std::vector<int>>& n_points, const std::vector<std::vector<int>>& k_points, Client& client)
{
    std::cout << "Running MPC computation with " << n_points.size() << " data points and " << k_points.size() << " centroids." << std::endl;
    
    // Send N data points to the MPC cluster
    std::cout << "Sending N" << std::endl;
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

    // Send K centroids to the MPC cluster
    std::cout << "Sending K" << std::endl;
    for (const auto& row : k_points)
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

    // Receiving centroids code (commented out) 
    // Uncomment and adjust when ready to receive centroids from the MPC cluster
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

    // Initialize variables
    size_t client_id;
    int num_mpc_parties;
    size_t n_size;
    size_t dim;
    size_t k_size;
    size_t finish;
    std::vector<std::vector<int>> n_points;
    std::vector<std::vector<int>> k_points;
    int port_base = 14000;

    // Parse command-line arguments
    if (argc < 7) {
        std::cout << "Usage: kmeans-client <client identifier> <number of spdz parties> "
                  << "<number of n data points> <dimensions of data points> <number of total k centroids> "
                  << "<finish (0 false; 1 true)> <optional host names> <optional port base>" << std::endl;
        exit(0);
    }

    // Extract and display input arguments
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

    // Fill matrices with random values
    fill_random_values(n_points, n_size, dim);
    fill_random_values(k_points, k_size, dim);

    // Handle optional hostnames and port base
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

    // Initialize multi-threading for bigint computations
    bigint::init_thread();
    std::cout << "Initialized bigint threading." << std::endl;

    // Setup connections from this client to each MP-SPDZ party socket
    std::cout << "Setting up client connections..." << std::endl;
    Client client(hostnames, port_base, client_id);
    octetStream& specification = client.specification;
    std::vector<client_socket*>& sockets = client.sockets;

    // Send finish flag to all parties
    std::cout << "Sending finish flag..." << std::endl;
    for (int i = 0; i < num_mpc_parties; i++)
    {
        octetStream os;
        os.store(finish);
        os.Send(sockets[i]);
    }

    std::cout << "Finished setting up socket connections to MP-SPDZ engines." << std::endl;

    // Determine computation type and run the appropriate MPC process
    char type = specification.get<int>();
    std::cout << "Computation type received: " << char(type) << std::endl;

    switch (type)
    {
        // Case for prime field computation
        case 'p':
        {
            // Initialize the field with the provided prime
            gfp::init_field(specification.get<bigint>());
            std::cerr << "Using prime " << gfp::pr() << std::endl;

            // Run the computation using the prime field
            run<gfp, gfp>(n_points, k_points, client);
            break;
        }

        // Case for ring-based computation
        case 'R':
        {
            // Get the ring parameters from the specification
            int R = specification.get<int>();
            int R2 = specification.get<int>();
            std::cout << "Ring parameters: R = " << R << ", R2 = " << R2 << std::endl;

            // Ensure the second ring parameter is 64, as this is the only supported size
            if (R2 != 64)
            {
                std::cerr << R2 << "-bit ring not implemented" << std::endl;
                exit(1);
            }

            // Nested switch to handle the different ring sizes
            switch (R)
            {
                case 64:
                    // Run the computation using a 64-bit ring
                    run<Z2<64>, Z2<64>>(n_points, k_points, client);
                    break;
                case 104:
                    // Run the computation using a 104-bit ring
                    run<Z2<104>, Z2<104>>(n_points, k_points, client);
                    break;
                case 128:
                    // Run the computation using a 128-bit ring
                    run<Z2<128>, Z2<128>>(n_points, k_points, client);
                    break;
                default:
                    // Handle unsupported ring sizes
                    std::cerr << R << "-bit ring not implemented" << std::endl;
                    exit(1);
            }
            break;
        }

        // Default case for unsupported computation types
        default:
            std::cerr << "Type " << type << " not implemented" << std::endl;
            exit(1);
    }

    // Indicate successful completion of the k-means client
    std::cout << "Kmeans client completed successfully." << std::endl;
    return 0;
}