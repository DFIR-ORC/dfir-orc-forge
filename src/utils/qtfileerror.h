//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include <system_error>
#include <QFile>

class QFileDeviceErrorCategory : public std::error_category
{
public:
    const char *name() const noexcept override { return "QFileDevice"; }

    std::string message(int condition) const override
    {
        switch (static_cast<QFileDevice::FileError>(condition)) {
        case QFileDevice::NoError:
            return "No error occurred.";
        case QFileDevice::ReadError:
            return "An error occurred when reading from the file.";
        case QFileDevice::WriteError:
            return "An error occurred when writing to the file.";
        case QFileDevice::FatalError:
            return "A fatal error occurred.";
        case QFileDevice::ResourceError:
            return "A resource error occurred.";
        case QFileDevice::OpenError:
            return "The file could not be opened.";
        case QFileDevice::AbortError:
            return "The operation was aborted.";
        case QFileDevice::TimeOutError:
            return "A timeout occurred.";
        case QFileDevice::UnspecifiedError:
            return "An unspecified error occurred.";
        case QFileDevice::RemoveError:
            return "The file could not be removed.";
        case QFileDevice::RenameError:
            return "The file could not be renamed.";
        case QFileDevice::PositionError:
            return "The position in the file could not be changed.";
        case QFileDevice::ResizeError:
            return "The file could not be resized.";
        case QFileDevice::PermissionsError:
            return "The file could not be accessed due to permission issues.";
        case QFileDevice::CopyError:
            return "The file could not be copied.";
        default:
            return "Unknown error.";
        }
    }
};

const QFileDeviceErrorCategory qFileDeviceErrorCategory;

std::error_code make_error_code(QFileDevice::FileError error)
{
    return std::error_code(static_cast<int>(error), qFileDeviceErrorCategory);
}
