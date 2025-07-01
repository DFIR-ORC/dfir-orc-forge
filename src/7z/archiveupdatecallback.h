//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include <filesystem>
#include <optional>
#include <system_error>
#include <vector>

#include <7zip/extras.h>

#include "utils/guard.h"

namespace SevenZip {

class ArchiveUpdateCallback : public IArchiveUpdateCallback,
                              public CMyUnknownImp //, public ICryptoGetTextPassword2
{
public:
    Z7_COM_UNKNOWN_IMP_0
    // IProgress
    STDMETHOD(SetTotal)(UInt64 /* size */) { return S_OK; }
    STDMETHOD(SetCompleted)(const UInt64 *completeValue) { return S_OK; }

    // IUpdateCallback
    STDMETHOD(GetUpdateItemInfo)
    (UInt32 index, Int32 *newData, Int32 *newProperties, UInt32 *indexInArchive);
    STDMETHOD(GetProperty)(UInt32 index, PROPID propID, PROPVARIANT *value);
    STDMETHOD(GetStream)(UInt32 index, ISequentialInStream **inStream);
    STDMETHOD(SetOperationResult)(Int32 operationResult);

    // STDMETHOD( CryptoGetTextPassword2 )
    //( Int32* passwordIsDefined, BSTR* password );

    static Guard::ComPtr<ArchiveUpdateCallback> Create(
        const std::filesystem::path &inputPath, std::error_code &ec);

    static Guard::ComPtr<ArchiveUpdateCallback> Create(
        std::string_view content, const std::wstring &filename, std::error_code &ec);

    const std::error_code &callbackError() const { return m_ec; }

private:
    ArchiveUpdateCallback() {};

    void Init(const std::filesystem::path &inputPath, std::error_code &ec);
    void Init(std::string_view content, const std::wstring &filename, std::error_code &ec);

    NWindows::NFile::NFind::CFileInfo m_fileInfo;
    std::error_code m_ec;
    Guard::ComPtr<ISequentialInStream> m_inStream;
};

} // namespace SevenZip
