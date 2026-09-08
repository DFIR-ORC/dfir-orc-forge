# Download helpers, usable as an include()d module or as a build-time script:
#   cmake -DURL=<url> -DDEST=<destination_path> -P DownloadFile.cmake
#
# A URL may point at a member inside an archive, e.g.
#   https://github.com/foo/bar/releases/download/v1.0/binaries.zip/OrcCapsule.exe
# The archive is then downloaded next to the destination (and reused as long
# as the URL does not change, so large archives are not re-fetched) and the
# member is extracted to the destination.

# Split a trailing '/<member>' off an archive ASSET_REGEX; the member is a
# plain entry name, so drop any regex escapes it carries.
function(_split_asset_regex input regex_var member_var)
    set(_regex "${input}")
    set(_member "")
    if(_regex MATCHES "^(.+\\.(zip|7z|tar|tgz|gz|bz2|xz))/(.+)$")
        set(_regex "${CMAKE_MATCH_1}")
        string(REPLACE "\\" "" _member "${CMAKE_MATCH_3}")
    endif()
    set(${regex_var} "${_regex}" PARENT_SCOPE)
    set(${member_var} "${_member}" PARENT_SCOPE)
endfunction()

# Download a release JSON to <file>, failing with a readable message.
function(_fetch_release_json url file display)
    file(DOWNLOAD "${url}" "${file}"
        ${ARGN}
        STATUS _status
        TLS_VERIFY ON
    )
    list(GET _status 0 _code)
    if(NOT _code EQUAL 0)
        list(GET _status 1 _msg)
        message(FATAL_ERROR "Failed to query ${display}: ${_msg}")
    endif()
endfunction()

# Resolve the download URL of an asset of a GitHub release of a repository.
#
#   resolve_github_asset(
#       REPO        DFIR-ORC/dfir-orc
#       ASSET_REGEX "DFIR-ORC\\.exe"      # matched against the end of the asset URL
#       VERSION     v10.3.4               # optional release tag, default: latest
#       HOST        github.example.com    # optional, default: github.com
#       OUTPUT      DFIRORC_EXECUTABLE_URL
#       OUTPUT_VERSION _version)          # optional: tag_name of the release,
#                                         # the tag 'latest' resolved to
#
# A GitHub Enterprise HOST is queried on https://<HOST>/api/v3. Set the
# GITHUB_TOKEN environment variable to authenticate (rate limits, private
# hosts).
#
# ASSET_REGEX may name a member inside an archive asset with
# '<archive_regex>/<member>', e.g. "DFIR-ORC-binaries\\.zip/OrcCapsule.exe":
# only the archive part is matched against the assets and OUTPUT is set to
# '<asset_url>/<member>', a form download_file() extracts.
#
function(resolve_github_asset)
    set(oneValueArgs REPO ASSET_REGEX VERSION HOST OUTPUT OUTPUT_VERSION)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})

    foreach(required REPO ASSET_REGEX OUTPUT)
        if(NOT ARG_${required})
            message(FATAL_ERROR "resolve_github_asset: ${required} is required")
        endif()
    endforeach()

    if(NOT ARG_VERSION)
        set(ARG_VERSION "latest")
    endif()

    if(NOT ARG_HOST)
        set(ARG_HOST "github.com")
    endif()
    if(ARG_HOST STREQUAL "github.com")
        set(_api "https://api.github.com")
    else()
        set(_api "https://${ARG_HOST}/api/v3")
    endif()

    _split_asset_regex("${ARG_ASSET_REGEX}" _asset_regex _member)

    if(ARG_VERSION STREQUAL "latest")
        set(_release_url "${_api}/repos/${ARG_REPO}/releases/latest")
        set(_release_display "latest release")
    else()
        set(_release_url "${_api}/repos/${ARG_REPO}/releases/tags/${ARG_VERSION}")
        set(_release_display "release ${ARG_VERSION}")
    endif()

    set(_auth_header "")
    if(DEFINED ENV{GITHUB_TOKEN} AND NOT "$ENV{GITHUB_TOKEN}" STREQUAL "")
        set(_auth_header HTTPHEADER "Authorization: Bearer $ENV{GITHUB_TOKEN}")
    endif()

    string(REPLACE "/" "_" _safe_repo "${ARG_HOST}_${ARG_REPO}")
    set(_json "${CMAKE_BINARY_DIR}/${_safe_repo}-${ARG_VERSION}-release.json")

    message(STATUS "Resolving ${_release_display} of ${ARG_REPO} from ${ARG_HOST}...")
    _fetch_release_json("${_release_url}" "${_json}"
        "${_release_display} of ${ARG_REPO} on ${ARG_HOST}"
        HTTPHEADER "Accept: application/vnd.github+json"
        HTTPHEADER "X-GitHub-Api-Version: 2022-11-28"
        ${_auth_header}
    )

    file(READ "${_json}" _content)
    string(REGEX MATCH "\"browser_download_url\"[ \t]*:[ \t]*\"([^\"]*${_asset_regex})\"" _match "${_content}")
    if(NOT _match)
        message(FATAL_ERROR "No asset matching '${_asset_regex}' in the ${_release_display} of ${ARG_REPO}")
    endif()

    set(_url "${CMAKE_MATCH_1}")
    if(_member)
        set(_url "${_url}/${_member}")
    endif()
    set(${ARG_OUTPUT} "${_url}" PARENT_SCOPE)

    if(ARG_OUTPUT_VERSION)
        # The tag the release carries: with VERSION unset this is the only way
        # back to the release 'latest' resolved to. Matched on its own instead
        # of through CMAKE_MATCH_1 above, which a missing tag_name would leave
        # holding the asset URL.
        set(_tag "")
        if(_content MATCHES "\"tag_name\"[ \t]*:[ \t]*\"([^\"]*)\"")
            set(_tag "${CMAKE_MATCH_1}")
        endif()
        set(${ARG_OUTPUT_VERSION} "${_tag}" PARENT_SCOPE)
    endif()

    message(STATUS "${ARG_REPO} asset: ${_url}")
