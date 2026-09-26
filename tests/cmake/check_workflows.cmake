# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.
# Workflow policy checker (test-only).
# Reads .github/workflows/ci.yml and ephemerides.yml as text (no YAML
# dependency) and enforces: dedicated local-runner labels, nonempty
# JPLEPH/JPL_TESTPO variable guards with directory checks, explicit local
# test registration inspection (no parser-only pass), top-level
# read-only permissions, and immutable pinned third-party actions.
# Variables: SOURCE (required source root).
if(NOT DEFINED SOURCE OR "${SOURCE}" STREQUAL "")
  message(FATAL_ERROR "check_workflows.cmake needs SOURCE")
endif()
foreach(_wf IN ITEMS ci ephemerides)
  set(_path "${SOURCE}/.github/workflows/${_wf}.yml")
  if(NOT EXISTS "${_path}")
    message(FATAL_ERROR "workflow file does not exist: ${_path}")
  endif()
  file(READ "${_path}" _content)
  string(REPLACE "\r\n" "\n" _content "${_content}")
  # Least privilege: explicit top-level read-only permissions.
  if(NOT _content MATCHES "(^|\n)permissions:[ \t]*\n[ \t]+contents:[ \t]*read")
    message(FATAL_ERROR "${_wf}.yml: missing top-level 'permissions: contents: read'")
  endif()
  # Immutable actions: no mutable @vN tags; every external uses:
  # reference ends in exactly 40 lowercase hex chars before an optional
  # comment; only the four approved owner/repository pairs appear.
  string(REGEX MATCHALL "(^|\n)[ \t]*uses:[ \t]*[^ \t\n]+" _uses_lines "${_content}")
  foreach(_line IN LISTS _uses_lines)
    if(_line MATCHES "@v[0-9]+([ \t]*$|[ \t])")
      message(FATAL_ERROR "${_wf}.yml: mutable action tag in '${_line}' (pin to a full commit SHA)")
    endif()
  endforeach()
  string(REGEX MATCHALL "uses:[ \t]*[^ \t\n@]+@[0-9a-f]+" _pins "${_content}")
  foreach(_pin IN LISTS _pins)
    if(NOT _pin MATCHES "@[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]( |$)")
      message(FATAL_ERROR "${_wf}.yml: action reference is not a 40-hex SHA in '${_pin}'")
    endif()
  endforeach()
  string(REGEX MATCHALL "uses:[ \t]*[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+" _repos "${_content}")
  foreach(_repo IN LISTS _repos)
    if(NOT _repo MATCHES "actions/checkout|actions/cache|actions/upload-artifact|ilammy/msvc-dev-cmd")
      message(FATAL_ERROR "${_wf}.yml: unapproved action '${_repo}' (only checkout/cache/upload-artifact/msvc-dev-cmd allowed)")
    endif()
  endforeach()
  if(_wf STREQUAL "ephemerides")
    # Dedicated local runner label: no bare [self-hosted] local job.
    if(NOT _content MATCHES "libjpl-ephemerides")
      message(FATAL_ERROR "ephemerides.yml: local job lacks the dedicated libjpl-ephemerides runner label")
    endif()
    if(_content MATCHES "runs-on: \\[self-hosted\\]")
      message(FATAL_ERROR "ephemerides.yml: bare [self-hosted] local job without the dedicated label")
    endif()
    # Nonempty-variable guards plus directory existence checks.
    foreach(_var IN ITEMS JPLEPH JPL_TESTPO)
      if(NOT _content MATCHES "${_var} repository variable is required")
        message(FATAL_ERROR "ephemerides.yml: missing nonempty ${_var} guard")
      endif()
      if(NOT _content MATCHES "${_var} directory not found")
        message(FATAL_ERROR "ephemerides.yml: missing ${_var} directory existence check")
      endif()
    endforeach()
    # Explicit registration inspection: ctest -N output must show
    # jpl_all_ephemerides_local plus at least one jpl_testpo_ name that is
    # not parser/440/440t; the run regex must not pass on parser-only.
    if(NOT _content MATCHES "ctest -N")
      message(FATAL_ERROR "ephemerides.yml: local sweep lacks a ctest -N registration proof")
    endif()
    if(NOT _content MATCHES "jpl_all_ephemerides_local")
      message(FATAL_ERROR "ephemerides.yml: local sweep does not require jpl_all_ephemerides_local")
    endif()
    if(_content MATCHES "-R 'jpl_all_ephemerides_local\\|jpl_testpo_'")
      message(FATAL_ERROR "ephemerides.yml: local sweep regex can pass on parser-only tests")
    endif()
  endif()
endforeach()
message(STATUS "libjpl: workflow policy check OK")
