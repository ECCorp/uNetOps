
cx_tune = -O0
cx = g++
cx_flags = $(cx_) -I./include -Wall
all: obj/us_xdp_load.o test/us_xdp_load obj/ks_xsk.o
		obj/us_xdp_umem.o test/us_xdp_umem

test/us_xdp_umem.o: src/us_xdp_umem.cc
	$(cx) $(CXXFLAGS) -DTEST -g3 -o$@ -c $< -lbpf

test/us_xdp_umem: src/us_xdp_umem.cc obj/us_xdp_load.o
	$(CXX) $(CXXFLAGS) -g3 -DTEST -o$@ $^ -lbpf

test/us_xdp_load: src/us_xdp_load.cc
	$(CXX) $(CXXFLAGS) -DTEST -g3 -o$@ $< -lbpf

obj/us_xdp_load.o: src/us_xdp_load.cc
	$(CXX) $(CXXFLAGS) -g3 -o$@ -c $< -lbpf

src/ks_xsk.o: src/ks_xsk.c
	clang -Wall -O2 -o$@ -target bpf -c $<

clean:
	@rm obj/*.o test/*
