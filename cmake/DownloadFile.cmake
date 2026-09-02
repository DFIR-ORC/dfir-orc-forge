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

# TRUE when a sources specification names a repository to clone (LATEST, a
# version tag or a commit to pin, an http(s) or ssh remote), FALSE for a local
# path.
# A commit is at least 7 hex digits (CMake regexes have no bounded repetition).
function(_is_commit value out_var)
    string(LENGTH "${value}" _length)
    if(value MATCHES "^[0-9a-f]+$" AND _length GREATER_EQUAL 7 AND _length LESS_EQUAL 40)
        set(${out_var} TRUE PARENT_SCOPE)
    else()
        set(${out_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(is_repo_spec spec out_var)
    string(REPLACE "," ";" _spec "${spec}")
    list(GET _spec 0 _source)
    _is_commit("${_source}" _commit)
    set(_result FALSE)
    if(_source STREQUAL "LATEST"
        OR _commit
        OR _source MATCHES "^v?[0-9]+(\\.[0-9]+)*(-[0-9A-Za-z.]+)?$"
        OR _source MATCHES "^https?://"
        OR _source MATCHES "^ssh://"
        OR _source MATCHES "^[^/@]+@[^/:]+:")
        set(_result TRUE)
    endif()
    set(${out_var} ${_result} PARENT_SCOPE)
endfunction()

# Zip the files listed in <files_var> (paths relative to <workdir>) into
# <archive>; the list goes through a file to stay clear of command line length
# limits.
function(_zip_files archive workdir files_var)
    list(JOIN ${files_var} "\n" _files_lines)
    file(WRITE "${archive}.files" "${_files_lines}\n")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar cf "${archive}" --format=zip
            "--files-from=${archive}.files"
        WORKING_DIRECTORY "${workdir}"
        RESULT_VARIABLE _rc
    )
    if(NOT _rc EQUAL 0)
        file(REMOVE "${archive}")
        message(FATAL_ERROR "Failed to archive ${workdir} into ${archive}")
    endif()
endfunction()

# Run git in <workdir> and return its exit code.
function(_git_run rc_var workdir)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${workdir}" ${ARGN}
        RESULT_VARIABLE _rc
    )
    set(${rc_var} ${_rc} PARENT_SCOPE)
endfunction()

