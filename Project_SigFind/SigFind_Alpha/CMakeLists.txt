cmake_minimum_required(VERSION 2.8)
project( ConvNet )
find_package( OpenCV REQUIRED )
add_executable( ConvNet ConvNet.cpp )
target_link_libraries ( ConvNet  ${OpenCV_LIBS} )
