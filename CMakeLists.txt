cmake_minimum_required(VERSION 3.19)
project(P4BackupMonitor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(P4BackupMonitor WIN32
    src/main.cpp
    src/core/collector.h      src/core/collector.cpp
    src/core/health.h         src/core/health.cpp
    src/ui/mainwindow.h       src/ui/mainwindow.cpp       src/ui/mainwindow.ui
    src/ui/overviewpage.h     src/ui/overviewpage.cpp     src/ui/overviewpage.ui
    resources/resources.qrc
)

# Racine des #include : on écrit "core/collector.h", "ui/mainwindow.h"
target_include_directories(P4BackupMonitor PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(P4BackupMonitor PRIVATE Qt6::Widgets)
target_compile_definitions(P4BackupMonitor PRIVATE APP_VERSION="${PROJECT_VERSION}")

# Le collecteur PowerShell est livré à côté de l'exécutable
add_custom_command(TARGET P4BackupMonitor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:P4BackupMonitor>/collector
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/collector/Get-BackupStatus.ps1
            $<TARGET_FILE_DIR:P4BackupMonitor>/collector/Get-BackupStatus.ps1)

install(TARGETS P4BackupMonitor RUNTIME DESTINATION .)
install(FILES collector/Get-BackupStatus.ps1 DESTINATION collector)