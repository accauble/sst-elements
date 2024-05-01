
#include <sst_config.h>

#include "epafrontend.h"

#include <signal.h>
#if !defined(SST_COMPILE_MACOSX)
#include <sys/prctl.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <fcntl.h>

#include <time.h>

#include <string.h>

#define ARIEL_INNER_STRINGIZE(input) #input
#define ARIEL_STRINGIZE(input) ARIEL_INNER_STRINGIZE(input)

using namespace SST::ArielComponent;

EPAFrontend::EPAFrontend(ComponentId_t id, Params& params, uint32_t cores, uint32_t maxCoreQueueLen, uint32_t defMemPool) : ArielFrontend(id, params, cores, maxCoreQueueLen, defMemPool) {

    // Parse arguments
    int verbosity = params.find<int>("verbose", 0);
    output = new SST::Output("EPAFrontend[@f:@l:@p] ", verbosity, 0, SST::Output::STDOUT);

    core_count = cores;

    //////////////////////////////////////////////////////////////////////////
    

    // Parse executable name
    executable = params.find<std::string>("executable", "");
    if ("" == executable) {
        output->fatal(CALL_INFO, -1, "The input deck did not specify an executable to be run\n");
    }
    // Add ".addstrinst" to the executable name (this is the instrumented
    // app name)
    executable.append(".addstrinst");

    // Parse redirect info
    epa_redirect_info.stdin_file = params.find<std::string>("appstdin", "");
    epa_redirect_info.stdout_file = params.find<std::string>("appstdout", "");
    epa_redirect_info.stderr_file = params.find<std::string>("appstderr", "");
    epa_redirect_info.stdoutappend = params.find<std::uint32_t>("appstdoutappend", "0");
    epa_redirect_info.stderrappend = params.find<std::uint32_t>("appstderrappend", "0");

    // Parse application arguments
    uint32_t app_argc = (uint32_t) params.find<uint32_t>("appargcount", 0);
    output->verbose(CALL_INFO, 1, 0, "Model specifies that there are %" PRIu32 " application arguments\n", app_argc);


    // Create shared memory region
    tunnelmgr = new SST::Core::Interprocess::SHMParent<ArielTunnel>(id, core_count, maxCoreQueueLen);

    std::string shmem_region_name = tunnelmgr->getRegionName();
    tunnel = tunnelmgr->getTunnel();
    output->verbose(CALL_INFO, 1, 0, "Base pipe name: %s\n", shmem_region_name.c_str());

    // Put together execution command
    execute_args = (char**) malloc(sizeof(char*) * (app_argc));
    output->verbose(CALL_INFO, 1, 0, "Processing application arguments...\n");

    uint32_t arg = 0;
    execute_args[arg++] = (char*) malloc(sizeof(char) * (executable.size() + 2));
    strcpy(execute_args[arg-1], executable.c_str());

    char* argv_buffer = (char*) malloc(sizeof(char) * 256);
    for(uint32_t aa = 0; aa < app_argc ; ++aa) {
        snprintf(argv_buffer, sizeof(char)*256, "apparg%" PRIu32, aa);
        std::string argv_i = params.find<std::string>(argv_buffer, "");

        output->verbose(CALL_INFO, 1, 0, "Found application argument %" PRIu32 " (%s) = %s\n",
                aa, argv_buffer, argv_i.c_str());
        execute_args[arg] = (char*) malloc(sizeof(char) * (argv_i.size() + 1));
        strcpy(execute_args[arg], argv_i.c_str());
        arg++;
    }
    execute_args[arg] = NULL;
    free(argv_buffer);

    output->verbose(CALL_INFO, 1, 0, "Completed processing application arguments.\n");

    // Remember that the list of arguments must be NULL terminated for execution
    execute_args[app_argc] = NULL;

    output->verbose(CALL_INFO, 1, 0, "Completed initialization of the Ariel CPU.\n");
    fflush(stdout);
}

void EPAFrontend::init(unsigned int phase)
{
    if (phase == 0) {
        // Init the child_pid = 0, this prevents problems in emergencyShutdown()
        // if forkChild() calls fatal (i.e. the child_pid would not be set)
        child_pid = 0;
        child_pid = forkChild(executable.c_str(), execute_args, execute_env, epa_redirect_info);
        output->verbose(CALL_INFO, 1, 0, "Waiting for child to attach.\n");
        tunnel->waitForChild();
        output->verbose(CALL_INFO, 1, 0, "Child has attached!\n");
    }
}

void EPAFrontend::finish() {
    // If the simulation ended early, e.g. by using --stop-at, the child
    // may still be executing. It will become a zombie if we do not
    // kill it.
    if (child_pid != 0) {
        kill(child_pid, SIGKILL);
    }
}

ArielTunnel* EPAFrontend::getTunnel() {
    return tunnel;
}

