#
# 'make depend' uses makedepend to automatically generate dependencies 
#               (dependencies are added to end of Makefile)
# 'make'        build executable file 'mycc'
# 'make clean'  removes all .o and executable files
#

# define the C compiler to use
CXX ?= gcc

# define any compile-time flags
CFLAGS = -Wall -Wextra -g -w -std=c++11

# define any directories containing header files other than /usr/include
#
INCLUDES = -I inc/  -I ../security_module/inc/

BUILD_PATH = build/obj

# define any libraries to link into executable:
#   if I want to link in libraries (libx.so or libx.a) I use the -llibname 
#   option, something like (this will link in libmylib.so and libm.so:
LIBS = -pthread -ljson-c

# define the C source files
SRCS = src/can2eth.c src/eth2can.c src/main.c ../security_module/src/aes.c ../security_module/src/cmac.c ../security_module/src/encrypt.c ../security_module/src/utils.c

# define the C object files 
#
# This uses Suffix Replacement within a macro:
#   $(name:string1=string2)
#         For each word in 'name' replace 'string1' with 'string2'
# Below we are replacing the suffix .c of all words in the macro SRCS
# with the .o suffix
#
OBJS = $(SRCS:.c=.o)

# define the executable file 
MAIN = gateway_non_critical 

#
# The following part of the makefile is generic; it can be used to 
# build any executable just by changing the definitions above and by
# deleting dependencies appended to the file from 'make depend'
#

.PHONY: depend clean

#all:    $(MAIN)
#	@echo  Simple compiler named mycc has been compiled

$(MAIN): $(OBJS) 
	$(CXX) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LIBS)
#	mv src/*.o ./build/obj/

# this is a suffix replacement rule for building .o's from .c's
# it uses automatic variables $<: the name of the prerequisite of
# the rule(a .c file) and $@: the name of the target of the rule (a .o file) 
# (see the gnu make manual section about automatic variables)
.c.o:
	$(CXX) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) ./build/obj/*.o *~ $(MAIN)

depend: $(SRCS)
	makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it

