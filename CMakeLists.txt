cmake_minimum_required(VERSION 3.21)

# Для сборки через vcpkg: при cmake --install копировать DLL зависимостей рядом с exe.
set(X_VCPKG_APPLOCAL_DEPS_INSTALL ON)

project(TypingTrainer
    VERSION 1.0.0
    DESCRIPTION "Тренажёр слепой печати с адаптивными упражнениями"
    HOMEPAGE_URL "https://github.com/TihonSotnikov/TypingTrainer"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW) # время файлов из скачанных архивов = время распаковки
endif()

option(TT_BUILD_APP "Собирать Qt-приложение (без него собираются только ядро и тесты)" ON)
option(TT_BUILD_TESTS "Собирать автотесты ядра (GoogleTest скачивается автоматически)" OFF)

# Предупреждения только для собственных целей проекта, сторонний код не трогаем.
add_library(tt_warnings INTERFACE)
if (MSVC)
    target_compile_options(tt_warnings INTERFACE "$<$<COMPILE_LANGUAGE:CXX>:/W4;/permissive->")
else()
    target_compile_options(tt_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

if (MSVC)
    # Исходники и словари в UTF-8.
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/utf-8>)
endif()

if (WIN32)
    # На Windows exe и все DLL лежат в корне дистрибутива. Задаётся до GNUInstallDirs и на
    # верхнем уровне: отсюда значение берёт и скрипт развёртывания Qt.
    set(CMAKE_INSTALL_BINDIR .)
endif()

if (TT_BUILD_APP)
    set(TT_QT_COMPONENTS Gui Qml Quick QuickControls2)
    if (TT_BUILD_TESTS)
        list(APPEND TT_QT_COMPONENTS Test)
    endif()
    find_package(Qt6 6.5 REQUIRED COMPONENTS ${TT_QT_COMPONENTS})
    qt_standard_project_setup(REQUIRES 6.5)
    if (QT_KNOWN_POLICY_QTP0004)
        qt_policy(SET QTP0004 NEW) # qmldir для подкаталогов с QML-файлами
    endif()
endif()

add_subdirectory(src)
add_subdirectory(src/backend)

if (TT_BUILD_APP)
    add_subdirectory(src/frontend)
endif()

if (TT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
