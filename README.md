# Find primes in given range
The program finds all the prime numbers in the given range with number of processes and number of threads in each process specified by the user. It can be used as a simple example for instantiating processes, threads, communicating between processes and interprocess mutex locks.

## Quick Start
Clone the repository to local and compile the code with `make`.
Then you can call `./a.out <range start> <range end> <process number> <thread number>`.

For example, `./a.out 1 100 2 4` will find the prime numbers in the range [1, 100] with 2 processes and 4 threads in each process.
