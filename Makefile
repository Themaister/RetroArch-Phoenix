
OS = $(shell uname -a)
ifneq ($(findstring Darwin,$(OS)),)
include Makefile.osx
else 
ifneq ($(findstring Linux,$(OS)),)
include Makefile.gtk
else
include Makefile.win
endif
endif