endfunction()

# Resolve the download URL of an asset of a GitLab release of a repository.
# Same interface as resolve_github_asset; the regex is matched against the end
# of the release asset link URLs (and source archive URLs). Set the
# GITLAB_TOKEN environment variable to authenticate.
#
#   resolve_gitlab_asset(
#       REPO        group/project
#       ASSET_REGEX "OrcCapsule\\.exe"
#       VERSION     v10.3.4               # optional release tag, default: latest
#       HOST        gitlab.example.com    # optional, default: gitlab.com
#       OUTPUT      URL_VAR)
#
function(resolve_gitlab_asset)
    set(oneValueArgs REPO ASSET_REGEX VERSION HOST OUTPUT OUTPUT_VERSION)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})

    foreach(required REPO ASSET_REGEX OUTPUT)
        if(NOT ARG_${required})
            message(FATAL_ERROR "resolve_gitlab_asset: ${required} is required")
        endif()
    endforeach()

    if(NOT ARG_VERSION)
        set(ARG_VERSION "latest")
    endif()

    if(NOT ARG_HOST)
        set(ARG_HOST "gitlab.com")
    endif()

    _split_asset_regex("${ARG_ASSET_REGEX}" _asset_regex _member)

    # Project paths are URL-encoded in the GitLab API ('group/project' ->
    # 'group%2Fproject').
    string(REPLACE "/" "%2F" _project "${ARG_REPO}")
    if(ARG_VERSION STREQUAL "latest")
        set(_release_url "https://${ARG_HOST}/api/v4/projects/${_project}/releases/permalink/latest")
        set(_release_display "latest release")
    else()
        set(_release_url "https://${ARG_HOST}/api/v4/projects/${_project}/releases/${ARG_VERSION}")
        set(_release_display "release ${ARG_VERSION}")
    endif()

    set(_auth_header "")
    if(DEFINED ENV{GITLAB_TOKEN} AND NOT "$ENV{GITLAB_TOKEN}" STREQUAL "")
        set(_auth_header HTTPHEADER "PRIVATE-TOKEN: $ENV{GITLAB_TOKEN}")
    endif()

    string(REPLACE "/" "_" _safe_repo "${ARG_HOST}_${ARG_REPO}")
    set(_json "${CMAKE_BINARY_DIR}/${_safe_repo}-${ARG_VERSION}-release.json")

    message(STATUS "Resolving ${_release_display} of ${ARG_REPO} from ${ARG_HOST}...")
    _fetch_release_json("${_release_url}" "${_json}"
        "${_release_display} of ${ARG_REPO} on ${ARG_HOST}"
        HTTPHEADER "Accept: application/json"
        ${_auth_header}
    )

    file(READ "${_json}" _content)
    string(REGEX MATCH "\"(direct_asset_)?url\"[ \t]*:[ \t]*\"([^\"]*${_asset_regex})\"" _match "${_content}")
    if(NOT _match)
        message(FATAL_ERROR "No asset matching '${_asset_regex}' in the ${_release_display} of ${ARG_REPO}")
    endif()

    set(_url "${CMAKE_MATCH_2}")
    if(_member)
        set(_url "${_url}/${_member}")
    endif()
    set(${ARG_OUTPUT} "${_url}" PARENT_SCOPE)

    if(ARG_OUTPUT_VERSION)
        # See resolve_github_asset(): the tag the release carries, matched on
        # its own so that a missing tag_name yields an empty version.
        set(_tag "")
        if(_content MATCHES "\"tag_name\"[ \t]*:[ \t]*\"([^\"]*)\"")
            set(_tag "${CMAKE_MATCH_1}")
        endif()
        set(${ARG_OUTPUT_VERSION} "${_tag}" PARENT_SCOPE)
    endif()

    message(STATUS "${ARG_REPO} asset: ${_url}")
