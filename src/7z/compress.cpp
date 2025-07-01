//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "compress.h"

#include <7zip/extras.h>

#include "utils/guard.h"
#include "utils/log.h"

#include "archiveupdatecallback.h"
#include "inmemstream.h"
#include "lib7z.h"
#include "outmemstream.h"

namespace fs = std::filesystem;

namespace {

void SetCompressionLevel(IOutArchive *outArchive, UInt32 level, std::error_code &ec)
{
    CMyComPtr<ISetProperties> setProperties;
    HRESULT hr = outArchive->QueryInterface(IID_ISetProperties, (void **) &setProperties);

    if (setProperties == nullptr) {
        ec.assign(hr, std::system_category());
        LOG_WARN("Failed retrieve IID_ISetProperties: {}", ec.message());
        return;
    }

    const uint8_t numProps = 1;
    const wchar_t *names[numProps] = {L"x"};
    NWindows::NCOM::CPropVariant values[numProps] = {level};

    hr = setProperties->SetProperties(names, values, numProps);
    if (FAILED(hr)) {
        ec.assign(hr, std::system_category());
        LOG_WARN("Failed to set property: {}", ec.message());
        return;
    }
}

} // namespace

namespace SevenZip {

void Compress7z(const fs::path &inputFile, std::vector<uint8_t> &out, std::error_code &ec)
{
#ifdef _7ZIP_STATIC
    SevenZip::Lib7z::Instance();
#endif // _7ZIP_STATIC

    Guard::ComPtr<OutMemStream> outStream(new OutMemStream(out));
    Guard::ComPtr<IOutArchive> outArchive;

    HRESULT hr
        = ::CreateObject(&CLSID_CFormat7z, &IID_IOutArchive, reinterpret_cast<void **>(&outArchive));

    if (FAILED(hr)) {
        ec.assign(hr, std::system_category());
        LOG_DEBUG("Failed to get IID_IOutArchive: {}", ec.message());
        return;
    }

    SetCompressionLevel(outArchive.get(), 9, ec);
    if (ec) {
        ec.assign(E_FAIL, std::system_category());
        LOG_DEBUG("Failed to update compression level");
        return;
    }

    auto updateCb = ArchiveUpdateCallback::Create(inputFile, ec);
    if (ec) {
        ec.assign(E_FAIL, std::system_category());
        LOG_DEBUG("Failed to instantiate ArchiveUpdateCallback");
        return;
    }

    hr = outArchive->UpdateItems(outStream.get(), 1, updateCb.get());
    if (FAILED(hr)) {
        ec.assign(hr, std::system_category());
        LOG_DEBUG("Failed to add file to archive: {}", ec.message());
        return;
    }

    // 'UpdateItems' called 'updateCb' methods which could failed
    ec = updateCb->callbackError();
    if (ec) {
        LOG_DEBUG("Failed to compress: {}", ec.message());
        return;
    }
}

void Compress7z(
    std::string_view input,
    const std::wstring &filename,
    std::vector<uint8_t> &out,
    std::error_code &ec)
{
#ifdef _7ZIP_STATIC
    SevenZip::Lib7z::Instance();
#endif // _7ZIP_STATIC

    Guard::ComPtr<OutMemStream> outStream(new OutMemStream(out));
    Guard::ComPtr<IOutArchive> outArchive;

    HRESULT hr
        = ::CreateObject(&CLSID_CFormat7z, &IID_IOutArchive, reinterpret_cast<void **>(&outArchive));

    if (FAILED(hr)) {
        ec.assign(hr, std::system_category());
        LOG_DEBUG("Failed to get IID_IOutArchive: {}", ec.message());
        return;
    }

    SetCompressionLevel(outArchive.get(), 9, ec);
    if (ec) {
        ec.assign(E_FAIL, std::system_category());
        LOG_DEBUG("Failed to update compression level");
        return;
    }

    auto updateCb = ArchiveUpdateCallback::Create(input, filename, ec);
    if (ec) {
        ec.assign(E_FAIL, std::system_category());
        LOG_DEBUG("Failed to instantiate ArchiveUpdateCallback");
        return;
    }

    hr = outArchive->UpdateItems(outStream.get(), 1, updateCb.get());
    if (FAILED(hr)) {
        ec.assign(hr, std::system_category());
        LOG_DEBUG("Failed to add file to archive: {}", ec.message());
        return;
    }

    // 'outArchive' calls 'updateCb' methods which could fail
    ec = updateCb->callbackError();
    if (ec) {
        LOG_DEBUG("Failed to compress: {}", ec.message());
        return;
    }
}

void Compress7z(
    const std::vector<uint8_t> &input,
    const std::wstring &fileNameInArchive,
    std::vector<uint8_t> &out,
    std::error_code &ec)
{
    std::string_view view(reinterpret_cast<const char *>(input.data()), input.size());

    Compress7z(view, fileNameInArchive, out, ec);
}

} // namespace SevenZip
