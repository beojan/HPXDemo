cmake_minimum_required(VERSION 3.17)
project(HPXDemo CXX)
find_package(MKL CONFIG REQUIRED)
find_package(HPX REQUIRED)
find_package(Boost REQUIRED)
# find_package(Eigen3 3.3 REQUIRED)
find_package(CUDAToolkit 10 REQUIRED)
find_package(fmt)

add_library(global_options INTERFACE)
target_compile_features(global_options INTERFACE cxx_std_17 )
# target_compile_options(global_options INTERFACE -march=haswell -fsanitize=address -fsanitize=undefined -fsanitize=leak)
# target_link_options(global_options INTERFACE -fsanitize=address -fsanitize=undefined -fsanitize=leak)

add_executable(NoHPXDemo src/no_hpx/main.cpp)
target_link_libraries(NoHPXDemo Boost::boost fmt::fmt global_options)

add_executable(HPXDemo_scalar src/scalar/hpx_main_scalar.cpp)
target_link_libraries(HPXDemo_scalar HPX::hpx HPX::wrap_main Boost::boost fmt::fmt global_options)

add_executable(HPXDemo src/events/hpx_main.cpp)
target_link_libraries(HPXDemo HPX::hpx HPX::wrap_main Boost::boost MKL::MKL fmt::fmt global_options)

add_executable(HPXDemoCUDA src/events_cuda/hpx_main.cu)
target_link_libraries(HPXDemoCUDA HPX::hpx HPX::wrap_main Boost::boost CUDA::cudart CUDA::cublas CUDA::curand fmt::fmt global_options)

add_executable(HPXDemoDist src/events_distributed/hpx_main.cpp)
target_link_libraries(HPXDemoDist HPX::hpx HPX::wrap_main Boost::boost fmt::fmt global_options)

add_executable(HPXSerializationDistributed src/hpx_serialization/dataclass.cpp src/hpx_serialization/main.cpp)

option(DEFAULT_CONSTRUCTIBLE "Use default constructor for serialization" ON)
if(DEFAULT_CONSTRUCTIBLE)
    target_compile_definitions(HPXSerializationDistributed PRIVATE "DEFAULT_CONSTRUCTIBLE")
endif()

target_link_libraries(HPXSerializationDistributed HPX::hpx HPX::wrap_main HPX::iostreams_component global_options)
