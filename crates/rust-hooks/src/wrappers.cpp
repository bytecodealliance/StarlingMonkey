/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is copied with slight changes from Gecko:
// https://searchfox.org/mozilla-central/rev/91030e74f3e9bb2aac9fe2cbff80734c6fd610b9/mozglue/static/rust/wrappers.cpp

#include "mozilla/Assertions.h"
#include "mozilla/mozalloc_oom.h"

// MOZ_Crash wrapper for use by rust, since MOZ_Crash is an inline function.
extern "C" void RustMozCrash(const char* aFilename, int aLine,
                             const char* aReason) {
  MOZ_Crash(aFilename, aLine, aReason);
}

// mozalloc_handle_oom wrapper for use by rust, because mozalloc_handle_oom is
// MFBT_API, that rust can't respect.
extern "C" void RustHandleOOM(size_t size) { mozalloc_handle_oom(size); }
