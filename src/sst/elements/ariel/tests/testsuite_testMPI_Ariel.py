# -*- coding: utf-8 -*-

from sst_unittest import *
from sst_unittest_support import *
import os
import inspect

################################################################################
# Code to support a single instance module initialize, must be called setUp method

module_init = 0
module_sema = threading.Semaphore()

def initializeTestModule_SingleInstance(class_inst):
    global module_init
    global module_sema

    module_sema.acquire()
    if module_init != 1:
        try:
            # Put your single instance Init Code Here
            class_inst._setup_ariel_test_files()
        except:
            pass
        module_init = 1
    module_sema.release()

################################################################################
################################################################################
################################################################################


class testcase_Ariel(SSTTestCase):

    def initializeClass(self, testName):
        super(type(self), self).initializeClass(testName)
        # Put test based setup code here. it is called before testing starts
        # NOTE: This method is called once for every test

    def setUp(self):
        super(type(self), self).setUp()
        initializeTestModule_SingleInstance(self)
        # Put test based setup code here. it is called once before every test

    def tearDown(self):
        # Put test based teardown code here. it is called once after every test
        super(type(self), self).tearDown()

#####
    def file_contains(self, filename, strings):
        with open(filename, 'r') as file:
            lines = file.readlines()
            for s in strings:
                self.assertTrue(s in lines, "Output {0} does not contain expected line {1}".format(filename, s))

    pin_loaded = testing_is_PIN_loaded()
    pin_error_msg = "Ariel: Requires PIN, but Env Var 'INTEL_PIN_DIRECTORY' is not found or path does not exist."

    @unittest.skipIf(not pin_loaded, pin_error_msg)
    @unittest.skipIf(testing_check_get_num_ranks() > 1, "Ariel: test_mpi skipped if ranks > 1 - Sandy Bridge test is incompatible with Multi-Rank.")
    testing_check_get_num_ranks():
    def test_mpi_01(self):
        self.ariel_Template(threads=1, ranks=1)

    @unittest.skipIf(not pin_loaded, pin_error_msg)
    @unittest.skipIf(testing_check_get_num_ranks() > 1, "Ariel: test_mpi skipped if ranks > 1 - Sandy Bridge test is incompatible with Multi-Rank.")
    testing_check_get_num_ranks():
    def test_mpi_02(self):
        self.ariel_Template(threads=1, ranks=2)

    @unittest.skipIf(not pin_loaded, pin_error_msg)
    @unittest.skipIf(testing_check_get_num_ranks() > 1, "Ariel: test_mpi skipped if ranks > 1 - Sandy Bridge test is incompatible with Multi-Rank.")
    @unittest.skipIf(host_os_is_osx(), "Ariel: Open MP is not supported on OSX.")
    testing_check_get_num_ranks():
    def test_mpi_0e(self):
        self.ariel_Template(threads=2, ranks=1)

    @unittest.skipIf(not pin_loaded, pin_error_msg)
    @unittest.skipIf(testing_check_get_num_ranks() > 1, "Ariel: test_mpi skipped if ranks > 1 - Sandy Bridge test is incompatible with Multi-Rank.")
    testing_check_get_num_ranks():
    def test_mpi_04(self):
        self.ariel_Template(threads=1, ranks=2, tracerank=1)

    @unittest.skipIf(not pin_loaded, pin_error_msg)
    @unittest.skipIf(testing_check_get_num_ranks() > 1, "Ariel: test_mpi skipped if ranks > 1 - Sandy Bridge test is incompatible with Multi-Rank.")
    @unittest.skipIf(host_os_is_osx(), "Ariel: Open MP is not supported on OSX.")
    testing_check_get_num_ranks():
    def test_mpi_05(self):
        self.ariel_Template(threads=2, ranks=3, tracerank=1)

    @unittest.skipIf(not pin_loaded, pin_error_msg)
    @unittest.skipIf(testing_check_get_num_ranks() > 1, "Ariel: test_mpi skipped if ranks > 1 - Sandy Bridge test is incompatible with Multi-Rank.")
    @unittest.skipIf(host_os_is_osx(), "Ariel: Open MP is not supported on OSX.")
    testing_check_get_num_ranks():
    def test_mpi_06(self):
        self.ariel_Template(threads=2, ranks=2)

        ##TODO add reduce tests

    def ariel_Template(self, threads, ranks, program="hello", tracerank=0, testtimeout=30):
        # Set the paths to the various directories
        testcase = inspect.stack()[1][3] # name the test after the calling function

        # Get the path to the test files
        test_path = self.get_testsuite_dir()
        outdir = self.get_test_output_run_dir()
        tmpdir = self.get_test_output_tmp_dir()

        # Set paths
        ArielElementDir = os.path.abspath("{0}/../".format(test_path))
        ArielElementAPIDir = "{0}/api".format(ArielElementDir)
        ArielElementTestMPIDir = "{0}/tests/testMPI".format(ArielElementDir)

        libpath = os.environ.get("LD_LIBRARY_PATH")
        if libpath:
            os.environ["LD_LIBRARY_PATH"] = ArielElementAPIDir + ":" + libpath
        else:
            os.environ["LD_LIBRARY_PATH"] = ArielElementAPIDir

        # Set the various file paths
        testDataFileName=("test_Ariel_{0}".format(testcase))

        sdlfile = "{0}/test-mpi.py".format(ArielElementTestMPIDir)
        outfile = "{0}/{1}.out".format(outdir, testDataFileName)
        errfile = "{0}/{1}.err".format(outdir, testDataFileName)
        mpioutfiles = "{0}/{1}.testfile".format(outdir, testDataFileName)
        program_output = f"{tmpdir}/ariel_testmpi_{testcase}.out""
        other_args = f'--model-options="-o {program_output} -r {ranks} -t {threads} -a {tracerank}"'

        log_debug("testcase = {0}".format(testcase))
        log_debug("sdl file = {0}".format(sdlfile))
        log_debug("ref file = {0}".format(reffile))
        log_debug("out file = {0}".format(outfile))
        log_debug("err file = {0}".format(errfile))

        # Run SST in the tests directory
        self.run_sst(sdlfile, outfile, errfile, set_cwd=ArielElementTestMPIDir,
                     mpi_out_files=mpioutfiles, timeout_sec=testtimeout, other_args=other_args)

        testing_remove_component_warning_from_file(outfile)

        # Each rank will have its own output file
        # We will examine all of them.

        # These programs are designed to output a separate file for each rank,
        # and append the rank id to the argument
        program_output_files = [f"{program_output}_{i}" for i in ranks]

        # Look for the word "FATAL" in the output file
        cmd = 'grep "FATAL" {0} '.format(outfile)
        grep_result = os.system(cmd) != 0
        self.assertTrue(grep_result, "Output file {0} contains the word 'FATAL'...".format(outfile))

        hello_string_traced = [f"Hello from rank {tracerank} of {ranks}, thread {i}! (Launched by pin)\n" for i in range(threads)]
        hello_string_normal = [f"Hello from rank {tracerank} of {ranks}, thread {i}!\n" for i in range(threads)]
        reduce_string = [f"Rank {rank} partial sum is {sum(range(int(rank*(1024/ranks)), int((rank+1)*(1024/ranks))))}, total sum is {sum(range(1024))}\n"]

        # Test for expected output
        for i in range(ranks):
            if program == "hello":
                if rank == tracerank:
                    self.file_contains(f'{program_output}_{i}', hello_string_traced)
                else:
                    self.file_contains(f'{program_output}_{i}', hello_string_normal)
            else:
                self.file_contains(f'{program_output}_{i}', reduce_string)

        # Test to make sure that each core did some work
        #TODO


