add_executable(lzpico main.cpp state-ui.cpp)
target_include_directories(lzpico PRIVATE ..)
target_link_libraries(lzpico PRIVATE imgui imgui-ws ${CMAKE_THREAD_LIBS_INIT})
