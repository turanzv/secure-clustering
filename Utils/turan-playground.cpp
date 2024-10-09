#include "Math/gfp.hpp"
#include "Machines/SPDZ2k.hpp"
#include "Machines/MalRep.hpp"
#include "Protocols/ProtocolSet.h"
#include "Protocols/Rep3Share.h"
#include "Networking/CryptoPlayer.h"
#include "Protocols/Rep3Shuffler.h"

#include <iostream>
#include <random>

#ifndef DEBUG_NETWORKING
#define DEBUG_NETWORKING
#endif

#define NO_MIXED_CIRCUITS

// Helper function to convert a 2D matrix to a StackedVector<T>
template <class T>
void convert_matrix_to_stacked_vector(T** matrix, int rows, int cols, StackedVector<T>& stacked_vector) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            stacked_vector.push_back(matrix[i][j]);
        }
    }
}

/**
 * Fills a matrix with random values.
 *
 * @param matrix The matrix to be filled with random values.
 * @param n The number of rows in the matrix.
 * @param dim The number of columns (dimensions) in the matrix.
 * @param client_id A unique ID for the client (used to seed the RNG).
 * @param min_val The minimum value for the random numbers (default: 1).
 * @param max_val The maximum value for the random numbers (default: 100).
 */
void fill_random_values(int** matrix, int n, int dim, int client_id, int min_val = 1, int max_val = 100)
{
    std::cout << "Filling random values in the matrix: " << n << " x " << dim << std::endl;
    
    // Seed the random number generator with the client_id
    std::mt19937 rng(std::time(nullptr) + client_id);
    std::uniform_int_distribution<int> dist(min_val, max_val);

    // Fill the matrix with random values
    for (int i = 0; i < n; ++i)
    {
        for (int d = 0; d < dim; ++d)
        {
            matrix[i][d] = dist(rng);
        }
    }

    // Print the filled matrix for debugging purposes
    std::cout << "Matrix filled with random values:" << std::endl;
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < dim; j++)
        {
            std::cout << matrix[i][j] << " ";
        }
        std::cout << std::endl;
    }
}

/**
 * Runs the secure protocol with secret sharing.
 *
 * @tparam T The secret share type (e.g., Rep3Share).
 * @param argv Command-line arguments.
 * @param prime_length The length of the prime field.
 */
template<class T>
void run(char** argv, int prime_length)
{
    int my_num = atoi(argv[1]);         // Party number
    int num_parties = atoi(argv[2]);    // Total number of parties
    int port_base = 14000;              // Default port base

    // Setup network communication
    Names NN(my_num, num_parties, "localhost", port_base);
    CryptoPlayer P(NN);

    // Initialize the protocol setup and set of protocols
    ProtocolSetup<T> setup(P, prime_length);
    ProtocolSet<T> set(P, setup);
    auto& protocol = set.protocol;
    auto& input = set.input;
    auto& output = set.output;
    auto& subprocessor = set.processor;

    //(void)output;  // Suppress unused variable warning
    (void)protocol;  // Suppress unused variable warning

    int n_size = 99;       // Size of N matrix
    int k_size = 12;       // Size of K matrix
    int my_n_size = n_size / 3;  // Size of local N matrix for each party
    int my_k_size = k_size / 3;  // Size of local K matrix for each party
    int dim = 3;           // Dimension of each vector (3D coordinates)

    // Dynamically allocate matrices
    int** my_N = new int*[dim];
    for (int i = 0; i < dim; i++)
        my_N[i] = new int[my_n_size];

    int** my_K = new int*[dim];
    for (int i = 0; i < dim; i++)
        my_K[i] = new int[my_k_size];

    // Fill matrices with random values
    fill_random_values(my_N, dim, my_n_size, my_num);
    fill_random_values(my_K, dim, my_k_size, my_num);

    // Allocate secret-shared matrices
    T** N = new T*[dim];
    for (int i = 0; i < dim; i++)
        N[i] = new T[n_size];

    T** K = new T*[dim];
    for (int i = 0; i < dim; i++)
        K[i] = new T[k_size];

    std::cout << "Parties have set up." << std::endl;

    // Reset the input buffer for sending secret shares
    input.reset_all(P);

    std::cout << "Reset complete." << std::endl;

    // Add local N and K matrices to input and prepare to send to all parties
    for (int i = 0; i < dim; i++) {
        for (int j = 0; j < my_n_size; j++) {
            input.add_from_all(my_N[i][j]);  // Add N values for secret sharing
        }
    }

    for (int i = 0; i < dim; i++) {
        for (int j = 0; j < my_k_size; j++) {
            input.add_from_all(my_K[i][j]);  // Add K values for secret sharing
        }
    }

    // Exchange secret shares between parties
    input.exchange();

    // Finalize and reconstruct secret shares for N matrix
    for (int i = 0; i < dim; i++) {
        for (int j = 0; j < my_n_size; j++) {
            N[i][j] = input.finalize(0);                  // Finalize share from party 0
            N[i][j + my_n_size] = input.finalize(1);      // Finalize share from party 1
            N[i][j + (2 * my_n_size)] = input.finalize(2);  // Finalize share from party 2
        }
    }

    // Finalize and reconstruct secret shares for N matrix
    for (int i = 0; i < dim; i++) {
        for (int j = 0; j < my_k_size; j++) {
            K[i][j] = input.finalize(0);                  // Finalize share from party 0
            K[i][j + my_k_size] = input.finalize(1);      // Finalize share from party 1
            K[i][j + (2 * my_k_size)] = input.finalize(2);  // Finalize share from party 2
        }
    }

    for (int i {0}; i < dim; i++) {
        for (int j {0}; j < k_size; j++) {
            std::cout <<  K[i][j] << std::endl;
        }
    }

    // Convert N to StackedVector<T> for shuffling
    StackedVector<T> stacked_N;
    convert_matrix_to_stacked_vector(N, dim, n_size, stacked_N);

    typename Replicated<T>::Shuffler shuffler(subprocessor);
    typename Replicated<T>::Shuffler::store_type store;

    int handle = shuffler.generate(10, store);
    const auto& shuffle = store.get(handle);


    for (size_t i = 0; i < shuffle.size(); i++) {
        for (size_t j = 0; j < shuffle[i].size(); j++) {
            std::cout << shuffle[i][j] << " ";
        }
        std::cout << endl;
    }

    vector<typename T::open_type> i;
    vector<T> k;
    k.push_back(K[0][0]);
    output.POpen(i, k, P);
    cout << i[0] << endl;

    output.exchange(P);

    for (int i = 0; i < dim; i++)
        delete[] my_N[i];
    delete[] my_N;

    for (int i = 0; i < dim; i++)
        delete[] my_K[i];
    delete[] my_K;

    for (int i = 0; i < dim; i++)
        delete[] N[i];
    delete[] N;

    for (int i = 0; i < dim; i++)
        delete[] K[i];
    delete[] K;
}

int main(int argc, char** argv)
{
    const int prime_length = 128;
    const int n_limbs = (prime_length + 63) / 64;

    // Check if the required arguments are passed
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] 
                  << " <my number (0/1/...)> <total number of parties> <optional: port base>" 
                  << std::endl;
        return 1;
    }

    int port_base = 14000; // Default port base
    if (argc == 4) {
        port_base = atoi(argv[3]); // Override port base if provided
    }

    // Initialize and run the protocol
    run<Rep3Share<gfp_<0, n_limbs>>>(argv, prime_length);

    return 0;
}