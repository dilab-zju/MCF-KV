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
include CMakeFiles/bloom_bench.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/bloom_bench.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/bloom_bench.dir/flags.make

CMakeFiles/bloom_bench.dir/bench/bloom_bench.cc.o: CMakeFiles/bloom_bench.dir/flags.make
CMakeFiles/bloom_bench.dir/bench/bloom_bench.cc.o: ../bench/bloom_bench.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/zhj/SB-KV2.0/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/bloom_bench.dir/bench/bloom_bench.cc.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/bloom_bench.dir/bench/bloom_bench.cc.o -c /home/zhj/SB-KV2.0/bench/bloom_bench.cc

CMakeFiles/bloom_bench.dir/bench/bloom_bench.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/bloom_bench.dir/bench/bloom_bench.cc.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/zhj/SB-KV2.0/bench/bloom_bench.cc > CMakeFiles/bloom_bench.dir/bench/bloom_bench.cc.i

CMakeFiles/bloom_bench.dir/bench/bloom_bench.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/bloom_bench.dir/bench/bloom_bench.cc.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/zhj/SB-KV2.0/bench/bloom_bench.cc -o CMakeFiles/bloom_bench.dir/bench/bloom_bench.cc.s

# Object files for target bloom_bench
bloom_bench_OBJECTS = \
"CMakeFiles/bloom_bench.dir/bench/bloom_bench.cc.o"

# External object files for target bloom_bench
bloom_bench_EXTERNAL_OBJECTS =

bloom_bench: CMakeFiles/bloom_bench.dir/bench/bloom_bench.cc.o
bloom_bench: CMakeFiles/bloom_bench.dir/build.make
bloom_bench: libleveldbd.a
bloom_bench: /usr/lib/x86_64-linux-gnu/libpthread.so
bloom_bench: /usr/local/lib/libpmem.so
bloom_bench: /usr/local/lib/libpmemcto.so
bloom_bench: /usr/local/lib/libpmemobj.so
bloom_bench: /usr/local/lib/libpmemlog.so
bloom_bench: /usr/local/lib/libvmem.so
bloom_bench: CMakeFiles/bloom_bench.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/zhj/SB-KV2.0/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable bloom_bench"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/bloom_bench.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/bloom_bench.dir/build: bloom_bench

.PHONY : CMakeFiles/bloom_bench.dir/build

CMakeFiles/bloom_bench.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/bloom_bench.dir/cmake_clean.cmake
.PHONY : CMakeFiles/bloom_bench.dir/clean

CMakeFiles/bloom_bench.dir/depend:
	cd /home/zhj/SB-KV2.0/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/zhj/SB-KV2.0 /home/zhj/SB-KV2.0 /home/zhj/SB-KV2.0/cmake-build-debug /home/zhj/SB-KV2.0/cmake-build-debug /home/zhj/SB-KV2.0/cmake-build-debug/CMakeFiles/bloom_bench.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/bloom_bench.dir/depend

