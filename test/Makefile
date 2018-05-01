


#CXXFLAGS=-std=c++14 -Wall -Werror -O3 -Wno-noexcept-type
CXXFLAGS=-std=c++14 -Wall -Werror -O0 -ggdb -Wno-noexcept-type

all: worker scheduler apply scheduler_1thread
clean:
	rm -f worker
	rm -f scheduler
	rm -f apply
	rm -f scheduler_1thread
	rm -f *.deps
	rm -f worker.pgm

-include worker.deps
worker : worker.cpp 
	g++ $(CXXFLAGS) -o worker worker.cpp -MMD -MF worker.deps -MT worker -lpthread

-include scheduler.deps
scheduler : scheduler.cpp 
	g++ $(CXXFLAGS) -o scheduler scheduler.cpp -MMD -MF scheduler.deps -MT scheduler -lpthread

-include scheduler_1thread.deps
scheduler_1thread : scheduler_1thread.cpp 
	g++ $(CXXFLAGS) -o scheduler_1thread scheduler_1thread.cpp -MMD -MF scheduler_1thread.deps -MT scheduler_1thread 

-include apply.deps
apply : apply.cpp 
	g++ $(CXXFLAGS) -o apply apply.cpp -MMD -MF apply.deps -MT apply


