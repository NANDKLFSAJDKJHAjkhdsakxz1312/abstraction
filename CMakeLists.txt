project(EtherCatUserExample C CXX)
cmake_minimum_required(VERSION 3.2)
set(CMAKE_BUILD_TYPE Debug)
list(APPEND CMAKE_PREFIX_PATH "/home/root/software/qtbase5.15.7_install")
list(APPEND CMAKE_PREFIX_PATH "/home/root/software/ethercat_install")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)
find_package(EtherCAT REQUIRED)

set(PROJECT_SOURCES
    main2.cpp
zdltask.cpp
   
)
add_executable(igh_test
main2.cpp
zdltask.cpp
)

# Qt 和 EtherCAT 链接都放在一起
target_link_libraries(igh_test
    PRIVATE
        Qt::Widgets
        EtherLab::ethercat
)

