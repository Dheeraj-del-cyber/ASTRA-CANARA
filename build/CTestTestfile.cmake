# CMake generated Testfile for 
# Source directory: C:/Users/Dheeraj/OneDrive/Videos/Music/Desktop/ASTRA-CANARA
# Build directory: C:/Users/Dheeraj/OneDrive/Videos/Music/Desktop/ASTRA-CANARA/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(torture_tool_phase5_concurrency "C:/Users/Dheeraj/OneDrive/Videos/Music/Desktop/ASTRA-CANARA/build/torture_tool_phase5.exe" "--seed" "1337" "--iterations" "500" "--threads" "4" "--reset" "--db" "phase5_concurrency_test.db" "--oracle" "phase5_concurrency_test.ack.log")
set_tests_properties(torture_tool_phase5_concurrency PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/Dheeraj/OneDrive/Videos/Music/Desktop/ASTRA-CANARA/CMakeLists.txt;56;add_test;C:/Users/Dheeraj/OneDrive/Videos/Music/Desktop/ASTRA-CANARA/CMakeLists.txt;0;")
