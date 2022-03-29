cmake_minimum_required(VERSION 3.20)
project(HPXDemo CXX)
find_package(HPX REQUIRED)
find_package(Boost REQUIRED)
find_package(fmt)

add_executable(NoHPXDemo src/main.cpp)
target_compile_features(NoHPXDemo PUBLIC cxx_std_17)
target_link_libraries(NoHPXDemo Boost::boost fmt::fmt)

add_executable(HPXDemo src/hpx_main.cpp)
target_compile_features(HPXDemo PUBLIC cxx_std_17)
target_link_libraries(HPXDemo HPX::hpx HPX::wrap_main Boost::boost fmt::fmt)
