


CXXFLAGS=-std=c++14 -Wall -O0 -ggdb 

all: worker scheduler apply
clean:
	rm -f worker
	rm -f scheduler
	rm -f apply
	rm -f *.deps
	rm -f worker.pgm

-include worker.deps
worker : worker.cpp 
	g++ $(CXXFLAGS) -o worker worker.cpp -MMD -MF worker.deps -MT worker -lpthread

-include scheduler.deps
scheduler : scheduler.cpp 
	g++ $(CXXFLAGS) -o scheduler scheduler.cpp -MMD -MF scheduler.deps -MT scheduler -lpthread

-include apply.deps
apply : apply.cpp 
	g++ $(CXXFLAGS) -o apply apply.cpp -MMD -MF apply.deps -MT apply