# Clone a repository (shallow, recursing into submodules) at a git reference
# and archive its working tree into a zip laid out like the embedded
# resources: entry names relative to the repository root, root '.git*' entries
# excluded, submodule content included. The specification is resolved like
# download_asset() resolves assets:
#   LATEST | <version> | <commit> | <repo-url>[,<ref>]
# LATEST, a bare version tag (e.g. v1.2.3) and a bare commit use REPO_URL; a
# branch, tag or commit is appended to a repository URL (https or ssh) with
# ',' or ';' (default: HEAD). The clone and the archive are reused while the
# reference still resolves to the same commit on the remote; a moving
# reference (HEAD, a branch) is cloned again when it advances.
#
#   clone_repo_archive(
#       SPEC          "${DFIRORC_CONFIG}"
#       REPO_URL      https://github.com/DFIR-ORC/dfir-orc-config
#       ARCHIVE       "${CMAKE_BINARY_DIR}/dfir-orc-config.zip"
#       REF           gh-pages        # optional: reference used when SPEC does
#                                     # not name one (default: HEAD)
#       DIRECTORY     docs            # optional: only archive this subdirectory
#                                     # (entry names keep the 'docs/' prefix)
#       OUTPUT_COMMIT DFIRORC_CONFIG_COMMIT   # optional: the cloned commit
#       OUTPUT_CACHED _cached)                # optional: TRUE when the previous
#                                             # clone and archive were reused
#
function(clone_repo_archive)
    set(oneValueArgs SPEC REPO_URL ARCHIVE REF DIRECTORY OUTPUT_COMMIT OUTPUT_CACHED)
    # PARSE_ARGV keeps ';' inside argument values, which a SPEC using ';' as
    # the reference separator relies on.
    cmake_parse_arguments(PARSE_ARGV 0 ARG "" "${oneValueArgs}" "")

    foreach(required SPEC REPO_URL ARCHIVE)
        if(NOT ARG_${required})
            message(FATAL_ERROR "clone_repo_archive: ${required} is required")
        endif()
    endforeach()

    if(NOT ARG_REF)
        set(ARG_REF "HEAD")
    endif()

    string(REPLACE "," ";" _spec "${ARG_SPEC}")
    list(GET _spec 0 _url)
    set(_ref "${ARG_REF}")
    list(LENGTH _spec _len)
    if(_len GREATER 1)
        list(GET _spec 1 _ref)
    endif()

    _is_commit("${_url}" _url_is_commit)
    if(_url STREQUAL "LATEST")
        set(_url "${ARG_REPO_URL}")
    elseif(_len EQUAL 1
        AND (_url_is_commit OR _url MATCHES "^v?[0-9]+(\\.[0-9]+)*(-[0-9A-Za-z.]+)?$"))
        # A bare version tag or commit pins the default repository.
        set(_ref "${_url}")
        set(_url "${ARG_REPO_URL}")
    endif()

    find_program(GIT_EXECUTABLE git REQUIRED)

    set(_clone "${ARG_ARCHIVE}.clone")
    set(_stamp "${ARG_ARCHIVE}.url")

    # The stamp records '<url>@<ref>@<commit>'. Reuse the archive only when the
    # reference still resolves to the recorded commit on the remote: a moving
    # reference (HEAD, a branch, a reused tag) is refreshed when it advances,
    # while a bare commit never needs the round-trip (ls-remote returns
    # nothing for it).
    set(_cached OFF)
    if(EXISTS "${ARG_ARCHIVE}" AND EXISTS "${_clone}" AND EXISTS "${_stamp}")
        file(READ "${_stamp}" _stamp_spec)
        # The CMAKE_MATCH variables cannot be tested inside this if(): they
        # would expand before the regex runs.
        set(_stamp_source "")
        if(_stamp_spec MATCHES "^(.+)@([0-9a-f]+)$")
            set(_stamp_source "${CMAKE_MATCH_1}")
            set(_stamp_commit "${CMAKE_MATCH_2}")
        endif()
        if(_stamp_source STREQUAL "${_url}@${_ref}")
            find_program(GIT_EXECUTABLE git REQUIRED)
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" ls-remote "${_url}" "${_ref}" "${_ref}^{}"
                OUTPUT_VARIABLE _remote_refs
                RESULT_VARIABLE _rc
            )
            if(NOT _rc EQUAL 0)
                # Unreachable remote: keep the cached content.
                message(STATUS "Cannot query ${_url}, keeping the cached archive")
                set(_cached ON)
            elseif(_remote_refs STREQUAL "" OR _remote_refs MATCHES "${_stamp_commit}")
                set(_cached ON)
            endif()
        endif()
    endif()

    if(ARG_OUTPUT_CACHED)
        set(${ARG_OUTPUT_CACHED} ${_cached} PARENT_SCOPE)
    endif()

    if(NOT _cached)
        file(REMOVE "${_stamp}" "${ARG_ARCHIVE}")
        file(REMOVE_RECURSE "${_clone}")
        file(MAKE_DIRECTORY "${_clone}")

        message(STATUS "Cloning ${_url} (${_ref})...")
        _git_run(_rc "${_clone}" init -q)
        if(_rc EQUAL 0)
            _git_run(_rc "${_clone}" remote add origin "${_url}")
        endif()
        if(_rc EQUAL 0)
            _git_run(_rc "${_clone}" fetch --depth 1 origin "${_ref}")
        endif()
        if(_rc EQUAL 0)
            _git_run(_rc "${_clone}" checkout -q --detach FETCH_HEAD)
        endif()
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "clone_repo_archive: failed to clone ${_url} at '${_ref}'")
        endif()

        # A shallow submodule fetch can be refused when the pinned commit is
        # not a branch tip on servers rejecting unadvertised objects.
        _git_run(_rc "${_clone}" submodule update --init --recursive --depth 1)
        if(NOT _rc EQUAL 0)
            message(STATUS "Retrying the submodule update without --depth")
            _git_run(_rc "${_clone}" submodule update --init --recursive)
            if(NOT _rc EQUAL 0)
                message(FATAL_ERROR "clone_repo_archive: failed to fetch the submodules of ${_url}")
            endif()
        endif()

        # Archive the tracked files (of the repository and its submodules):
        # this excludes the .git directories, and the file list goes through a
        # file to stay clear of command line length limits.
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${_clone}" -c core.quotepath=off
                ls-files --recurse-submodules
            OUTPUT_VARIABLE _files
            RESULT_VARIABLE _rc
        )
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "clone_repo_archive: failed to list the files of ${_url}")
        endif()
        string(REPLACE "\r" "" _files "${_files}")
        string(REPLACE "\n" ";" _files "${_files}")
        list(FILTER _files EXCLUDE REGEX "^\\.git")
        list(FILTER _files EXCLUDE REGEX "^$")
        if(ARG_DIRECTORY)
            list(FILTER _files INCLUDE REGEX "^${ARG_DIRECTORY}/")
        endif()
        list(LENGTH _files _count)
        if(_count EQUAL 0)
            message(FATAL_ERROR "clone_repo_archive: nothing to archive from ${_url} (${_ref})")
        endif()

        _zip_files("${ARG_ARCHIVE}" "${_clone}" _files)
        message(STATUS "${_url} (${_ref}) archived: ${ARG_ARCHIVE}")
    endif()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${_clone}" rev-parse HEAD
        OUTPUT_VARIABLE _commit
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _rc
    )
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "clone_repo_archive: failed to resolve the cloned commit")
    endif()
    if(NOT _cached)
        file(WRITE "${_stamp}" "${_url}@${_ref}@${_commit}")
    endif()
    if(ARG_OUTPUT_COMMIT)
        set(${ARG_OUTPUT_COMMIT} "${_commit}" PARENT_SCOPE)
    endif()
