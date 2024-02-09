corrosion_import_crate(MANIFEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/crates/rust-url/Cargo.toml NO_LINKER_OVERRIDE)
set_property(TARGET rust-url PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/crates/rust-url/)

corrosion_import_crate(MANIFEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/crates/rust-encoding/Cargo.toml NO_LINKER_OVERRIDE)
set_property(TARGET rust-encoding PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/crates/rust-encoding/)
