project(solo_eps_sim)

find_package(ITC_Common REQUIRED QUIET COMPONENTS itc_logger)
find_package(NOSENGINE REQUIRED QUIET COMPONENTS common transport client uart)

include_directories(inc
                    ${sim_common_SOURCE_DIR}/inc
                    ${ITC_Common_INCLUDE_DIRS}
                    ${NOSENGINE_INCLUDE_DIRS})

set(solo_eps_sim_src
    src/solo_eps_hardware_model.cpp
    src/solo_eps_42_data_provider.cpp
    src/solo_eps_data_point.cpp
)

# For Code::Blocks and other IDEs
file(GLOB solo_eps_sim_inc inc/*.hpp)

set(solo_eps_sim_libs
    sim_common
    ${ITC_Common_LIBRARIES}
    ${NOSENGINE_LIBRARIES}
)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_RPATH}:$ORIGIN/../lib") # Pick up .so in install directory

add_library(solo_eps_sim SHARED ${solo_eps_sim_src} ${solo_eps_sim_inc})
target_link_libraries(solo_eps_sim ${solo_eps_sim_libs})
install(TARGETS solo_eps_sim LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

add_executable(nos3-solo_eps-simulator src/solo_eps_sim.cpp)
target_link_libraries(nos3-solo_eps-simulator solo_eps_sim ${solo_eps_sim_libs})
install(TARGETS nos3-solo_eps-simulator RUNTIME DESTINATION bin)
install(TARGETS solo_eps_sim LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