#######################

    def _setup_ariel_test_files(self):
        # NOTE: This routine is called a single time at module startup, so it
        #       may have some redunant
        log_debug("_setup_ariel_test_files() Running")
        test_path = self.get_testsuite_dir()
        outdir = self.get_test_output_run_dir()

        # Set the paths to the various directories
        self.ArielElementDir = os.path.abspath("{0}/../".format(test_path))
        self.ArielElementTestMPIDir = "{0}/tests/testMPI".format(self.ArielElementDir)

        # Build the Ariel API library
        ArielApiDir = "{0}/api".format(self.ArielElementDir)
        cmd = "make"
        rtn0 = OSCommand(cmd, set_cwd=ArielApiDir).run()
        log_debug("Ariel api/libarielapi.so Make result = {0}; output =\n{1}".format(rtn0.result(), rtn0.output()))
        os.environ["ARIELAPI"] =  ArielApiDir

        # Build the test mpi programs
        cmd = "make"
        rtn1 = OSCommand(cmd, set_cwd=self.ArielElementTestMPIDir).run()
        log_debug("Ariel ariel/tests/testMPI make result = {1}; output =\n{2}".format(ArielElementTestMPIDir, rtn1.result(), rtn1.output()))

        # Check that everything compiled OK
        self.assertTrue(rtn0.result() == 0, "libarielapi failed to compile")
        self.assertTrue(rtn1.result() == 0, "mpi test binaries failed to compile")

