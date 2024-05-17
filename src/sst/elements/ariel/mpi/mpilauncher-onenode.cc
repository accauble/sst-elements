#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <string>
#include <array>
#include <algorithm>
#include <cstring>
#include <cassert>
#include <cerrno>

/*
 *  SLURM-specific MPI launcher for Ariel simulations
 *  Ariel forks this process which initiates mpirun
 *
 *  If one rank is found in the MPI allocation, all ranks
 *  will run there. If multiple ranks are found, a single rank
 *  will run on the node with SST, and the remaining
 *  ranks will be distributed on the other nodes.
 */
int main(int argc, char *argv[]) {

    if (argc < 4 || std::string(argv[1]).compare("-H") == 0) {
        std::cout << "Usage: " << argv[0] << " <nprocs> <tracerank> <pin-binary> [pin args] -- <program-binary> [program args]\n";
        exit(1);
    }

    std::array<char, 128> buffer;

    // Get node that SST is running on.
    // In the "onenode" version of mpilauncher, all processes
    // will run on the same node.
    gethostname(buffer.data(), 128);
    /*
    std::string pin_host = buffer.data();
    size_t pos = pin_host.find('.');
    std::string pin_host = pin_host.substr(0,pos);
    */
    std::string host = buffer.data();

    int procs = atoi(argv[1]);
    int tracerank = atoi(argv[2]);

    if (procs < 1) {
        printf("Error: %s: <nprocs> must be positive\n", argv[0]);
        exit(1);
    }

    if (tracerank < 0 || tracerank >= procs) {
        printf("Error: %s: <tracerank> must be in [0,nprocs)\n", argv[0]);
        exit(1);
    }

    // In order to trace the appropriate rank, determine how many
    // should launch before the traced rank, and how many should launch after
    int ranks_before = tracerank;
    int ranks_after = procs - tracerank - 1;
    if (ranks_after < 0) {
        ranks_after = 0;
    }

    // `pinstring` will contain the command to launch pin and all of its arguments
    // `binary` will contain the traced program and all of its arguments
    std::string pinstring = "";
    std::string binary = "";
    bool getbinary = false;
    std::string arg;
    for (int i = 3; i < argc; i++) {
        arg = argv[i];

        // Pin string
        pinstring += arg;
        pinstring += " ";

        // Binary string
        if (getbinary) {
            binary += arg;
            binary += " ";
        }

        if (arg == "--")
            getbinary = true;
    }

    // Build the mpirun command
    std::string mpicmd = "mpirun --oversubscribe";

    if (ranks_before > 0) {
        mpicmd += " -H ";
        mpicmd += host;
        mpicmd += " -np ";
        mpicmd += std::to_string(ranks_before);
        mpicmd += " ";
        mpicmd += binary;
        mpicmd += " : ";
    }

    mpicmd += " -H ";
    mpicmd += host;
    mpicmd += " -np ";
    mpicmd += std::to_string(1);
    mpicmd += " ";
    mpicmd += pinstring;

    if (ranks_after > 0) {
        mpicmd += " : -H ";
        mpicmd += host;
        mpicmd += " -np ";
        mpicmd += std::to_string(ranks_after);
        mpicmd += " ";
        mpicmd += binary;
    }

    int use_system = 0;
    if (use_system) {
        printf("Wrapper starting...\n");
        printf("Arg to system: %s\n", mpicmd.c_str());
        system(mpicmd.c_str());
        printf("Wrapper complete...\n");
    } else {
        // Use execve to make sure that the child processes exits when killed by SST
        // I am lazily assuming that there are no spaces in any of the arguments.

        // Get a mutable copy
        char * cmd_copy = new char[mpicmd.length() + 1];
        std::strcpy(cmd_copy, mpicmd.c_str());

        // Calculate an upper bound for the number of arguments
        const int MAX_ARGS = std::strlen(cmd_copy) / 2 + 2;

        // Allocate memory for the pointers
        char** argv = new char*[MAX_ARGS];
        for (int i = 0;i < MAX_ARGS; i++) {
            argv[i] = NULL;
        }

        // Temporary variable to hold each word
        char* word;

        // Counter for the number of words
        int argc = 0;

        // Use strtok to split the string by spaces
        word = std::strtok(cmd_copy, " ");
        while (word != nullptr) {
            // Allocate memory for the word and copy it
            argv[argc] = new char[std::strlen(word) + 1];
            std::strcpy(argv[argc], word);

            // Move to the next word
            word = std::strtok(nullptr, " ");
            argc++;
        }

        assert(argv[argc] == NULL);

        printf("MPI Command: %s\n", mpicmd.c_str());
        int ret = execvp(argv[0], argv);
        printf("Error: mpilauncher-onenode.cc: This should be unreachable. execvp error: %d, %s\n", errno, strerror(errno));
        exit(1);
    }
}
