//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include <system_error>

class HRESULTErrorCategory : public std::error_category
{
public:
    const char *name() const noexcept override;
    std::string message(int condition) const override;
};

inline const HRESULTErrorCategory hresultErrorCategory;

std::error_code make_error_code_from_hr(long hr);
