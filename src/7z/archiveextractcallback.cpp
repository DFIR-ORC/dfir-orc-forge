//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "archiveextractcallback.h"

#include "outmemstream.h"
#include "utils/log.h"

namespace SevenZip {

ArchiveExtractCallback::ArchiveExtractCallback() {}

void ArchiveExtractCallback::Init(IInArchive *archiveHandler, const UString &directoryPath)
{
    archiveHandler_ = archiveHandler;
    directoryPath_ = directoryPath;
    directoryPath_.Replace(L'/', L'\\');
    if (!directoryPath_.IsEmpty() && !IsPathSepar(directoryPath_.Back())) {
        directoryPath_ += WCHAR_PATH_SEPARATOR;
    }
}

STDMETHODIMP ArchiveExtractCallback::SetTotal(UInt64 size)
{
    return S_OK;
}

STDMETHODIMP ArchiveExtractCallback::SetCompleted(const UInt64 *completeValue)
{
    if (completeValue) {
        LOG_DEBUG("Completed: {} bytes", *completeValue);
    }

    return S_OK;
}

STDMETHODIMP ArchiveExtractCallback::GetStream(
    UInt32 index, ISequentialOutStream **outStream, Int32 askExtractMode)
{
    *outStream = nullptr;

    if (askExtractMode != NArchive::NExtract::NAskMode::kExtract) {
        return S_OK;
    }

    NWindows::NCOM::CPropVariant prop;
    HRESULT hr = archiveHandler_->GetProperty(index, kpidPath, &prop);
    if (FAILED(hr)) {
        return hr;
    }

    UString fullPath = directoryPath_;
    if (prop.vt == VT_BSTR) {
        fullPath += prop.bstrVal;
    } else if (prop.vt != VT_EMPTY) {
        return E_FAIL;
    } else {
        LOG_CRITICAL(L"Property kpidPath is empty for index #{}", index);
    }

    hr = archiveHandler_->GetProperty(index, kpidIsDir, &prop);
    if (FAILED(hr)) {
        return hr;
    }

    bool isDir = false;
    if (prop.vt == VT_BOOL) {
        isDir = VARIANT_BOOLToBool(prop.boolVal);
    }

    if (isDir) {
        NWindows::NFile::NDir::CreateComplexDir(fullPath);
        return S_OK;
    }

    int slashPos = fullPath.ReverseFind(WCHAR_PATH_SEPARATOR);
    if (slashPos >= 0) {
        NWindows::NFile::NDir::CreateComplexDir(fullPath.Left(slashPos));
    }

    COutFileStream *outFileStreamSpec = new COutFileStream;
    CMyComPtr<ISequentialOutStream> outFileStream(outFileStreamSpec);
    if (!outFileStreamSpec->Create_ALWAYS(fullPath)) {
        LOG_DEBUG(L"Cannot create output file: {}", fullPath.GetBuf());
        return E_FAIL;
    }

    *outStream = outFileStream.Detach();
    return S_OK;
}

STDMETHODIMP ArchiveExtractCallback::PrepareOperation(Int32 askExtractMode)
{
    switch (askExtractMode) {
    case NArchive::NExtract::NAskMode::kExtract:
        break;
    case NArchive::NExtract::NAskMode::kTest:
        break;
    case NArchive::NExtract::NAskMode::kSkip:
        break;
    }

    return S_OK;
}

STDMETHODIMP ArchiveExtractCallback::SetOperationResult(Int32 operationResult)
{
    switch (operationResult) {
    case NArchive::NExtract::NOperationResult::kOK:
        break;
    default:
        break;
    }

    return S_OK;
}

STDMETHODIMP ArchiveExtractCallback::CryptoGetTextPassword(BSTR *password)
{
    return StringToBstr(password_, password);
}

} // namespace SevenZip
