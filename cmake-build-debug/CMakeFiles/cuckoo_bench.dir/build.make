# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.16

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/zhj/SB-KV2.0

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/zhj/SB-KV2.0/cmake-build-debug

# Include any dependencies generated for this target.
include CMakeFiles/cuckoo_bench.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/cuckoo_bench.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/cuckoo_bench.dir/flags.make

CMakeFiles/cuckoo_bench.dir/bench/cuckoo_bench.cc.o: CMakeFiles/cuckoo_bench.dir/flags.make
CMakeFiles/cuckoo_bench.dir/bench/cuckoo_bench.cc.o: ../bench/cuckoo_bench.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/zhj/SB-KV2.0/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/cuckoo_bench.dir/bench/cuckoo_bench.cc.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/cuckoo_bench.dir/bench/cuckoo_bench.cc.o -c /home/zhj/SB-KV2.0/bench/cuckoo_bench.cc

CMakeFiles/cuckoo_bench.dir/bench/cuckoo_bench.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/cuckoo_bench.dir/bench/cuckoo_bench.cc.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/zhj/SB-KV2.0/bench/cuckoo_bench.cc > CMakeFiles/cuckoo_bench.dir/bench/cuckoo_bench.cc.i

CMakeFiles/cuckoo_bench.dir/bench/cuckoo_bench.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/cuckoo_bench.dir/bench/cuckoo_bench.cc.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/zhj/SB-KV2.0/bench/cuckoo_bench.cc -o CMakeFiles/cuckoo_bench.dir/bench/cuckoo_bench.cc.s

# Object files for target cuckoo_bench
cuckoo_bench_OBJECTS = \
"CMakeFiles/cuckoo_bench.dir/bench/cuckoo_bench.cc.o"

# External object files for target cuckoo_bench
cuckoo_bench_EXTERNAL_OBJECTS =

cuckoo_bench: CMakeFiles/cuckoo_bench.dir/bench/cuckoo_bench.cc.o
cuckoo_bench: CMakeFiles/cuckoo_bench.dir/build.make
cuckoo_bench: libleveldbd.a
cuckoo_bench: /usr/lib/x86_64-linux-gnu/libpthread.so
cuckoo_bench: /usr/local/lib/libpmem.so
cuckoo_bench: /usr/local/lib/libpmemcto.so
cuckoo_bench: /usr/local/lib/libpmemobj.so
cuckoo_bench: /usr/local/lib/libpmemlog.so
cuckoo_bench: /usr/local/lib/libvmem.so
cuckoo_bench: CMakeFiles/cuckoo_bench.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/zhj/SB-KV2.0/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable cuckoo_bench"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/cuckoo_bench.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/cuckoo_bench.dir/build: cuckoo_bench

.PHONY : CMakeFiles/cuckoo_bench.dir/build

CMakeFiles/cuckoo_bench.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/cuckoo_bench.dir/cmake_clean.cmake
.PHONY : CMakeFiles/cuckoo_bench.dir/clean

CMakeFiles/cuckoo_bench.dir/depend:
	cd /home/zhj/SB-KV2.0/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/zhj/SB-KV2.0 /home/zhj/SB-KV2.0 /home/zhj/SB-KV2.0/cmake-build-debug /home/zhj/SB-KV2.0/cmake-build-debug /home/zhj/SB-KV2.0/cmake-build-debug/CMakeFiles/cuckoo_bench.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/cuckoo_bench.dir/depend

