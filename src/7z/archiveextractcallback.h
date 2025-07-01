//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025-2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include <7zip/7zip.h>
#include <7zip/extras.h>

namespace SevenZip {

class ArchiveExtractCallback : public IArchiveExtractCallback,
                               public ICryptoGetTextPassword,
                               public CMyUnknownImp
{
public:
    Z7_COM_UNKNOWN_IMP_2(IArchiveExtractCallback, ICryptoGetTextPassword)

private:
    CMyComPtr<IInArchive> archiveHandler_;
    UString directoryPath_;
    UString password_;

public:
    ArchiveExtractCallback();

    void Init(IInArchive *archiveHandler, const UString &directoryPath);

    // IProgress
    STDMETHOD(SetTotal)(UInt64 size);
    STDMETHOD(SetCompleted)(const UInt64 *completeValue);

    // IArchiveExtractCallback
    STDMETHOD(GetStream)(UInt32 index, ISequentialOutStream **outStream, Int32 askExtractMode);
    STDMETHOD(PrepareOperation)(Int32 askExtractMode);
    STDMETHOD(SetOperationResult)(Int32 resultEOperationResult);

    // ICryptoGetTextPassword
    STDMETHOD(CryptoGetTextPassword)(BSTR *password);
};

} // namespace SevenZip
