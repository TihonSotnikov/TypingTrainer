# nlohmann/json: берём установленный в системе, иначе скачиваем фиксированную версию.
find_package(nlohmann_json 3.11 QUIET)
if (NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
        URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# Словари Smart-режима хранятся в текстовых файлах (слово на строку) и встраиваются
# в программу при конфигурации: каждое слово превращается в строку U"слово",.
function(tt_embed_word_list txt_file inc_file)
    file(STRINGS "${txt_file}" lines ENCODING UTF-8)
    cmake_path(GET txt_file FILENAME txt_name)
    set(content "// Сгенерировано CMake из dictionaries/${txt_name}. Не редактировать вручную.\n")
    foreach(line IN LISTS lines)
        string(STRIP "${line}" word)
        if (word STREQUAL "" OR word MATCHES "^#")
            continue()
        endif()
        string(APPEND content "U\"${word}\",\n")
    endforeach()
    file(CONFIGURE OUTPUT "${inc_file}" CONTENT "${content}" @ONLY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${txt_file}")
endfunction()

set(TT_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
tt_embed_word_list("${CMAKE_CURRENT_SOURCE_DIR}/dictionaries/en.txt" "${TT_GENERATED_DIR}/dictionary_en.inc")
tt_embed_word_list("${CMAKE_CURRENT_SOURCE_DIR}/dictionaries/ru.txt" "${TT_GENERATED_DIR}/dictionary_ru.inc")

add_library(
    backend STATIC
    typing_trainer_core.cpp
    typing_trainer_core.hpp
    ngram_statistics.cpp
    ngram_statistics.hpp
    session_engine.cpp
    session_engine.hpp
    session_history.cpp
    session_history.hpp
    dictionaries.cpp
    dictionaries.hpp
    json_storage.cpp
    json_storage.hpp
    smart_text_generator.cpp
    smart_text_generator.hpp
    text_utils.cpp
    text_utils.hpp
)

target_include_directories(
    backend
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    PRIVATE
    "${TT_GENERATED_DIR}"
)

target_link_libraries(
    backend
    PUBLIC
    contracts
    nlohmann_json::nlohmann_json
    PRIVATE
    tt_warnings
)
