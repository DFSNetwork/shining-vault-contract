project(shiningvault)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_contract( shiningvault shiningvault shiningvault.cpp )
target_include_directories( shiningvault PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( shiningvault ${CMAKE_SOURCE_DIR}/../ricardian )