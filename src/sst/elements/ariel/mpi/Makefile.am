all-local: mpilauncher epa-mpilauncher

mpilauncher: mpilauncher.cc
	 $(ARIEL_MPICXX) $(AM_CFLAGS) mpilauncher.cc -o mpilauncher
fakepin: fakepin.cc
	 $(ARIEL_MPICXX) fakepin.cc -o fakepin
epa-mpilauncher: epa-mpilauncher.cc
	 $(ARIEL_MPICXX) $(AM_CFLAGS) epa-mpilauncher.cc -o epa-mpilauncher

bin_SCRIPTS = mpilauncher epa-mpilauncher

clean-local:
	rm -f mpilauncher fakepin epa-mpilauncher
