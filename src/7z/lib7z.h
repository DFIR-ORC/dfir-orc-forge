//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include <7zip/extras.h>

#ifdef _7ZIP_STATIC

// Static linking requires calling specifics functions.
// Call '::Lib7z::Instance()' to initialize singleton
namespace SevenZip {

class Lib7z
{
    Lib7z(const Lib7z &) = delete;
    Lib7z &operator=(const Lib7z &) = delete;

    Lib7z()
    {
        ::lib7zCrcTableInit();

        NArchive::N7z::Register();
        NArchive::NZip::Register();
        NCompress::RegisterCodecCopy();
        NCompress::NBcj::RegisterCodecBCJ();
        NCompress::NBcj2::RegisterCodecBCJ2();
        NCompress::NLzma::RegisterCodecLZMA();
        NCompress::NLzma2::RegisterCodecLZMA2();
        NCompress::NDeflate::RegisterCodecDeflate();
        NCompress::NDeflate::RegisterCodecDeflate64();

        NCrypto::N7z::RegisterCodecSzAES();
    }

public:
    static Lib7z &Instance()
    {
        static Lib7z lib;
        return lib;
    }
};

} // namespace SevenZip

#endif // _7ZIP_STATIC
