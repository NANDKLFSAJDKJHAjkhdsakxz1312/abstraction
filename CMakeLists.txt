project(EtherCatUserExample C)
cmake_minimum_required(VERSION 3.2)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_PREFIX_PATH "/home/root/software/ethercat_install" ${CMAKE_PREFIX_PATH})
find_package(EtherCAT REQUIRED)
add_executable(userexample
    main.c
)

target_link_libraries(userexample PUBLIC EtherLab::ethercat)
