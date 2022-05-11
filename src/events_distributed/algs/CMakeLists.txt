add_library(AlgComp SHARED AlgComp.cpp)
target_link_libraries(AlgComp 
                        PUBLIC HPX::hpx compile_features)
target_include_directories(AlgComp
                            INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> $<INSTALL_INTERFACE:include>
                            )