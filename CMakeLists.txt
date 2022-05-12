cmake_minimum_required(VERSION 3.17)
project(HPXDemo CXX)
find_package(HPX REQUIRED)
find_package(Boost REQUIRED)
find_package(fmt)

add_library(compile_features INTERFACE)
target_compile_features(compile_features INTERFACE cxx_std_17)

add_executable(NoHPXDemo src/no_hpx/main.cpp)
target_link_libraries(NoHPXDemo Boost::boost fmt::fmt compile_features)

add_executable(HPXDemo_scalar src/scalar/hpx_main_scalar.cpp)
target_link_libraries(HPXDemo_scalar HPX::hpx HPX::wrap_main Boost::boost fmt::fmt compile_features)

add_executable(HPXDemo src/events/hpx_main.cpp)
target_link_libraries(HPXDemo HPX::hpx HPX::wrap_main Boost::boost fmt::fmt compile_features)

add_subdirectory(src/events_distributed/algs)
add_executable(HPXDemoDist src/events_distributed/hpx_main.cpp)
target_link_libraries(HPXDemoDist AlgComp HPX::hpx HPX::wrap_main Boost::boost fmt::fmt compile_features)

add_executable(HPXSerializationDistributed src/hpx_serialization/dataclass.cpp src/hpx_serialization/main.cpp)

option(DEFAULT_CONSTRUCTIBLE "Use default constructor for serialization" ON)
if(DEFAULT_CONSTRUCTIBLE)
    target_compile_definitions(HPXSerializationDistributed PRIVATE "DEFAULT_CONSTRUCTIBLE")
endif()

target_link_libraries(HPXSerializationDistributed HPX::hpx HPX::wrap_main HPX::iostreams_component compile_features)
