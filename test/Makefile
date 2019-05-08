


#CXXFLAGS=-std=c++14 -Wall -Werror -O3 -Wno-noexcept-type
CXXFLAGS=-std=c++14 -Wall -Werror -O0 -ggdb -Wno-noexcept-type

all: worker scheduler apply scheduler_1thread future_test defer shared_function linear_map
clean:
	rm -f worker
	rm -f scheduler
	rm -f apply
	rm -f scheduler_1thread
	rm -f *.deps
	rm -f worker.pgm
	rm -f future_test
	rm -f defer
	rm -f linear_map
	rm -f shared_function

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

-include future_test.deps
future_test : future_test.cpp 
	g++ $(CXXFLAGS) -o future_test future_test.cpp -MMD -MF future_test.deps -MT future_test -lpthread


-include defer.deps
defer : defer.cpp 
	g++ $(CXXFLAGS) -o defer defer.cpp -MMD -MF defer.deps -MT defer 

-include linear_map.deps
linear_map : linear_map.cpp 
	g++ $(CXXFLAGS) -o linear_map linear_map.cpp -MMD -MF linear_map.deps -MT linear_map 

-include shared_function.deps
shared_function : shared_function.cpp 
	g++ $(CXXFLAGS) -o shared_function shared_function.cpp -MMD -MF shared_function.deps -MT shared_function -lpthread  

