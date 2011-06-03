noinst_PROGRAMS=hash_rot_sbox hash_rk hash_null hash_shift_add hash_shift_xor hash_rk_static hash_adler hash_rp_table hash_rot_sbox_pre
LDADD=-lstdc++

rk_vals.h: rk_gen
	./rk_gen > $@

SUFFIXES = .h

BUILT_SOURCES = rk_vals.h 

.h.o: hashbench.cc rk_vals.h
	$(CXX) $(CXXFLAGS) -I. -I$(srcdir) $(srcdir)/hashbench.cc -c -o $@ -DHASHFN=$*
	
# Add to clean to remove rk_vals.h
