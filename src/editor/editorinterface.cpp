//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "editorinterface.h"

EditorInterface::~EditorInterface() {}

EditorSignals &EditorInterface::editorSignals()
{
    return editorSignals_;
}

EditorSignals &EditorInterface::editorSignals() const
{
    return editorSignals_;
}

#include "editorinterface.moc"
