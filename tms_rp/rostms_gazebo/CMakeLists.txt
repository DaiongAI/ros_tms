cmake_minimum_required(VERSION 2.8.3)
project(rostms_gazebo)

find_package(catkin REQUIRED COMPONENTS urdf)

catkin_package()

include_directories(
  ${catkin_INCLUDE_DIRS}
)
