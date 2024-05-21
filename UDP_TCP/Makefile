COMPILERFLAGS = -g -Wall -Wextra -Wno-sign-compare
LINKLIBS = -lpthread
SERVEROBJECTS = obj/receiver_main.o
CLIENTOBJECTS = obj/sender_main.o

.PHONY: all clean

all : obj reliable_sender reliable_receiver

reliable_receiver: $(SERVEROBJECTS)
	$(CXX) $(COMPILERFLAGS) $^ -o $@ $(LINKLIBS)

reliable_sender: $(CLIENTOBJECTS)
	$(CXX) $(COMPILERFLAGS) $^ -o $@ $(LINKLIBS)

clean :
	$(RM) obj/*.o reliable_sender reliable_receiver

obj/%.o: src/%.cpp
	$(CXX) $(COMPILERFLAGS) -c -o $@ $<

obj:
	mkdir -p obj
