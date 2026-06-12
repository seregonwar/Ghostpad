#pragma once

// Ghostpad Native - Platform detection header
// Copyright (c) 2026  seregowar
// Licensed under the GNU General Public License v3.0. See LICENSE file for details.
//
// Detects compilation targets and sets the appropriate platform macros.
//
// Platform macros:
//   GHOSTPAD_IOS       - iOS / iPadOS (defined via CMake: -DGHOSTPAD_IOS)
//   GHOSTPAD_CONSOLE   - PS4 (OpenOrbis) or PS5 (Prospero) native homebrew

#if defined(__ORBIS__) || defined(__PROSPERO__)
    #ifndef GHOSTPAD_CONSOLE
        #define GHOSTPAD_CONSOLE
    #endif
#endif
