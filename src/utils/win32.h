//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include <system_error>

class Win32ErrorCategory : public std::error_category
{
public:
    const char *name() const noexcept override;
    std::string message(int condition) const override;
};

inline const Win32ErrorCategory win32ErrorCategory;

std::error_code make_error_code_from_win32(unsigned long code);
