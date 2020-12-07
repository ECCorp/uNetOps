CXX = g++
CXXFLAGS = -Wall -O3 -I./include
all: obj/us_xdp_load.o bin/us_xdp_load obj/ks_xsk.o \
		obj/us_xdp_umem.o bin/us_xdp_umem

obj/us_xdp_umem.o: u_xdp/us_xdp_umem.cc
	$(CXX) $(CXXFLAGS) -g3 -o$@ -c $< -lbpf

bin/us_xdp_umem: u_xdp/us_xdp_umem.cc obj/us_xdp_load.o
	$(CXX) $(CXXFLAGS) -g3 -DTEST -o$@ $^ -lbpf

obj/us_xdp_load.o: u_xdp/us_xdp_load.cc
	$(CXX) $(CXXFLAGS) -g3 -o$@ -c $< -lbpf

bin/us_xdp_load: u_xdp/us_xdp_load.cc
	$(CXX) $(CXXFLAGS) -DTEST -g3 -o$@ $< -lbpf

obj/ks_xsk.o: u_xdp/ks_xsk.c
	clang -Wall -O2 -o$@ -target bpf -c $<
