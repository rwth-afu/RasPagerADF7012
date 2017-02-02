project(HWTest_GFSK)

set(CMAKE_CXX_FLAGS "-lwiringPi")
add_executable(HWTest_GFSK main.cpp 4BitTable.cpp 7BitASCII.cpp BCHEncoder.cpp adf7012.cpp)


