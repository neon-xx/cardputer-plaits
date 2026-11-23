# Native build of Plaits + synth core for testing on a PC (not the firmware).
ROOT := ..
MI   := $(ROOT)/lib/mutable/src
SRCS := host_test.cpp $(ROOT)/src/synth/synth.cpp $(ROOT)/src/synth/params.cpp \
        $(shell find $(MI) -name '*.cc')
CXXFLAGS := -O2 -g -fsingle-precision-constant -I$(MI) -I$(ROOT)/src -Wall -Wno-unused-variable -Wno-unused-local-typedefs
OBJDIR := build
OBJS := $(patsubst %,$(OBJDIR)/%.o,$(subst /,_,$(subst ../,,$(SRCS))))

host_test: $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $@

clean:
	rm -f host_test demo.wav
