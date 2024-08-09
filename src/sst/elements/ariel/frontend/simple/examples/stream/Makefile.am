if SST_USE_ARIEL_MPI
CXX_LOCAL = $(ARIEL_MPICXX)
CFLAGS_LOCAL = -fopenmp $(ARIEL_MPI_CFLAGS)
else
CXX_LOCAL = $(CXX)
CFLAGS_LOCAL =
endif

API_DIR = ../../../../api

all-local: stream stream_mlm

clean-local:
	rm -f stream stream_mlm

stream:
	$(CXX_LOCAL) stream.c -o stream $(CFLAGS_LOCAL) -I$(API_DIR) -L$(API_DIR) -larielapi

stream_mlm:
	$(CXX_LOCAL) stream_malloc.c -o stream_mlm $(CFLAGS_LOCAL) -I$(API_DIR) -L$(API_DIR) -larielapi

print-compiler:
	@echo "compiler: $(CXX_LOCAL)"
	@echo "compiler: $(ARIEL_MPICXX)"

AM_DEFAULT_VERBOSITY=1
