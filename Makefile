
OS = $(shell uname -a)
ifneq ($(findstring Darwin,$(OS)),)
include Makefile.osx
else 
ifneq ($(findstring Linux,$(OS)),)
include Makefile.qt
else
include Makefile.win32
endif
endif