int EPAFrontend::forkChild(const char* app, char** args, std::map<std::string, std::string>& app_env, epa_redirect_info_t epa_redirect_info) {
    // If user only wants to init the simulation then we do NOT fork the binary
    if(isSimulationRunModeInit())
        return 0;

    int next_arg_index = 0;
    int next_line_index = 0;

    char* full_execute_line = (char*) malloc(sizeof(char) * 16384);

    memset(full_execute_line, 0, sizeof(char) * 16384);
// TODO Why is no command appearing?
    while (NULL != args[next_arg_index]) {
        int copy_char_index = 0;

        if (0 != next_line_index) {
            full_execute_line[next_line_index++] = ' ';
        }

        while ('\0' != args[next_arg_index][copy_char_index]) {
            full_execute_line[next_line_index++] = args[next_arg_index][copy_char_index++];
        }

        next_arg_index++;
    }

    full_execute_line[next_line_index] = '\0';

    output->verbose(CALL_INFO, 1, 0, "Executing command: %s\n", full_execute_line);
    free(full_execute_line);


    // Fork this binary, then exec to get around waiting for
    // child to exit.
    pid_t the_child;
    the_child = fork();

    // If the fork failed
    if (the_child < 0) {
        perror("fork");
        output->fatal(CALL_INFO, 1, "Fork failed to launch the traced process. errno = %d, errstr = %s\n", errno, strerror(errno));
    }

    // If this is the parent, return the PID of our child process
    if(the_child != 0) {
        // Set the member variable child_pid in case the waitpid() below fails
        // this allows the fatal process to kill the process and prevent it
        // from becoming a zombie process.  Because as we all know, zombies are
        // bad and eat your brains...
        child_pid = the_child;

        // Wait a second, and check to see that the child actually started
        sleep(1);
        int pstat;
        pid_t check = waitpid(the_child, &pstat, WNOHANG);
        // If the child process is Stopped or Terminated
        if (check > 0) {
            // There are 3 possible results
            // If the child exitted
            if (WIFEXITED(pstat) == true) {
                output->fatal(CALL_INFO, 1, "Launching trace child failed!  Child Exited with status %d\n", WEXITSTATUS(pstat));
            // If the child had an error
            } else if (WIFSIGNALED(pstat) == true) {
                output->fatal(CALL_INFO, 1, "Launching trace child failed!  Child Terminated With Signal %d; Core Dump File Created = %d\n", WTERMSIG(pstat), WCOREDUMP(pstat));
            // If the child stopped with an error
            } else if (WIFSTOPPED(pstat) == true) {
                output->fatal(CALL_INFO, 1, "Launching trace child failed!  Child Stopped with Signal  %d\n", WSTOPSIG(pstat));
            // If there was some other error
            } else {
                output->fatal(CALL_INFO, 1, "Launching trace child failed!  Unknown Problem; pstat = %d\n", pstat);
            }
        // If waitpid returned an error
        } else if (check < 0) {
            perror("waitpid");
            output->fatal(CALL_INFO, 1, "Waitpid returned an error, errno = %d.  Did the child ever even start?\n", errno);
        }
        // Return the PID of the child process
        return (int) the_child;

    // If this is the child
    } else {
        //Do I/O redirection before exec
        if ("" != epa_redirect_info.stdin_file) {
            output->verbose(CALL_INFO, 1, 0, "Redirecting child stdin from file %s\n", epa_redirect_info.stdin_file.c_str());
            if (!freopen(epa_redirect_info.stdin_file.c_str(), "r", stdin)) {
                output->fatal(CALL_INFO, 1, 0, "Failed to redirect stdin\n");
            }
        }
        if ("" != epa_redirect_info.stdout_file) {
            output->verbose(CALL_INFO, 1, 0, "Redirecting child stdout to file %s\n", epa_redirect_info.stdout_file.c_str());
            std::string mode = "w+";
            if (epa_redirect_info.stdoutappend) {
                mode = "a+";
            }
            if (!freopen(epa_redirect_info.stdout_file.c_str(), mode.c_str(), stdout)) {
                output->fatal(CALL_INFO, 1, 0, "Failed to redirect stdout\n");
            }
        }
        if ("" != epa_redirect_info.stderr_file) {
            output->verbose(CALL_INFO, 1, 0, "Redirecting child stderr from file %s\n", epa_redirect_info.stderr_file.c_str());
            std::string mode = "w+";
            if (epa_redirect_info.stderrappend) {
                mode = "a+";
            }
            if (!freopen(epa_redirect_info.stderr_file.c_str(), mode.c_str(), stderr)) {
                output->fatal(CALL_INFO, 1, 0, "Failed to redirect stderr\n");
            }
        }

        output->verbose(CALL_INFO, 1, 0, "Launching executable: %s...\n", app);

#if defined(SST_COMPILE_MACOSX)
#else
#if defined(HAVE_SET_PTRACER)
        prctl(PR_SET_PTRACER, getppid(), 0, 0 ,0);
#endif // End of HAVE_SET_PTRACER
#endif // End SST_COMPILE_MACOSX (else branch)

        std::string shmem_region_name = tunnelmgr->getRegionName();
        // Pass the shared memory location to EPA library
        setenv("METASIM_CACHE_SIMULATION", "0", 1);
        setenv("METASIM_ARIEL_FRONTEND", "1", 1);
        setenv("METASIM_SST_SHMEM", shmem_region_name.c_str(), 1);
        int ret_code = execvp(app, args);
        perror("execve");

        output->verbose(CALL_INFO, 1, 0,
            "Call to execvp returned: %d\n", ret_code);

        output->fatal(CALL_INFO, -1,
            "Error executing: %s under an EPA fork\n",
            app);
    }

    return 0;
}

EPAFrontend::~EPAFrontend() {
    // Everything loaded by calls to the core are deleted by the core
    // (subcomponents, component extension, etc.)
    delete tunnelmgr;
}

void EPAFrontend::emergencyShutdown() {
    // If child_pid = 0, dont kill (this would kill all processes of the group)
    if (child_pid != 0) {
        kill(child_pid, SIGKILL);
    }

    delete tunnelmgr; // Clean up tmp file
}
