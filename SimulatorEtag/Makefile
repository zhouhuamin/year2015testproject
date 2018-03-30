CC=g++
CFLAGS= -c -g -pthread -D_REENTRANT -D`uname -s` -Wall
LDFLAGS= -g -lpthread
target = SimulatorEtag
all:	$(target)
target_objects=SimulatorEtag.o ./tinyxml/tinystr.o ./tinyxml/tinyxml.o ./tinyxml/tinyxmlerror.o ./tinyxml/tinyxmlparser.o
SimulatorEtag	:$(target_objects)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo  $@ Build OK.
.SUFFIXES:.cpp .o
.cpp.o :
	$(CC) $(CFLAGS) -o $@ $<	

clean:
	@rm -rf $(target)  $(target_objects)
