# Общий контракт frontend <-> backend и вспомогательные заголовки.
add_library(contracts INTERFACE)

target_include_directories(contracts
    INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_sources(contracts
    INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/contracts.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/concurrent_queue.hpp"
)