endfunction()

# Download URL to DEST, extracting the member when URL is '<archive-url>/<member>'.
# A file already downloaded from the same URL is reused.
#
#   download_file(
#       URL  "https://github.com/.../DFIR-ORC-binaries.zip/OrcCapsule.exe"
#       DEST "${CMAKE_BINARY_DIR}/OrcCapsule.exe"
#       OUTPUT_CACHED _cached)  # optional: TRUE when nothing was downloaded
#
function(download_file)
    set(oneValueArgs URL DEST OUTPUT_CACHED)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})

    foreach(required URL DEST)
        if(NOT ARG_${required})
            message(FATAL_ERROR "download_file: ${required} is required")
        endif()
    endforeach()

    set(_download_url "${ARG_URL}")
    set(_member "")
    if(ARG_URL MATCHES "^(https?://.+\\.(zip|7z|tar|tgz|gz|bz2|xz))/(.+)$")
        set(_download_url "${CMAKE_MATCH_1}")
        set(_member "${CMAKE_MATCH_3}")
    endif()

    if(_member)
        get_filename_component(_dest_dir "${ARG_DEST}" DIRECTORY)
        get_filename_component(_archive_name "${_download_url}" NAME)
        set(_download_dest "${_dest_dir}/${_archive_name}")
    else()
        set(_download_dest "${ARG_DEST}")
    endif()

    # Reuse a previous download of the same URL: the stamp records the URL so
    # that a new release publishing the same file name is fetched again.
    set(_stamp "${_download_dest}.url")
    set(_cached OFF)
    if(EXISTS "${_download_dest}" AND EXISTS "${_stamp}")
        file(READ "${_stamp}" _stamp_url)
        if(_stamp_url STREQUAL _download_url)
            set(_cached ON)
        endif()
    endif()

    if(ARG_OUTPUT_CACHED)
        set(${ARG_OUTPUT_CACHED} ${_cached} PARENT_SCOPE)
    endif()

    if(NOT _cached)
        message(STATUS "Downloading ${_download_url} -> ${_download_dest}")
        file(REMOVE "${_stamp}")
        file(DOWNLOAD "${_download_url}" "${_download_dest}"
            SHOW_PROGRESS
            STATUS _status
            TLS_VERIFY ON
        )
        list(GET _status 0 _code)
        if(NOT _code EQUAL 0)
            list(GET _status 1 _message)
            file(REMOVE "${_download_dest}")
            message(FATAL_ERROR "Failed to download ${_download_url}: ${_message}")
        endif()
        file(WRITE "${_stamp}" "${_download_url}")
    endif()

    if(NOT _member)
        return()
    endif()

    # Resolve the member to an exact entry path first: ARCHIVE_EXTRACT PATTERNS
    # requires the full entry path, and a bare name (e.g. 'OrcCapsule.exe' when
    # the zip stores it as 'x64/OrcCapsule.exe') would fail with an unhelpful
    # 'not found in archive'.
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar tf "${_download_dest}"
        OUTPUT_VARIABLE _entries
        RESULT_VARIABLE _tar_rc
    )
    if(NOT _tar_rc EQUAL 0)
        message(FATAL_ERROR "download_file: cannot list archive ${_download_dest}")
    endif()
    string(REPLACE "\r" "" _entries "${_entries}")
    string(REPLACE "\n" ";" _entries "${_entries}")

    set(_matches "")
    foreach(_entry IN LISTS _entries)
        if(_entry STREQUAL _member)
            list(APPEND _matches "${_entry}")
        else()
            # Also accept the member as an entry-name suffix ('.../<member>').
            string(LENGTH "/${_member}" _suffix_len)
            string(LENGTH "${_entry}" _entry_len)
            if(_entry_len GREATER _suffix_len)
                math(EXPR _suffix_start "${_entry_len} - ${_suffix_len}")
                string(SUBSTRING "${_entry}" ${_suffix_start} -1 _entry_suffix)
                if(_entry_suffix STREQUAL "/${_member}")
                    list(APPEND _matches "${_entry}")
                endif()
            endif()
        endif()
    endforeach()

    list(LENGTH _matches _match_count)
    if(_match_count EQUAL 0)
        message(FATAL_ERROR "download_file: '${_member}' not found in ${_download_dest}")
    elseif(_match_count GREATER 1)
        message(FATAL_ERROR
            "download_file: multiple entries match '${_member}' in ${_download_dest}:\n"
            "  ${_matches}\n"
            "Name the entry with its full path, e.g. <archive>/x64/${_member}")
    endif()
    list(GET _matches 0 _entry)

    set(_extract_dir "${_download_dest}.extracted")
    file(REMOVE_RECURSE "${_extract_dir}")
    message(STATUS "Extracting ${_entry} from ${_download_dest}")
    file(ARCHIVE_EXTRACT
        INPUT "${_download_dest}"
        DESTINATION "${_extract_dir}"
        PATTERNS "${_entry}"
    )
    if(NOT EXISTS "${_extract_dir}/${_entry}")
        message(FATAL_ERROR "download_file: failed to extract '${_entry}' from ${_download_dest}")
    endif()
    file(COPY_FILE "${_extract_dir}/${_entry}" "${ARG_DEST}" ONLY_IF_DIFFERENT)