endfunction()

# Resolve a sources specification to an archive laid out like the embedded
# resources (entry names relative to the source root) and to its commit when
# one is known:
#   LATEST | <version> | <commit> | <repo-url>[,<ref>] | <directory> | <archive>
# A repository specification is cloned and archived by clone_repo_archive(). A
# local directory is archived at configure time (root '.git*' entries
# excluded), with its commit when it is a git checkout. A local archive is
# used as is (OUTPUT_ARCHIVE is then the specification itself). DIRECTORY
# limits the archive to one subdirectory, whose prefix is kept in the entry
# names.
#
#   fetch_repo_archive(
#       SPEC           "${DFIRORC_DOC}"
#       REPO_URL       https://github.com/DFIR-ORC/dfir-orc.github.io
#       ARCHIVE        "${CMAKE_BINARY_DIR}/dfir-orc.github.io.zip"
#       REF            gh-pages       # optional, see clone_repo_archive()
#       DIRECTORY      docs           # optional
#       OUTPUT_ARCHIVE DFIRORC_DOC_ARCHIVE   # the archive to embed
#       OUTPUT_COMMIT  DFIRORC_DOC_COMMIT    # optional, empty when unknown
#       OUTPUT_CACHED  _cached)              # optional: TRUE when a previous
#                                            # clone and archive were reused
#
function(fetch_repo_archive)
    set(oneValueArgs SPEC REPO_URL ARCHIVE REF DIRECTORY OUTPUT_ARCHIVE OUTPUT_COMMIT OUTPUT_CACHED)
    # PARSE_ARGV keeps ';' inside argument values, which a SPEC using ';' as
    # the reference separator relies on.
    cmake_parse_arguments(PARSE_ARGV 0 ARG "" "${oneValueArgs}" "")

    foreach(required SPEC REPO_URL ARCHIVE OUTPUT_ARCHIVE)
        if(NOT ARG_${required})
            message(FATAL_ERROR "fetch_repo_archive: ${required} is required")
        endif()
    endforeach()

    set(_archive "${ARG_ARCHIVE}")
    set(_commit "")
    set(_cached OFF)

    is_repo_spec("${ARG_SPEC}" _is_repo)
    if(_is_repo)
        set(_optional_args "")
        if(ARG_REF)
            list(APPEND _optional_args REF "${ARG_REF}")
        endif()
        if(ARG_DIRECTORY)
            list(APPEND _optional_args DIRECTORY "${ARG_DIRECTORY}")
        endif()
        clone_repo_archive(
            SPEC "${ARG_SPEC}"
            REPO_URL "${ARG_REPO_URL}"
            ARCHIVE "${ARG_ARCHIVE}"
            ${_optional_args}
            OUTPUT_COMMIT _commit
            OUTPUT_CACHED _cached)
    elseif(NOT EXISTS "${ARG_SPEC}")
        message(FATAL_ERROR "fetch_repo_archive: SPEC does not exist: ${ARG_SPEC}")
    elseif(IS_DIRECTORY "${ARG_SPEC}")
        find_program(GIT_EXECUTABLE git)
        if(GIT_EXECUTABLE)
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" -C "${ARG_SPEC}" rev-parse HEAD
                OUTPUT_VARIABLE _commit
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
                RESULT_VARIABLE _rc
            )
            if(NOT _rc EQUAL 0)
                set(_commit "")
            endif()
        endif()

        file(GLOB_RECURSE _files RELATIVE "${ARG_SPEC}" "${ARG_SPEC}/*")
        list(FILTER _files EXCLUDE REGEX "^\\.git")
        if(ARG_DIRECTORY)
            list(FILTER _files INCLUDE REGEX "^${ARG_DIRECTORY}/")
        endif()
        list(LENGTH _files _count)
        if(_count EQUAL 0)
            message(FATAL_ERROR "fetch_repo_archive: nothing to archive from ${ARG_SPEC}")
        endif()

        _zip_files("${ARG_ARCHIVE}" "${ARG_SPEC}" _files)
        message(STATUS "${ARG_SPEC} archived: ${ARG_ARCHIVE}")
    else()
        # Local archive: used as is.
        set(_archive "${ARG_SPEC}")
    endif()

    set(${ARG_OUTPUT_ARCHIVE} "${_archive}" PARENT_SCOPE)
    if(ARG_OUTPUT_COMMIT)
        set(${ARG_OUTPUT_COMMIT} "${_commit}" PARENT_SCOPE)
    endif()
    if(ARG_OUTPUT_CACHED)
        set(${ARG_OUTPUT_CACHED} ${_cached} PARENT_SCOPE)
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
