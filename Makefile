#compiles code
COMPILER = g++

# compiler flags, options that are sent to compiler
# -fopenmp: enables openmp so that compiler will watch for #pragma omp directives
# -Iinclude: adds include/ folder directory to compiler's search path,
# 		so i dont have to do things like #include <include/glad/glad.h>
# -Wall: turns on all common compiler warnings
COMPILERFLAGS = -fopenmp -Iinclude -Wall

# my source files to be compiled
# main.cpp: driver file
# GLFW.cpp: my personal App function implementation file
# src/glad.c: in the src directory, need to compile glad.c for opengl
SOURCEFILES = main.cpp App.cpp include/src/glad.c

# linker flags, link the source files for the other libraries
# -Linclude/lib: look into the include/lib folder
# -lglfw3: in the lib folder, get the library for glfw
# -lopengl32: windows opengl library
# -lgdi32: windows gdi, pixel and format handler
# -luser32: windows user, used to get inputs
# -lshell32: windows shell functions (paths, program startup)
# -lkernel32: windows kernel functions (memory, threads, timing)
# -mwindows: if not looking for errors, uncomment the mwindows so the command prompt wont open
LINKERFLAGS = -Linclude/lib -lglfw3 -lopengl32 -lgdi32 -luser32 -lshell32 -lkernel32 #-mwindows 

#executible file name
EXENAME = -o field_sim

all:
	$(COMPILER) $(COMPILERFLAGS) $(SOURCEFILES) $(LINKERFLAGS) $(EXENAME)

clean:
	del field_sim.exe
