# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.10

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
CMAKE_SOURCE_DIR = /home/michal/Work/gitHub/Casper

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/michal/Work/gitHub/Casper

# Utility rule file for ExperimentalSubmit.

# Include the progress variables for this target.
include Dependencies/json-3.5.0/CMakeFiles/ExperimentalSubmit.dir/progress.make

Dependencies/json-3.5.0/CMakeFiles/ExperimentalSubmit:
	cd /home/michal/Work/gitHub/Casper/Dependencies/json-3.5.0 && /usr/bin/ctest -D ExperimentalSubmit

ExperimentalSubmit: Dependencies/json-3.5.0/CMakeFiles/ExperimentalSubmit
ExperimentalSubmit: Dependencies/json-3.5.0/CMakeFiles/ExperimentalSubmit.dir/build.make

.PHONY : ExperimentalSubmit

# Rule to build all files generated by this target.
Dependencies/json-3.5.0/CMakeFiles/ExperimentalSubmit.dir/build: ExperimentalSubmit

.PHONY : Dependencies/json-3.5.0/CMakeFiles/ExperimentalSubmit.dir/build

Dependencies/json-3.5.0/CMakeFiles/ExperimentalSubmit.dir/clean:
	cd /home/michal/Work/gitHub/Casper/Dependencies/json-3.5.0 && $(CMAKE_COMMAND) -P CMakeFiles/ExperimentalSubmit.dir/cmake_clean.cmake
.PHONY : Dependencies/json-3.5.0/CMakeFiles/ExperimentalSubmit.dir/clean

Dependencies/json-3.5.0/CMakeFiles/ExperimentalSubmit.dir/depend:
	cd /home/michal/Work/gitHub/Casper && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/michal/Work/gitHub/Casper /home/michal/Work/gitHub/Casper/Dependencies/json-3.5.0 /home/michal/Work/gitHub/Casper /home/michal/Work/gitHub/Casper/Dependencies/json-3.5.0 /home/michal/Work/gitHub/Casper/Dependencies/json-3.5.0/CMakeFiles/ExperimentalSubmit.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : Dependencies/json-3.5.0/CMakeFiles/ExperimentalSubmit.dir/depend

