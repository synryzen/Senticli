cmake_minimum_required(VERSION 3.21)
project(Senticli VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 Qml Network)

qt_standard_project_setup(REQUIRES 6.4)

qt_add_executable(Senticli
    src/main.cpp
    src/core/AppController.cpp
    src/models/MessageModel.cpp
    src/core/include/AppController.h
    src/models/MessageModel.h
)

set_target_properties(Senticli PROPERTIES
    OUTPUT_NAME senticli
)

target_include_directories(Senticli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/models
)

set_source_files_properties(qml/theme/Colors.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/theme/Typography.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(Senticli
    URI Senticli
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/views/FaceView.qml
        qml/views/StatusStrip.qml
        qml/views/SettingsView.qml
        qml/views/SetupWizard.qml
        qml/views/TerminalView.qml
        qml/views/InputBar.qml
        qml/components/Eye.qml
        qml/components/Mouth.qml
        qml/components/CommandBubble.qml
        qml/theme/Colors.qml
        qml/theme/Typography.qml
)

target_link_libraries(Senticli PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Qml
    Qt6::Network
)

install(TARGETS Senticli
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/senticli.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/senticli.desktop
    @ONLY
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/senticli.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
)

set(CPACK_PACKAGE_NAME "senticli")
set(CPACK_PACKAGE_VENDOR "Senticli")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Local-first Linux AI terminal companion")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_CONTACT "maintainers@senticli.local")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_GENERATOR "TGZ;DEB")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Senticli Team")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6quick6, libqt6qml6, libqt6network6, libqt6quickcontrols2-6")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

include(CPack)
