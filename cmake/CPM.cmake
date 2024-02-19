# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2019-2023 Lars Melchior and contributors

set(CPM_DOWNLOAD_VERSION 0.38.7)
set(CPM_HASH_SUM "83e5eb71b2bbb8b1f2ad38f1950287a057624e385c238f6087f94cdfc44af9c5")

# Ensure that the CPM_SOURCE_CACHE is defined and in sync with ENV{CPM_SOURCE_CACHE}
if (NOT DEFINED CPM_SOURCE_CACHE)
  if(DEFINED ENV{CPM_SOURCE_CACHE})
    set(CPM_SOURCE_CACHE $ENV{CPM_SOURCE_CACHE})
  else()
    set(CPM_SOURCE_CACHE ${CMAKE_CURRENT_SOURCE_DIR}/deps/cpm_cache)
  endif()
endif()
set(ENV{CPM_SOURCE_CACHE} ${CPM_SOURCE_CACHE})

set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
set(CPM_USE_NAMED_CACHE_DIRECTORIES ON)

# Expand relative path. This is important if the provided path contains a tilde (~)
get_filename_component(CPM_DOWNLOAD_LOCATION ${CPM_DOWNLOAD_LOCATION} ABSOLUTE)

file(DOWNLOAD
     https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
     ${CPM_DOWNLOAD_LOCATION} EXPECTED_HASH SHA256=${CPM_HASH_SUM}
)

include(${CPM_DOWNLOAD_LOCATION})
