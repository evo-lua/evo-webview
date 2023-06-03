cmake_minimum_required(VERSION 3.25.2)
project(webview)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CROSSPLATFORM_SOURCES "src/common/webview.cc")

if(WIN32)
	set(WINDOWS_SOURCES ${WINDOWS_SOURCES}
		src/windows/com_init_wrapper.cpp
		src/windows/mswebview_engine.cpp
		src/windows/webview2_loader/reg_key.cpp
		src/windows/webview2_loader/com_event_handler.cpp
		src/windows/webview2_loader/msedge_runtime_loader.cpp
	)
elseif(APPLE)
	set(OSX_SOURCES ${OSX_SOURCES}
		src/macOS/cocoa_engine.mm
	)
else()
	set(LINUX_SOURCES ${LINUX_SOURCES}
		src/linux/webkit2gtk_engine.cpp
	)
endif()

add_library(webview STATIC ${CROSSPLATFORM_SOURCES} ${WINDOWS_SOURCES} ${OSX_SOURCES} ${LINUX_SOURCES})

if(WIN32)
	target_include_directories(webview PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/src/windows
	)
	target_link_libraries(webview PUBLIC advapi32 ole32 shell32 shlwapi user32 version)
elseif(APPLE)
	target_include_directories(webview PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/macOS)
	target_link_libraries(webview PUBLIC "-framework WebKit")
else()
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
	pkg_check_modules(WEBKIT2GTK4 REQUIRED webkit2gtk-4.0)
	target_include_directories(webview PUBLIC
		${GTK3_INCLUDE_DIRS}
		${WEBKIT2GTK4_INCLUDE_DIRS}
		${CMAKE_CURRENT_SOURCE_DIR}/src/linux
	)
	target_link_libraries(webview PUBLIC ${GTK3_LIBRARIES} ${WEBKIT2GTK4_LIBRARIES})
endif()

target_include_directories(webview PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/src/common
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(basic_c WIN32 examples/basic.c)
add_executable(basic_cpp WIN32 examples/basic.cc)
add_executable(bind_c WIN32 examples/bind.c)
add_executable(bind_cpp WIN32 examples/bind.cc)
target_link_libraries(basic_c PRIVATE webview)
target_link_libraries(basic_cpp PRIVATE webview)
target_link_libraries(bind_c PRIVATE webview)
target_link_libraries(bind_cpp PRIVATE webview)

add_executable(webview_test webview_test.cc)
target_link_libraries(webview_test PRIVATE webview)

enable_testing()
add_test(NAME WebviewTest COMMAND ${CMAKE_BINARY_DIR}/webview_test)
