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

// Helper function to convert a 2D matrix to a StackedVector<T>: stacked_vector.push_back(matrix[i][j]);
template <class T>
void convert_matrix_to_stacked_vector(T** matrix, int rows, int cols, StackedVector<T>& stacked_vector) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            stacked_vector.push_back(matrix[i][j]);
        }
    }
}

// Helper function to convert a StackedVector<T> to a 2D matrix: matrix[i][j] = stacked_vector[(i*j)+j];
template <class T>
void convert_stacked_vector_to_matrix(T** matrix, int rows, int cols, StackedVector<T>& stacked_vector) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            matrix[i][j] = stacked_vector[(i*j)+j];
        }
    }
}

// Helper function to print N and K
template<class T>
void print_matrix(T** matrix, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            cout << matrix[i][j] <<  endl;
        }
    }
}

// Helper function to describe the shuffling object
void print_shuffler(array<vector<int>, 2> shuffle) {
    std::cout << "shuffle size: " << shuffle.size() << endl;
    std::cout << "shuffle[0] size: " << shuffle[0].size() << endl;
    std::cout << "shuffle[1] size: " << shuffle[1].size() << endl;
    for (size_t i = 0; i < shuffle.size(); i++) {
        std::cout << "shuffle[] " << i << endl;
        for (size_t j = 0; j < shuffle[i].size(); j++) {
            std::cout << "shuffle[][] " << j << endl;
            std::cout << shuffle[i][j] << " " << endl;
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

    /**
     * STEP 0: GENERATE INPUTS LOCALLY
     */

    // Dynamically allocate matrices
    int** my_N = new int*[my_n_size];
    for (int i = 0; i < my_n_size; i++)
        my_N[i] = new int[dim];

    int** my_K = new int*[my_k_size];
    for (int i = 0; i < my_k_size; i++)
        my_K[i] = new int[dim];

    // Fill matrices with random values
    fill_random_values(my_N, my_n_size, dim, my_num);
    fill_random_values(my_K, my_k_size, dim, my_num);

    // Allocate secret-shared matrices
    T** N = new T*[n_size];
    for (int i = 0; i < n_size; i++)
        N[i] = new T[dim];

    T** K = new T*[k_size];
    for (int i = 0; i < k_size; i++)
        K[i] = new T[dim];

    std::cout << "Parties have completed local set up." << std::endl;

    /**
     * STEP 1: SHARE AND SHUFFLE INPUTS
     */

    // Reset the input buffer for sending secret shares
    input.reset_all(P);

    // Add local N and K matrices to input and prepare to send to all parties
    for (int i = 0; i < my_n_size; i++) {
        for (int j = 0; j < dim; j++) {
            input.add_from_all(my_N[i][j]);  // Add N values for secret sharing
        }
    }

    for (int i = 0; i < my_k_size; i++) {
        for (int j = 0; j < dim; j++) {
            input.add_from_all(my_K[i][j]);  // Add K values for secret sharing
        }
    }

    // Exchange secret shares between parties
    input.exchange();

    // Finalize and reconstruct secret shares for N matrix
    for (int i = 0; i < my_n_size; i++) {
        for (int j = 0; j < dim; j++) {
            N[i][j] = input.finalize(0);                  // Finalize share from party 0
            N[i + my_n_size][j] = input.finalize(1);      // Finalize share from party 1
            N[i + (2 * my_n_size)][j] = input.finalize(2);  // Finalize share from party 2
        }
    }

    // Finalize and reconstruct secret shares for K matrix
    for (int i = 0; i < my_k_size; i++) {
        for (int j = 0; j < dim; j++) {
            K[i][j] = input.finalize(0);                  // Finalize share from party 0
            K[i + my_k_size][j] = input.finalize(1);      // Finalize share from party 1
            K[i + (2 * my_k_size)][j] = input.finalize(2);  // Finalize share from party 2
        }
    }

    for (int i {0}; i < k_size; i++) {
        for (int j {0}; j < dim; j++) {
            std::cout <<  K[i][j] << std::endl;
        }
    }

    // Convert N to StackedVector<T> for shuffling
    StackedVector<T> stacked_K;
    convert_matrix_to_stacked_vector(K, k_size, dim, stacked_K);
    int shuffle_size = k_size * 3;

    // Instantiate the shuffler
    typename Replicated<T>::Shuffler shuffler(subprocessor);
    typename Replicated<T>::Shuffler::store_type store;

    // Get the permutation
    int handle = shuffler.generate(k_size, store);
    const typename Rep3Shuffler<T>::shuffle_type& shuffle = store.get(handle);

    print_shuffler(shuffle);

    // Apply the shuffle to the arrays
    cout << stacked_K.size() << endl;
    shuffler.apply(stacked_K, shuffle_size, 3, 0, 0, store.get(handle), false);

    // Return to the non-stacked array
    convert_stacked_vector_to_matrix(K, k_size, dim, stacked_K);

    print_matrix(K, k_size, dim);

    /**
     * STEP 2: CREATE THE KD-TREE
     */
    // This is an insecure stop-gap
    // Reveal K to all parties
    vector<typename T::open_type> K_clear;
    vector<T> K_secret;
    for (int i = 0; i < k_size; i++) {
        for (int j = 0; j < dim; j++) {
            K_secret.push_back(K[i][j]);
        }
    }
    output.POpen(K_clear, K_secret, P);
    output.exchange(P);

    // Sort K according to a dimension
    // std::sort(std::begin(arr), std::end(arr), [](const int a[dim], const int b[dim]){return a[1] < a[1]});
    // Pick median

    // Double down

    /**
     * STEP 3: CLUSTER (TRAVERSE THE KD-TREE)
     * 
     * Requirement: Parties must own a secret share of kd-tree and secret shares of datapoints. 
     * Rashmi to add SISOPIR here. 
     * Rashmi to add a function to compute LessThanGarbledCircuit - Yao modification.  
     */

    /**
     * STEP 4: RECALCULATE CENTROIDS
     */

    /**
     * SETP 5: REVEAL OUTPUTS TO PARTIES
     */

    // test revealing a type: reveal K[0][1], Party 0's first input
    vector<typename T::open_type> i;
    vector<T> k;
    k.push_back(K[0][0]);
    output.POpen(i, k, P);
    cout << i[0] << endl;

    output.exchange(P);

    cout << i[0] << endl;
    /**
     * Now to sort all data points and make a thing 
     */

    for (int i = 0; i < my_n_size; i++)
        delete[] my_N[i];
    delete[] my_N;

    for (int i = 0; i < my_k_size; i++)
        delete[] my_K[i];
    delete[] my_K;

    for (int i = 0; i < n_size; i++)
        delete[] N[i];
    delete[] N;

    for (int i = 0; i < k_size; i++)
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