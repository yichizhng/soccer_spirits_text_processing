CC = g++
CXX = g++
CPPFLAGS = -MMD
CXXFLAGS = -O1 -std=c++11
SRCS = blah.cc
YOURMOM = personality_processing.cc

blah: $(SRCS:.cc=.o)

run: blah
	./blah > questions_processed.txt 2> errors.txt

clean:
	rm -f blah *.o *~

bleh: $(YOURMOM:.cc=.o)

.PHONY: clean run

-include $(SRCS:.cc=.d)
