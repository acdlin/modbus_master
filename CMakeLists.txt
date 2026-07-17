cmake_minimum_required(VERSION 3.16)

project(modbus_master LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets SerialPort Network)

add_executable(${PROJECT_NAME}     
    src/main.cpp
    src/MainWindow.cpp
    src/ModbusClient.cpp
    src/RegisterModel.cpp
    src/ConnectionManager.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::SerialPort Qt6::Network)


target_include_directories(${PROJECT_NAME} PRIVATE src)
