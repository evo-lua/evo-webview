cmake_minimum_required(VERSION 3.21)

# At least C++11 is required, though this may change in the future
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Consumer of the library (root project)
project(WebViewTestApp)
add_executable(WebViewTestApp main.cpp)

# Build and link the library itself
add_subdirectory(webview)
target_link_libraries(WebViewTestApp PRIVATE webview)