endfunction()

# Resolve a release asset of a GitHub or GitLab repository and download it
# (extracting an archive member if ASSET_REGEX names one) in a single step.
# OUTPUT receives the local path of the downloaded file.
#
# URL may also be an ssh remote ('git@github.com:DFIR-ORC/dfir-orc' or
# 'ssh://git@host/DFIR-ORC/dfir-orc'): it only identifies the repository, the
# release is still resolved and downloaded over https.
#
#   download_repo_asset(
#       URL         https://github.com/DFIR-ORC/dfir-orc
#       ASSET_REGEX "DFIR-ORC-binaries\\.zip/OrcCapsule.exe"
#       VERSION     v10.3.4               # optional release tag, default: latest
#       PROVIDER    github                # optional github|gitlab, inferred from
#                                         # the URL host name when omitted
#       OUTPUT      DFIRORC_CAPSULE
#       DEST        "${CMAKE_BINARY_DIR}/MyCapsule.exe"  # optional, defaults to
#                                       # ${CMAKE_BINARY_DIR}/<asset or member name>
#       OUTPUT_CACHED _cached           # optional: TRUE when nothing was downloaded
#       OUTPUT_VERSION _version)        # optional: tag of the resolved release
#
function(download_repo_asset)
    set(oneValueArgs URL ASSET_REGEX VERSION PROVIDER OUTPUT DEST OUTPUT_CACHED OUTPUT_VERSION)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})

    foreach(required URL ASSET_REGEX OUTPUT)
        if(NOT ARG_${required})
            message(FATAL_ERROR "download_repo_asset: ${required} is required")
        endif()
    endforeach()

    # Two passes: a single "(\\.git)?/?$" would also match the empty string at
    # the end of the input, which CMake rejects.
    string(REGEX REPLACE "/+$" "" _repo_url "${ARG_URL}")
    string(REGEX REPLACE "\\.git$" "" _repo_url "${_repo_url}")
    string(REGEX REPLACE "/+$" "" _repo_url "${_repo_url}")
    if(_repo_url MATCHES "^https?://([^/]+)/(.+)$")
        set(_host "${CMAKE_MATCH_1}")
        set(_repo "${CMAKE_MATCH_2}")
    elseif(_repo_url MATCHES "^ssh://([^@/]+@)?([^/:]+)(:[0-9]+)?/(.+)$")
        set(_host "${CMAKE_MATCH_2}")
        set(_repo "${CMAKE_MATCH_4}")
    elseif(_repo_url MATCHES "^[^/@]+@([^/:]+):(.+)$")
        # scp-like remote: git@host:owner/repo (the user part is required so
        # that a local path like C:/x is never mistaken for a remote)
        set(_host "${CMAKE_MATCH_1}")
        set(_repo "${CMAKE_MATCH_2}")
    else()
        message(FATAL_ERROR "download_repo_asset: not a repository URL: ${ARG_URL}")
    endif()

    if(NOT ARG_PROVIDER)
        if(_host MATCHES "github")
            set(ARG_PROVIDER "github")
        elseif(_host MATCHES "gitlab")
            set(ARG_PROVIDER "gitlab")
        else()
            message(FATAL_ERROR
                "download_repo_asset: cannot infer the provider from host '${_host}', "
                "pass PROVIDER github|gitlab")
        endif()
    endif()

    set(_resolved_version "")
    if(ARG_PROVIDER STREQUAL "github")
        resolve_github_asset(
            HOST "${_host}"
            REPO "${_repo}"
            ASSET_REGEX "${ARG_ASSET_REGEX}"
            VERSION "${ARG_VERSION}"
            OUTPUT _url
            OUTPUT_VERSION _resolved_version)
    elseif(ARG_PROVIDER STREQUAL "gitlab")
        resolve_gitlab_asset(
            HOST "${_host}"
            REPO "${_repo}"
            ASSET_REGEX "${ARG_ASSET_REGEX}"
            VERSION "${ARG_VERSION}"
            OUTPUT _url
            OUTPUT_VERSION _resolved_version)
    else()
        message(FATAL_ERROR "download_repo_asset: unknown PROVIDER '${ARG_PROVIDER}', expected github|gitlab")
    endif()

    if(NOT ARG_DEST)
        get_filename_component(_name "${_url}" NAME)
        set(ARG_DEST "${CMAKE_BINARY_DIR}/${_name}")
    endif()

    download_file(URL "${_url}" DEST "${ARG_DEST}" OUTPUT_CACHED _cached)
    set(${ARG_OUTPUT} "${ARG_DEST}" PARENT_SCOPE)
    if(ARG_OUTPUT_CACHED)
        set(${ARG_OUTPUT_CACHED} ${_cached} PARENT_SCOPE)
    endif()
    if(ARG_OUTPUT_VERSION)
        set(${ARG_OUTPUT_VERSION} "${_resolved_version}" PARENT_SCOPE)
    endif()
