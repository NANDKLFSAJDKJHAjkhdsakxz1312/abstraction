project(EtherCatUserExample C CXX)
cmake_minimum_required(VERSION 3.2)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_PREFIX_PATH "/home/root/software/ethercat_install" ${CMAKE_PREFIX_PATH})

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
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
   
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(igh_test
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(igh_test SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(igh_test
            ${PROJECT_SOURCES}
        )
    endif()
endif()

# Qt 和 EtherCAT 链接都放在一起
target_link_libraries(igh_test
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        EtherLab::ethercat
)

