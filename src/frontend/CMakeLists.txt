set(TT_PACKAGING_DIR "${PROJECT_SOURCE_DIR}/packaging")

# ----- Интерфейс: QML-модуль TypingTrainerModule -----
# Модуль собран отдельной статической библиотекой, чтобы его загружали
# и приложение, и UI-тесты.

qt_add_library(typing_trainer_ui STATIC)

set(TT_QML_FILES
    qml/Main.qml
    qml/Theme.qml
    qml/TypingPage.qml
    qml/TypingArea.qml
    qml/TextEditorPanel.qml
    qml/ResultsPanel.qml
    qml/StatisticsPage.qml
    qml/WpmChart.qml
    qml/SettingsPage.qml
    qml/SettingsSection.qml
    qml/SettingRow.qml
    qml/PageHeader.qml
    qml/MetricTile.qml
    qml/KeyHint.qml
    qml/XButton.qml
    qml/XSegmentedControl.qml
)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
# Файлы лежат в qml/, но в ресурсах - в корне модуля, рядом со шрифтами.
foreach(qml_file IN LISTS TT_QML_FILES)
    cmake_path(GET qml_file FILENAME qml_name)
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_name})
endforeach()

# Иконка окна (на Linux и в панели задач Windows) - из общих ресурсов упаковки.
set_source_files_properties("${TT_PACKAGING_DIR}/icons/icon.png" PROPERTIES QT_RESOURCE_ALIAS icons/app.png)

qt_add_qml_module(typing_trainer_ui
    URI TypingTrainerModule
    VERSION 1.0
    OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/qml/TypingTrainerModule"
    QML_FILES ${TT_QML_FILES}
    SOURCES
        typing_trainer_adapter.cpp
        typing_trainer_adapter.hpp
    RESOURCES
        fonts/JetBrainsMono-Regular.ttf
        fonts/JetBrainsMono-Bold.ttf
        "${TT_PACKAGING_DIR}/icons/icon.png"
)

target_include_directories(typing_trainer_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(typing_trainer_ui
    PUBLIC
    contracts
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    PRIVATE
    backend
    tt_warnings
)

# ----- Приложение -----

qt_add_executable(TypingTrainer main.cpp)

target_compile_definitions(TypingTrainer PRIVATE TT_VERSION="${PROJECT_VERSION}")

target_link_libraries(TypingTrainer
    PRIVATE
    typing_trainer_uiplugin
    tt_warnings
    Qt6::Gui
    Qt6::Qml
    Qt6::QuickControls2
)

# ----- Платформенные метаданные -----

if (WIN32)
    enable_language(RC)
    set(TT_ICON_ICO "${TT_PACKAGING_DIR}/icons/icon.ico")
    configure_file("${TT_PACKAGING_DIR}/windows/TypingTrainer.rc.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/TypingTrainer.rc" @ONLY)
    target_sources(TypingTrainer PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/TypingTrainer.rc")

    # В Release консольное окно не нужно.
    set_target_properties(TypingTrainer PROPERTIES WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>)
endif()

if (APPLE)
    set(TT_ICON_ICNS "${TT_PACKAGING_DIR}/icons/icon.icns")
    set_source_files_properties("${TT_ICON_ICNS}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(TypingTrainer PRIVATE "${TT_ICON_ICNS}")

    set_target_properties(TypingTrainer PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Typing Trainer"
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.tihonsotnikov.typingtrainer"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "icon.icns"
        MACOSX_BUNDLE_COPYRIGHT "MIT License"
    )
endif()

# ----- Установка и развёртывание Qt -----

install(TARGETS TypingTrainer
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if (WIN32 OR APPLE)
    # Скрипт копирует рядом с приложением библиотеки, плагины и QML-модули Qt
    # (windeployqt / macdeployqt). Выполняется только при cmake --install.
    qt_generate_deploy_qml_app_script(
        TARGET TypingTrainer
        OUTPUT_SCRIPT deploy_script
    )
    install(SCRIPT ${deploy_script})
endif()

if (UNIX AND NOT APPLE)
    # Ярлык и иконка для меню приложений (используются и при сборке AppImage).
    install(FILES "${TT_PACKAGING_DIR}/linux/io.github.tihonsotnikov.TypingTrainer.desktop"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES "${TT_PACKAGING_DIR}/icons/icon.png"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps"
            RENAME typingtrainer.png)
endif()

if (WIN32)
    # Рантайм MSVC, чтобы приложение запускалось без установки vc_redist.
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    include(InstallRequiredSystemLibraries)
endif()
