//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025-2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "extract.h"

#include <expected>

#include <7zip/7zip.h>
#include <7zip/extras.h>

#include "archiveextractcallback.h"
#include "inmemstream.h"
#include "lib7z.h"
#include "utils/guard.h"
#include "utils/hresult.h"
#include "utils/log.h"

namespace SevenZip {

std::expected<void, std::error_code> ExtractArchive(
    const GUID &clsidFormat, const std::wstring &archivePath, const std::wstring &outputDir)
{
#ifdef _7ZIP_STATIC
    SevenZip::Lib7z::Instance();
#endif // _7ZIP_STATIC

    CInFileStream *fileStreamSpec = new CInFileStream;
    CMyComPtr<IInStream> fileStream(fileStreamSpec);
    if (!fileStreamSpec->Open(archivePath.c_str())) {
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }

    CMyComPtr<IInArchive> archive;
    HRESULT hr = ::CreateObject(&clsidFormat, &IID_IInArchive, reinterpret_cast<void **>(&archive));
    if (FAILED(hr)) {
        return std::unexpected(make_error_code_from_hr(hr));
    }

    // 'Open' returns S_FALSE when the data does not match the requested format
    hr = archive->Open(fileStreamSpec, 0, NULL);
    if (hr != S_OK) {
        return std::unexpected(
            FAILED(hr) ? make_error_code_from_hr(hr)
                       : std::make_error_code(std::errc::not_supported));
    }

    ArchiveExtractCallback *extractCallbackSpec = new ArchiveExtractCallback;
    CMyComPtr<IArchiveExtractCallback> extractCallback(extractCallbackSpec);
    extractCallbackSpec->Init(archive, outputDir.c_str());

    hr = archive->Extract(NULL, (UInt32) (Int32) (-1), false, extractCallback);
    if (FAILED(hr)) {
        return std::unexpected(make_error_code_from_hr(hr));
    }

    return {};
}

std::expected<void, std::error_code> Extract7z(
    const std::wstring &archivePath, const std::wstring &outputDir)
{
    return ExtractArchive(CLSID_CFormat7z, archivePath, outputDir);
}

std::expected<void, std::error_code> ExtractZip(
    const std::wstring &archivePath, const std::wstring &outputDir)
{
    return ExtractArchive(CLSID_CFormatZip, archivePath, outputDir);
}

std::expected<void, std::error_code> ExtractArchive(
    const std::wstring &archivePath, const std::wstring &outputDir)
{
    auto rv = Extract7z(archivePath, outputDir);
    if (rv) {
        return {};
    }

    rv = ExtractZip(archivePath, outputDir);
    if (rv) {
        return {};
    }

    return std::unexpected(std::make_error_code(std::errc::not_supported));
}

std::expected<void, std::error_code> ExtractArchive(
    const GUID &clsidFormat, const std::string_view buffer, const std::wstring &outputDir)
{
#ifdef _7ZIP_STATIC
    SevenZip::Lib7z::Instance();
#endif // _7ZIP_STATIC

    InMemStream *memStreamSpec = new InMemStream(buffer);

    CMyComPtr<IInStream> memStream = memStreamSpec;

    CMyComPtr<IInArchive> archive;
    HRESULT hr = ::CreateObject(&clsidFormat, &IID_IInArchive, reinterpret_cast<void **>(&archive));
    if (FAILED(hr)) {
        return std::unexpected(make_error_code_from_hr(hr));
    }

    // 'Open' returns S_FALSE when the data does not match the requested format
    hr = archive->Open(memStream, 0, NULL);
    if (hr != S_OK) {
        return std::unexpected(
            FAILED(hr) ? make_error_code_from_hr(hr)
                       : std::make_error_code(std::errc::not_supported));
    }

    ArchiveExtractCallback *extractCallbackSpec = new ArchiveExtractCallback;
    CMyComPtr<IArchiveExtractCallback> extractCallback(extractCallbackSpec);
    extractCallbackSpec->Init(archive, outputDir.c_str());

    hr = archive->Extract(NULL, -1, false, extractCallback);
    if (FAILED(hr)) {
        return std::unexpected(make_error_code_from_hr(hr));
    }

    return {};
}

std::expected<void, std::error_code> Extract7z(
    const std::string_view archive, const std::wstring &outputDir)
{
    return ExtractArchive(CLSID_CFormat7z, archive, outputDir);
}

std::expected<void, std::error_code> ExtractZip(
    const std::string_view archive, const std::wstring &outputDir)
{
    return ExtractArchive(CLSID_CFormatZip, archive, outputDir);
}

std::expected<void, std::error_code> ExtractArchive(
    const std::string_view buffer, const std::wstring &outputDir)
{
    auto rv = Extract7z(buffer, outputDir);
    if (rv) {
        return {};
    }

    rv = ExtractZip(buffer, outputDir);
    if (rv) {
        return {};
    }

    return std::unexpected(std::make_error_code(std::errc::not_supported));
}

} // namespace SevenZip
