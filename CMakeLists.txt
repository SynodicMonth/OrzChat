cmake_minimum_required(VERSION 3.15)

# set the project name
project(OrzChat)

# add the executable
add_executable(client src/client.cpp)
add_executable(server src/server.cpp)

target_link_libraries(client ws2_32)
target_link_libraries(server ws2_32)

include(CPack)