endfunction()

# Resolve an asset specification to a local file, downloading it when needed:
#   LATEST | <version> | <repo-url>[,<version>] | <url> | <path>
# LATEST and a bare version tag (e.g. v10.3.4) use REPO_URL; a version is
# appended to a repository URL with ',' or ';' (default: latest release). A
# URL pointing at a file (or an archive member) is downloaded as is; any other
# URL is a repository whose release asset matching ASSET_REGEX is fetched — an
# ssh remote ('git@github.com:DFIR-ORC/dfir-orc' or 'ssh://git@host/...') is
# always a repository. A local path is returned unchanged. OUTPUT receives the
# local file path.
#
#   download_asset(
#       SPEC        "${DFIRORC_EXECUTABLE}"
#       REPO_URL    https://github.com/DFIR-ORC/dfir-orc
#       ASSET_REGEX "DFIR-ORC\\.exe"
#       PROVIDER    gitlab                # optional github|gitlab, inferred from
#                                         # the repository URL host name when omitted
#       VERSION     v10.3.4               # optional: release used when SPEC names
#                                         # a repository without a version, to pin
#                                         # a second asset to a release a first
#                                         # download_asset() call resolved
#       OUTPUT      DFIRORC_EXECUTABLE
#       DEST        "${CMAKE_BINARY_DIR}/DFIR-ORC.exe"  # optional, defaults to
#                                       # ${CMAKE_BINARY_DIR}/<asset or member name>
#       OUTPUT_VERSION _version         # optional: tag of the resolved release,
#                                       # empty for a local path or a direct URL
#       OUTPUT_CACHED _cached)          # optional: TRUE when a previous download
#                                       # was reused (FALSE for a local path)
#
function(download_asset)
    set(oneValueArgs SPEC REPO_URL ASSET_REGEX PROVIDER VERSION OUTPUT DEST OUTPUT_CACHED OUTPUT_VERSION)
    # PARSE_ARGV keeps ';' inside argument values, which a SPEC using ';' as
    # the version separator relies on.
    cmake_parse_arguments(PARSE_ARGV 0 ARG "" "${oneValueArgs}" "")

    foreach(required SPEC REPO_URL ASSET_REGEX OUTPUT)
        if(NOT ARG_${required})
            message(FATAL_ERROR "download_asset: ${required} is required")
        endif()
    endforeach()

    string(REPLACE "," ";" _spec "${ARG_SPEC}")
    list(GET _spec 0 _source)
    set(_version "")
    list(LENGTH _spec _len)
    if(_len GREATER 1)
        list(GET _spec 1 _version)
    endif()

    if(_source STREQUAL "LATEST")
        set(_source "${ARG_REPO_URL}")
    elseif(NOT _version AND _source MATCHES "^v?[0-9]+(\\.[0-9]+)*(-[0-9A-Za-z.]+)?$")
        set(_version "${_source}")
        set(_source "${ARG_REPO_URL}")
    endif()

    # An ssh remote always names a repository; its assets are still resolved
    # and downloaded over https.
    set(_is_ssh OFF)
    if(_source MATCHES "^ssh://" OR _source MATCHES "^[^/@]+@[^/:]+:")
        set(_is_ssh ON)
    endif()

    if(NOT _is_ssh AND NOT _source MATCHES "^https?://")
        set(${ARG_OUTPUT} "${_source}" PARENT_SCOPE)
        if(ARG_OUTPUT_CACHED)
            set(${ARG_OUTPUT_CACHED} OFF PARENT_SCOPE)
        endif()
        if(ARG_OUTPUT_VERSION)
            # A local path names no release.
            set(${ARG_OUTPUT_VERSION} "" PARENT_SCOPE)
        endif()
        return()
    endif()

    # A direct file URL names no release either; only the branch below resolves one.
    set(_resolved_version "")
    if(NOT _is_ssh AND NOT _version AND _source MATCHES "\\.(exe|zip|7z|tar|tgz|gz|bz2|xz)(/.+)?$")
        if(NOT ARG_DEST)
            get_filename_component(_name "${_source}" NAME)
            set(ARG_DEST "${CMAKE_BINARY_DIR}/${_name}")
        endif()
        download_file(URL "${_source}" DEST "${ARG_DEST}" OUTPUT_CACHED _cached)
        set(_local "${ARG_DEST}")
    else()
        # A caller that already resolved a release of this repository can pin
        # this asset to it, so that two assets meant to come from one release
        # cannot straddle a release published between the two calls. A version
        # carried by SPEC always wins.
        if(NOT _version AND ARG_VERSION)
            set(_version "${ARG_VERSION}")
        endif()

        set(_optional_args "")
        if(ARG_DEST)
            list(APPEND _optional_args DEST "${ARG_DEST}")
        endif()
        if(ARG_PROVIDER)
            list(APPEND _optional_args PROVIDER "${ARG_PROVIDER}")
        endif()
        download_repo_asset(
            URL "${_source}"
            ASSET_REGEX "${ARG_ASSET_REGEX}"
            VERSION "${_version}"
            ${_optional_args}
            OUTPUT _local
            OUTPUT_CACHED _cached
            OUTPUT_VERSION _resolved_version)
    endif()
    set(${ARG_OUTPUT} "${_local}" PARENT_SCOPE)
    if(ARG_OUTPUT_CACHED)
        set(${ARG_OUTPUT_CACHED} ${_cached} PARENT_SCOPE)
    endif()
    if(ARG_OUTPUT_VERSION)
        set(${ARG_OUTPUT_VERSION} "${_resolved_version}" PARENT_SCOPE)
    endif()
endfunction()

# Script mode: cmake -DURL=<url> -DDEST=<path> -P DownloadFile.cmake
if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
    if(NOT URL)
        message(FATAL_ERROR "DownloadFile.cmake: URL is not set")
    endif()
    if(NOT DEST)
        message(FATAL_ERROR "DownloadFile.cmake: DEST is not set")
    endif()
    download_file(URL "${URL}" DEST "${DEST}")
endif()
