cmake_minimum_required(VERSION 3.8)
project(agrobot_odrive)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(hardware_interface REQUIRED)
find_package(pluginlib REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)

add_library(agrobot_odrive_plugins SHARED
  src/geared_odrive_hardware_interface.cpp
)
target_compile_features(agrobot_odrive_plugins PRIVATE cxx_std_17)
target_link_libraries(agrobot_odrive_plugins PUBLIC
  hardware_interface::hardware_interface
  pluginlib::pluginlib
  rclcpp::rclcpp
  rclcpp_lifecycle::rclcpp_lifecycle
)

pluginlib_export_plugin_description_file(hardware_interface agrobot_odrive_plugins.xml)

install(TARGETS agrobot_odrive_plugins
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(
  DIRECTORY config docs
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
