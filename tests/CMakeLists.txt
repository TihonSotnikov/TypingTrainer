include(FetchContent)

FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz
    URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE) # тот же рантайм MSVC, что и у проекта
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(backend_tests
    test_utils.hpp
    concurrent_queue_test.cpp
    dictionaries_test.cpp
    json_storage_test.cpp
    metrics_test.cpp
    ngram_statistics_test.cpp
    session_engine_test.cpp
    session_history_test.cpp
    smart_text_generator_test.cpp
    text_utils_test.cpp
    typing_trainer_core_test.cpp
)

target_link_libraries(backend_tests
    PRIVATE
    backend
    tt_warnings
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(backend_tests DISCOVERY_MODE PRE_TEST)

# ----- UI-тесты: настоящий интерфейс без экрана -----
if (TARGET typing_trainer_ui)
    qt_add_executable(ui_tests ui/ui_smoke_test.cpp)
    target_link_libraries(ui_tests
        PRIVATE
        typing_trainer_ui
        typing_trainer_uiplugin
        Qt6::Test
        Qt6::Quick
        Qt6::QuickControls2
    )

    add_test(NAME ui_tests COMMAND ui_tests)
    set_tests_properties(ui_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
