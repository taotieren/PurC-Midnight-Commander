if (NOT WTF_DIR)
    set(WTF_DIR "${CMAKE_SOURCE_DIR}/src/wtf")
endif ()
if (NOT PURCMC_DIR)
    set(PURCMC_DIR "${CMAKE_SOURCE_DIR}/src/purcmc")
endif ()
if (NOT THIRDPARTY_DIR)
    set(THIRDPARTY_DIR "${CMAKE_SOURCE_DIR}/src/third-party")
endif ()
if (NOT TOOLS_DIR)
    set(TOOLS_DIR "${CMAKE_SOURCE_DIR}/tools")
endif ()

set(DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources")
set(WTF_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WTF")
set(PurcMC_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/PurcMC")
set(PurcMCTestRunner_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/PurcMCTestRunner")

set(FORWARDING_HEADERS_DIR ${DERIVED_SOURCES_DIR}/ForwardingHeaders)

set(bmalloc_FRAMEWORK_HEADERS_DIR ${FORWARDING_HEADERS_DIR})
set(WTF_FRAMEWORK_HEADERS_DIR ${FORWARDING_HEADERS_DIR})
set(PurcMC_FRAMEWORK_HEADERS_DIR ${FORWARDING_HEADERS_DIR})
set(PurcMC_PRIVATE_FRAMEWORK_HEADERS_DIR ${FORWARDING_HEADERS_DIR})
