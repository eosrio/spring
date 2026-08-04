include(CMakeParseArguments)

set(SPRING_CONSENSUS_MODULES "" CACHE STRING
    "Semicolon-separated source directories for compile-time consensus modules")

function(spring_add_consensus_module)
   cmake_parse_arguments(MODULE "" "NAME;VERSION" "SOURCES;INTRINSICS" ${ARGN})

   if(NOT MODULE_NAME)
      message(FATAL_ERROR "spring_add_consensus_module requires NAME")
   endif()
   if(NOT MODULE_NAME MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
      message(FATAL_ERROR "Consensus module NAME '${MODULE_NAME}' must be a C identifier")
   endif()
   if(NOT MODULE_VERSION)
      message(FATAL_ERROR "Consensus module '${MODULE_NAME}' requires VERSION")
   endif()
   if(NOT MODULE_SOURCES)
      message(FATAL_ERROR "Consensus module '${MODULE_NAME}' requires SOURCES")
   endif()
   if(NOT MODULE_INTRINSICS)
      message(FATAL_ERROR "Consensus module '${MODULE_NAME}' requires INTRINSICS")
   endif()

   get_property(module_names GLOBAL PROPERTY SPRING_CONSENSUS_MODULE_NAMES)
   if(MODULE_NAME IN_LIST module_names)
      message(FATAL_ERROR "Duplicate consensus module name '${MODULE_NAME}'")
   endif()
   set_property(GLOBAL APPEND PROPERTY SPRING_CONSENSUS_MODULE_NAMES "${MODULE_NAME}")

   set(module_source_manifest "")
   foreach(source IN LISTS MODULE_SOURCES)
      if(IS_ABSOLUTE "${source}")
         set(source_abs "${source}")
      else()
         get_filename_component(source_abs "${source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
      endif()
      if(NOT EXISTS "${source_abs}")
         message(FATAL_ERROR "Consensus module '${MODULE_NAME}' source does not exist: ${source_abs}")
      endif()
      set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source_abs}")
      set_property(GLOBAL APPEND PROPERTY SPRING_CONSENSUS_MODULE_SOURCES "${source_abs}")
      file(SHA256 "${source_abs}" source_hash)
      get_filename_component(source_name "${source_abs}" NAME)
      list(APPEND module_source_manifest "${source_name}:${source_hash}")
   endforeach()

   get_property(registered_intrinsics GLOBAL PROPERTY SPRING_CONSENSUS_MODULE_INTRINSICS)
   foreach(intrinsic IN LISTS MODULE_INTRINSICS)
      if(NOT intrinsic MATCHES "^[A-Za-z0-9_]+$")
         message(FATAL_ERROR
                 "Consensus module '${MODULE_NAME}' intrinsic '${intrinsic}' must be an unqualified env name")
      endif()
      if(intrinsic IN_LIST registered_intrinsics)
         message(FATAL_ERROR "Duplicate consensus intrinsic '${intrinsic}'")
      endif()
      list(APPEND registered_intrinsics "${intrinsic}")
      set_property(GLOBAL APPEND PROPERTY SPRING_CONSENSUS_MODULE_INTRINSICS "${intrinsic}")
   endforeach()

   list(JOIN MODULE_INTRINSICS "," module_intrinsic_manifest)
   list(JOIN module_source_manifest "," module_source_manifest)
   set_property(GLOBAL APPEND PROPERTY SPRING_CONSENSUS_MODULE_MANIFEST
                "${MODULE_NAME}@${MODULE_VERSION}=${module_intrinsic_manifest}#${module_source_manifest}")
   message(STATUS "[Consensus Module] ${MODULE_NAME}@${MODULE_VERSION}: ${MODULE_INTRINSICS}")
endfunction()

function(spring_configure_consensus_modules)
   cmake_parse_arguments(CONFIG "" "OUTPUT_INCLUDE_DIR;OUT_SOURCES;OUT_MANIFEST_HASH" "" ${ARGN})
   if(NOT CONFIG_OUTPUT_INCLUDE_DIR OR NOT CONFIG_OUT_SOURCES OR NOT CONFIG_OUT_MANIFEST_HASH)
      message(FATAL_ERROR
              "spring_configure_consensus_modules requires OUTPUT_INCLUDE_DIR, OUT_SOURCES, and OUT_MANIFEST_HASH")
   endif()

   set_property(GLOBAL PROPERTY SPRING_CONSENSUS_MODULE_NAMES "")
   set_property(GLOBAL PROPERTY SPRING_CONSENSUS_MODULE_SOURCES "")
   set_property(GLOBAL PROPERTY SPRING_CONSENSUS_MODULE_INTRINSICS "")
   set_property(GLOBAL PROPERTY SPRING_CONSENSUS_MODULE_MANIFEST "")

   foreach(module_dir IN LISTS SPRING_CONSENSUS_MODULES)
      if(IS_ABSOLUTE "${module_dir}")
         set(module_abs "${module_dir}")
      else()
         get_filename_component(module_abs "${module_dir}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
      endif()
      if(NOT EXISTS "${module_abs}/CMakeLists.txt")
         message(FATAL_ERROR "Consensus module directory has no CMakeLists.txt: ${module_abs}")
      endif()
      string(MD5 module_hash "${module_abs}")
      add_subdirectory("${module_abs}" "${CMAKE_CURRENT_BINARY_DIR}/consensus_modules/${module_hash}")
   endforeach()

   get_property(module_sources GLOBAL PROPERTY SPRING_CONSENSUS_MODULE_SOURCES)
   get_property(module_intrinsics GLOBAL PROPERTY SPRING_CONSENSUS_MODULE_INTRINSICS)
   get_property(module_manifest GLOBAL PROPERTY SPRING_CONSENSUS_MODULE_MANIFEST)
   get_property(module_names GLOBAL PROPERTY SPRING_CONSENSUS_MODULE_NAMES)

   set(SPRING_CONSENSUS_OC_INTRINSICS "")
   set(SPRING_CONSENSUS_GENESIS_INTRINSICS "")
   foreach(intrinsic IN LISTS module_intrinsics)
      string(APPEND SPRING_CONSENSUS_OC_INTRINSICS ",\n      \"env.${intrinsic}\"")
      string(APPEND SPRING_CONSENSUS_GENESIS_INTRINSICS ",\n   \"${intrinsic}\"")
   endforeach()

   set(SPRING_CONSENSUS_MODULE_DECLARATIONS "")
   set(SPRING_CONSENSUS_MODULE_REGISTRATIONS "")
   foreach(module_name IN LISTS module_names)
      string(APPEND SPRING_CONSENSUS_MODULE_DECLARATIONS
             "extern \"C\" void spring_register_consensus_module_${module_name}();\n")
      string(APPEND SPRING_CONSENSUS_MODULE_REGISTRATIONS
             "      spring_register_consensus_module_${module_name}();\n")
   endforeach()

   list(JOIN module_manifest "|" SPRING_CONSENSUS_MODULE_MANIFEST)
   if(SPRING_CONSENSUS_MODULE_MANIFEST)
      string(SHA256 SPRING_CONSENSUS_MODULE_MANIFEST_HASH "${SPRING_CONSENSUS_MODULE_MANIFEST}")
      string(SUBSTRING "${SPRING_CONSENSUS_MODULE_MANIFEST_HASH}" 0 16 SPRING_CONSENSUS_MODULE_CACHE_ID)
   else()
      string(SHA256 SPRING_CONSENSUS_MODULE_MANIFEST_HASH "")
      set(SPRING_CONSENSUS_MODULE_CACHE_ID "0000000000000000")
   endif()

   file(MAKE_DIRECTORY "${CONFIG_OUTPUT_INCLUDE_DIR}/eosio/chain/webassembly")
   get_filename_component(generated_root "${CONFIG_OUTPUT_INCLUDE_DIR}" DIRECTORY)
   configure_file(
      "${CMAKE_SOURCE_DIR}/CMakeModules/consensus_intrinsic_names.inc.in"
      "${CONFIG_OUTPUT_INCLUDE_DIR}/eosio/chain/webassembly/consensus_intrinsic_names.inc"
      @ONLY
   )
   configure_file(
      "${CMAKE_SOURCE_DIR}/CMakeModules/consensus_genesis_intrinsics.inc.in"
      "${CONFIG_OUTPUT_INCLUDE_DIR}/eosio/chain/consensus_genesis_intrinsics.inc"
      @ONLY
   )
   configure_file(
      "${CMAKE_SOURCE_DIR}/CMakeModules/consensus_module_manifest.hpp.in"
      "${CONFIG_OUTPUT_INCLUDE_DIR}/eosio/chain/consensus_module_manifest.hpp"
      @ONLY
   )
   configure_file(
      "${CMAKE_SOURCE_DIR}/CMakeModules/consensus_module_registry.cpp.in"
      "${generated_root}/consensus_module_registry.cpp"
      @ONLY
   )

   list(APPEND module_sources "${generated_root}/consensus_module_registry.cpp")
   set(${CONFIG_OUT_SOURCES} "${module_sources}" PARENT_SCOPE)
   set(${CONFIG_OUT_MANIFEST_HASH} "${SPRING_CONSENSUS_MODULE_MANIFEST_HASH}" PARENT_SCOPE)
endfunction()
