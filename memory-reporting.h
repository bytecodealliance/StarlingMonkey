/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Very basic reporting of memory usage, printing to stdout.
 * Adapted from
 * https://searchfox.org/mozilla-central/rev/6a2fc29e9c69deba4798c18d445b92df7adcbcc7/js/xpconnect/src/XPCJSRuntime.cpp#1270-2014
 */

#include "js/HeapAPI.h"
#include "js/MemoryMetrics.h"

// The REPORT* macros do an unconditional report.  The ZRREPORT* macros are for
// realms and zones; they aggregate any entries smaller than
// SUNDRIES_THRESHOLD into the "sundries/gc-heap" and "sundries/malloc-heap"
// entries for the realm.

#define KIND_HEAP "heap"
#define KIND_NONHEAP "non-heap"
#define KIND_OTHER "other"
#define UNITS_BYTES "bytes"

#define SUNDRIES_THRESHOLD js::MemoryReportingSundriesThreshold()

#define REPORT(_path, _kind, _units, _amount, _desc)                                               \
  printf("%s alloc, %s: %zu %s\n", _kind, (_path).c_str(), _amount, _units);

#define REPORT_BYTES(_path, _kind, _amount, _desc)                                                 \
  REPORT(_path, _kind, UNITS_BYTES, _amount, _desc);

#define REPORT_GC_BYTES(_path, _amount, _desc)                                                     \
  do {                                                                                             \
    size_t amount = _amount; /* evaluate _amount only once */                                      \
    printf("heap alloc, %s: %zu\n", (_path).c_str(), amount);                                      \
    gcTotal += amount;                                                                             \
  } while (0)

// Report realm/zone non-GC (KIND_HEAP) bytes.
#define ZRREPORT_BYTES(_path, _amount, _desc)                                                      \
  do {                                                                                             \
    /* Assign _descLiteral plus "" into a char* to prove that it's */                              \
    /* actually a literal. */                                                                      \
    size_t amount = _amount; /* evaluate _amount only once */                                      \
    if (amount >= SUNDRIES_THRESHOLD) {                                                            \
      printf("zone heap alloc, %s: %zu\n", (_path).c_str(), amount);                               \
    } else {                                                                                       \
      sundriesMallocHeap += amount;                                                                \
    }                                                                                              \
  } while (0)

// Report realm/zone GC bytes.
#define ZRREPORT_GC_BYTES(_path, _amount, _desc)                                                   \
  do {                                                                                             \
    size_t amount = _amount; /* evaluate _amount only once */                                      \
    if (amount >= SUNDRIES_THRESHOLD) {                                                            \
      printf("zone gc alloc, %s: %zu\n", (_path).c_str(), amount);                                 \
      gcTotal += amount;                                                                           \
    } else {                                                                                       \
      sundriesGCHeap += amount;                                                                    \
    }                                                                                              \
  } while (0)

// Report realm/zone non-heap bytes.
#define ZRREPORT_NONHEAP_BYTES(_path, _amount, _desc)                                              \
  do {                                                                                             \
    size_t amount = _amount; /* evaluate _amount only once */                                      \
    if (amount >= SUNDRIES_THRESHOLD) {                                                            \
      printf("zone non-heap alloc, %s: %zu\n", (_path).c_str(), amount);                           \
    } else {                                                                                       \
      sundriesNonHeap += amount;                                                                   \
    }                                                                                              \
  } while (0)

// Report runtime bytes.
#define RREPORT_BYTES(_path, _kind, _amount, _desc)                                                \
  do {                                                                                             \
    size_t amount = _amount; /* evaluate _amount only once */                                      \
    printf("runtime heap alloc, %s: %zu\n", (_path).c_str(), amount);                              \
    rtTotal += amount;                                                                             \
  } while (0)

#include <memory>

template <typename... Args> std::string string_format(const char *format, Args... args) {
  int size = snprintf(nullptr, 0, format, args...);
  if (size <= 0)
    return nullptr;
  std::unique_ptr<char[]> buf(new char[size + 1]); // Extra space for '\0'
  snprintf(buf.get(), size, format, args...);
  return std::string(buf.get(),
                     buf.get() + size); // We don't want the '\0' inside
}

static void ReportZoneStats(const JS::ZoneStats &zStats, nsISupports *data, bool anonymize,
                            size_t *gcTotalOut = nullptr) {
  std::string pathPrefix("zone/");
  size_t gcTotal = 0;
  size_t sundriesGCHeap = 0;
  size_t sundriesMallocHeap = 0;
  size_t sundriesNonHeap = 0;

  MOZ_ASSERT(!gcTotalOut == zStats.isTotals);

  ZRREPORT_GC_BYTES(pathPrefix + "symbols/gc-heap", zStats.symbolsGCHeap, "Symbols.");

  ZRREPORT_GC_BYTES(pathPrefix + "gc-heap-arena-admin", zStats.gcHeapArenaAdmin,
                    "Bookkeeping information and alignment padding within GC arenas.");

  ZRREPORT_GC_BYTES(pathPrefix + "unused-gc-things", zStats.unusedGCThings.totalSize(),
                    "Unused GC thing cells within non-empty arenas.");

  ZRREPORT_BYTES(pathPrefix + "unique-id-map", zStats.uniqueIdMap,
                 "Address-independent cell identities.");

  ZRREPORT_BYTES(pathPrefix + "shape-tables", zStats.shapeTables,
                 "Tables storing shape information.");

  ZRREPORT_BYTES(pathPrefix + "compartments/compartment-objects", zStats.compartmentObjects,
                 "The JS::Compartment objects in this zone.");

  ZRREPORT_BYTES(pathPrefix + "compartments/cross-compartment-wrapper-tables",
                 zStats.crossCompartmentWrappersTables, "The cross-compartment wrapper tables.");

  ZRREPORT_BYTES(pathPrefix + "compartments/private-data", zStats.compartmentsPrivateData,
                 "Extra data attached to each compartment by XPConnect, including "
                 "its wrapped-js.");

  ZRREPORT_GC_BYTES(pathPrefix + "jit-codes-gc-heap", zStats.jitCodesGCHeap,
                    "References to executable code pools used by the JITs.");

  ZRREPORT_GC_BYTES(pathPrefix + "scopes/gc-heap", zStats.scopesGCHeap,
                    "Scope information for scripts.");

  ZRREPORT_BYTES(pathPrefix + "scopes/malloc-heap", zStats.scopesMallocHeap,
                 "Arrays of binding names and other binding-related data.");

  ZRREPORT_GC_BYTES(pathPrefix + "regexp-shareds/gc-heap", zStats.regExpSharedsGCHeap,
                    "Shared compiled regexp data.");

  ZRREPORT_BYTES(pathPrefix + "regexp-shareds/malloc-heap", zStats.regExpSharedsMallocHeap,
                 "Shared compiled regexp data.");

  ZRREPORT_BYTES(pathPrefix + "regexp-zone", zStats.regexpZone, "The regexp zone and regexp data.");

  ZRREPORT_BYTES(pathPrefix + "jit-zone", zStats.jitZone, "The JIT zone.");

  ZRREPORT_BYTES(pathPrefix + "baseline/optimized-stubs", zStats.baselineStubsOptimized,
                 "The Baseline JIT's optimized IC stubs (excluding code).");

  ZRREPORT_BYTES(pathPrefix + "script-counts-map", zStats.scriptCountsMap,
                 "Profiling-related information for scripts.");

  ZRREPORT_NONHEAP_BYTES(pathPrefix + "code/ion", zStats.code.ion,
                         "Code generated by the IonMonkey JIT.");

  ZRREPORT_NONHEAP_BYTES(pathPrefix + "code/baseline", zStats.code.baseline,
                         "Code generated by the Baseline JIT.");

  ZRREPORT_NONHEAP_BYTES(pathPrefix + "code/regexp", zStats.code.regexp,
                         "Code generated by the regexp JIT.");

  ZRREPORT_NONHEAP_BYTES(pathPrefix + "code/other", zStats.code.other,
                         "Code generated by the JITs for wrappers and trampolines.");

  ZRREPORT_NONHEAP_BYTES(pathPrefix + "code/unused", zStats.code.unused,
                         "Memory allocated by one of the JITs to hold code, "
                         "but which is currently unused.");

  size_t stringsNotableAboutMemoryGCHeap = 0;
  size_t stringsNotableAboutMemoryMallocHeap = 0;

#define MAYBE_INLINE "The characters may be inline or on the malloc heap."
#define MAYBE_OVERALLOCATED "Sometimes over-allocated to simplify string concatenation."

  for (size_t i = 0; i < zStats.notableStrings.length(); i++) {
    const JS::NotableStringInfo &info = zStats.notableStrings[i];

    MOZ_ASSERT(!zStats.isTotals);

    // We don't do notable string detection when anonymizing, because
    // there's a good chance its for crash submission, and the memory
    // required for notable string detection is high.
    MOZ_ASSERT(!anonymize);

    // Viewing about:memory generates many notable strings which contain
    // "string(length=".  If we report these as notable, then we'll create
    // even more notable strings the next time we open about:memory (unless
    // there's a GC in the meantime), and so on ad infinitum.
    //
    // To avoid cluttering up about:memory like this, we stick notable
    // strings which contain "string(length=" into their own bucket.
#define STRING_LENGTH "string(length="
    // if (FindInReadable(nsLiteralCString(STRING_LENGTH), notableString)) {
    //   stringsNotableAboutMemoryGCHeap += info.gcHeapLatin1;
    //   stringsNotableAboutMemoryGCHeap += info.gcHeapTwoByte;
    //   stringsNotableAboutMemoryMallocHeap += info.mallocHeapLatin1;
    //   stringsNotableAboutMemoryMallocHeap += info.mallocHeapTwoByte;
    //   continue;
    // }

    std::string notableString(info.buffer.get());

    // Escape / to \ before we put notableString into the memory reporter
    // path, because we don't want any forward slashes in the string to
    // count as path separators.
    std::string escapedString(notableString);
    // escapedString.ReplaceSubstring("/", "\\");

    bool truncated = notableString.length() < info.length;

    std::string path =
        string_format("strings/" STRING_LENGTH "%zu, copies=%d, \"%s\"%s)/", info.length,
                      info.numCopies, escapedString.c_str(), truncated ? " (truncated)" : "");

    if (info.gcHeapLatin1 > 0) {
      REPORT_GC_BYTES(path + "gc-heap/latin1", info.gcHeapLatin1, "Latin1 strings. " MAYBE_INLINE);
    }

    if (info.gcHeapTwoByte > 0) {
      REPORT_GC_BYTES(path + "gc-heap/two-byte", info.gcHeapTwoByte,
                      "TwoByte strings. " MAYBE_INLINE);
    }

    if (info.mallocHeapLatin1 > 0) {
      REPORT_BYTES(path + "malloc-heap/latin1", KIND_HEAP, info.mallocHeapLatin1,
                   "Non-inline Latin1 string characters. " MAYBE_OVERALLOCATED);
    }

    if (info.mallocHeapTwoByte > 0) {
      REPORT_BYTES(path + "malloc-heap/two-byte", KIND_HEAP, info.mallocHeapTwoByte,
                   "Non-inline TwoByte string characters. " MAYBE_OVERALLOCATED);
    }
  }

  std::string nonNotablePath = "";
  nonNotablePath +=
      (zStats.isTotals || anonymize) ? "strings/" : "strings/string(<non-notable strings>)/";

  if (zStats.stringInfo.gcHeapLatin1 > 0) {
    REPORT_GC_BYTES(nonNotablePath + "gc-heap/latin1", zStats.stringInfo.gcHeapLatin1,
                    "Latin1 strings. " MAYBE_INLINE);
  }

  if (zStats.stringInfo.gcHeapTwoByte > 0) {
    REPORT_GC_BYTES(nonNotablePath + "gc-heap/two-byte", zStats.stringInfo.gcHeapTwoByte,
                    "TwoByte strings. " MAYBE_INLINE);
  }

  if (zStats.stringInfo.mallocHeapLatin1 > 0) {
    REPORT_BYTES(nonNotablePath + "malloc-heap/latin1", KIND_HEAP,
                 zStats.stringInfo.mallocHeapLatin1,
                 "Non-inline Latin1 string characters. " MAYBE_OVERALLOCATED);
  }

  if (zStats.stringInfo.mallocHeapTwoByte > 0) {
    REPORT_BYTES(nonNotablePath + "malloc-heap/two-byte", KIND_HEAP,
                 zStats.stringInfo.mallocHeapTwoByte,
                 "Non-inline TwoByte string characters. " MAYBE_OVERALLOCATED);
  }

  if (stringsNotableAboutMemoryGCHeap > 0) {
    MOZ_ASSERT(!zStats.isTotals);
    REPORT_GC_BYTES(pathPrefix + "strings/string(<about-memory>)/gc-heap",
                    stringsNotableAboutMemoryGCHeap,
                    "Strings that contain the characters '" STRING_LENGTH "', which "
                    "are probably from about:memory itself." MAYBE_INLINE
                    " We filter them out rather than display them, because displaying "
                    "them would create even more such strings every time about:memory "
                    "is refreshed.");
  }

  if (stringsNotableAboutMemoryMallocHeap > 0) {
    MOZ_ASSERT(!zStats.isTotals);
    REPORT_BYTES(pathPrefix + "strings/string(<about-memory>)/malloc-heap", KIND_HEAP,
                 stringsNotableAboutMemoryMallocHeap,
                 "Non-inline string characters of strings that contain the "
                 "characters '" STRING_LENGTH "', which are probably from "
                 "about:memory itself. " MAYBE_OVERALLOCATED
                 " We filter them out rather than display them, because displaying "
                 "them would create even more such strings every time about:memory "
                 "is refreshed.");
  }

  const JS::ShapeInfo &shapeInfo = zStats.shapeInfo;
  if (shapeInfo.shapesGCHeapTree > 0) {
    REPORT_GC_BYTES(pathPrefix + "shapes/gc-heap/tree", shapeInfo.shapesGCHeapTree,
                    "Shapes in a property tree.");
  }

  if (shapeInfo.shapesGCHeapDict > 0) {
    REPORT_GC_BYTES(pathPrefix + "shapes/gc-heap/dict", shapeInfo.shapesGCHeapDict,
                    "Shapes in dictionary mode.");
  }

  if (shapeInfo.shapesGCHeapBase > 0) {
    REPORT_GC_BYTES(pathPrefix + "shapes/gc-heap/base", shapeInfo.shapesGCHeapBase,
                    "Base shapes, which collate data common to many shapes.");
  }

  if (shapeInfo.shapesMallocHeapTreeTables > 0) {
    REPORT_BYTES(pathPrefix + "shapes/malloc-heap/tree-tables", KIND_HEAP,
                 shapeInfo.shapesMallocHeapTreeTables,
                 "Property tables of shapes in a property tree.");
  }

  if (shapeInfo.shapesMallocHeapDictTables > 0) {
    REPORT_BYTES(pathPrefix + "shapes/malloc-heap/dict-tables", KIND_HEAP,
                 shapeInfo.shapesMallocHeapDictTables,
                 "Property tables of shapes in dictionary mode.");
  }

  if (shapeInfo.shapesMallocHeapTreeChildren > 0) {
    REPORT_BYTES(pathPrefix + "shapes/malloc-heap/tree-children", KIND_HEAP,
                 shapeInfo.shapesMallocHeapTreeChildren,
                 "Sets of shape children in a property tree.");
  }

  if (sundriesGCHeap > 0) {
    // We deliberately don't use ZRREPORT_GC_BYTES here.
    REPORT_GC_BYTES(pathPrefix + "sundries/gc-heap", sundriesGCHeap,
                    "The sum of all 'gc-heap' measurements that are too small to be "
                    "worth showing individually.");
  }

  if (sundriesMallocHeap > 0) {
    // We deliberately don't use ZRREPORT_BYTES here.
    REPORT_BYTES(pathPrefix + "sundries/malloc-heap", KIND_HEAP, sundriesMallocHeap,
                 "The sum of all 'malloc-heap' measurements that are too small to "
                 "be worth showing individually.");
  }

  if (sundriesNonHeap > 0) {
    // We deliberately don't use ZRREPORT_NONHEAP_BYTES here.
    REPORT_BYTES(pathPrefix + "sundries/other-heap", KIND_NONHEAP, sundriesNonHeap,
                 "The sum of non-malloc/gc measurements that are too small to "
                 "be worth showing individually.");
  }

  if (gcTotalOut) {
    *gcTotalOut += gcTotal;
  }

#undef STRING_LENGTH
}

static void ReportClassStats(const JS::ClassInfo &classInfo, const std::string &path,
                             nsISupports *data, size_t &gcTotal) {
  // We deliberately don't use ZRREPORT_BYTES, so that these per-class values
  // don't go into sundries.

  if (classInfo.objectsGCHeap > 0) {
    REPORT_GC_BYTES(path + "objects/gc-heap", classInfo.objectsGCHeap,
                    "Objects, including fixed slots.");
  }

  if (classInfo.objectsMallocHeapSlots > 0) {
    REPORT_BYTES(path + "objects/malloc-heap/slots", KIND_HEAP, classInfo.objectsMallocHeapSlots,
                 "Non-fixed object slots.");
  }

  if (classInfo.objectsMallocHeapElementsNormal > 0) {
    REPORT_BYTES(path + "objects/malloc-heap/elements/normal", KIND_HEAP,
                 classInfo.objectsMallocHeapElementsNormal, "Normal (non-wasm) indexed elements.");
  }

  if (classInfo.objectsMallocHeapElementsAsmJS > 0) {
    REPORT_BYTES(path + "objects/malloc-heap/elements/asm.js", KIND_HEAP,
                 classInfo.objectsMallocHeapElementsAsmJS,
                 "asm.js array buffer elements allocated in the malloc heap.");
  }

  if (classInfo.objectsMallocHeapMisc > 0) {
    REPORT_BYTES(path + "objects/malloc-heap/misc", KIND_HEAP, classInfo.objectsMallocHeapMisc,
                 "Miscellaneous object data.");
  }

  if (classInfo.objectsNonHeapElementsNormal > 0) {
    REPORT_BYTES(path + "objects/non-heap/elements/normal", KIND_NONHEAP,
                 classInfo.objectsNonHeapElementsNormal,
                 "Memory-mapped non-shared array buffer elements.");
  }

  if (classInfo.objectsNonHeapElementsShared > 0) {
    REPORT_BYTES(path + "objects/non-heap/elements/shared", KIND_NONHEAP,
                 classInfo.objectsNonHeapElementsShared,
                 "Memory-mapped shared array buffer elements. These elements are "
                 "shared between one or more runtimes; the reported size is divided "
                 "by the buffer's refcount.");
  }

  // WebAssembly memories are always non-heap-allocated (mmap). We never put
  // these under sundries, because (a) in practice they're almost always
  // larger than the sundries threshold, and (b) we'd need a third category of
  // sundries ("non-heap"), which would be a pain.
  if (classInfo.objectsNonHeapElementsWasm > 0) {
    REPORT_BYTES(path + "objects/non-heap/elements/wasm", KIND_NONHEAP,
                 classInfo.objectsNonHeapElementsWasm,
                 "wasm/asm.js array buffer elements allocated outside both the "
                 "malloc heap and the GC heap.");
  }

  if (classInfo.objectsNonHeapCodeWasm > 0) {
    REPORT_BYTES(path + "objects/non-heap/code/wasm", KIND_NONHEAP,
                 classInfo.objectsNonHeapCodeWasm, "AOT-compiled wasm/asm.js code.");
  }

  // Although wasm guard pages aren't committed in memory they can be very
  // large and contribute greatly to vsize and so are worth reporting.
  // if (classInfo.wasmGuardPages > 0) {
  //   REPORT_BYTES(
  //       "wasm-guard-pages", KIND_OTHER, classInfo.wasmGuardPages,
  //       "Guard pages mapped after the end of wasm memories, reserved for "
  //       "optimization tricks, but not committed and thus never contributing"
  //       " to RSS, only vsize.");
  // }
}

static void ReportRealmStats(const JS::RealmStats &realmStats, nsISupports *data,
                             size_t *gcTotalOut = nullptr) {

  size_t gcTotal = 0, sundriesGCHeap = 0, sundriesMallocHeap = 0;
  std::string realmJSPathPrefix("js/");

  MOZ_ASSERT(!gcTotalOut == realmStats.isTotals);

  std::string nonNotablePath = realmJSPathPrefix;
  nonNotablePath += realmStats.isTotals ? "classes/" : "classes/class(<non-notable classes>)/";

  ReportClassStats(realmStats.classInfo, nonNotablePath, data, gcTotal);

  for (size_t i = 0; i < realmStats.notableClasses.length(); i++) {
    MOZ_ASSERT(!realmStats.isTotals);
    const JS::NotableClassInfo &classInfo = realmStats.notableClasses[i];

    std::string classPath = string_format("classes/class(%s)/", classInfo.className_.get());

    ReportClassStats(classInfo, classPath, data, gcTotal);
  }

  // Note that we use realmDOMPathPrefix here.  This is because we measure
  // orphan DOM nodes in the JS reporter, but we want to report them in a "dom"
  // sub-tree rather than a "js" sub-tree.
  // ZRREPORT_BYTES(
  //     "orphan-nodes", realmStats.objectsPrivate,
  //     "Orphan DOM nodes, i.e. those that are only reachable from JavaScript "
  //     "objects.");

  ZRREPORT_GC_BYTES(realmJSPathPrefix + "scripts/gc-heap", realmStats.scriptsGCHeap,
                    "JSScript instances. There is one per user-defined function in a "
                    "script, and one for the top-level code in a script.");

  ZRREPORT_BYTES(realmJSPathPrefix + "scripts/malloc-heap/data", realmStats.scriptsMallocHeapData,
                 "Various variable-length tables in JSScripts.");

  ZRREPORT_BYTES(realmJSPathPrefix + "baseline/data", realmStats.baselineData,
                 "The Baseline JIT's compilation data (BaselineScripts).");

  ZRREPORT_BYTES(realmJSPathPrefix + "baseline/fallback-stubs", realmStats.baselineStubsFallback,
                 "The Baseline JIT's fallback IC stubs (excluding code).");

  ZRREPORT_BYTES(realmJSPathPrefix + "ion-data", realmStats.ionData,
                 "The IonMonkey JIT's compilation data (IonScripts).");

  ZRREPORT_BYTES(realmJSPathPrefix + "jit-scripts", realmStats.jitScripts,
                 "JIT data associated with scripts.");

  ZRREPORT_BYTES(realmJSPathPrefix + "realm-object", realmStats.realmObject,
                 "The JS::Realm object itself.");

  ZRREPORT_BYTES(realmJSPathPrefix + "realm-tables", realmStats.realmTables,
                 "Realm-wide tables storing object group information and wasm instances.");

  ZRREPORT_BYTES(realmJSPathPrefix + "inner-views", realmStats.innerViewsTable,
                 "The table for array buffer inner views.");

  ZRREPORT_BYTES(realmJSPathPrefix + "object-metadata", realmStats.objectMetadataTable,
                 "The table used by debugging tools for tracking object metadata");

  ZRREPORT_BYTES(realmJSPathPrefix + "saved-stacks-set", realmStats.savedStacksSet,
                 "The saved stacks set.");

  ZRREPORT_BYTES(realmJSPathPrefix + "non-syntactic-lexical-scopes-table",
                 realmStats.nonSyntacticLexicalScopesTable,
                 "The non-syntactic lexical scopes table.");

  ZRREPORT_BYTES(realmJSPathPrefix + "jit-realm", realmStats.jitRealm, "The JIT realm.");

  if (sundriesGCHeap > 0) {
    // We deliberately don't use ZRREPORT_GC_BYTES here.
    REPORT_GC_BYTES(realmJSPathPrefix + "sundries/gc-heap", sundriesGCHeap,
                    "The sum of all 'gc-heap' measurements that are too small to be "
                    "worth showing individually.");
  }

  if (sundriesMallocHeap > 0) {
    // We deliberately don't use ZRREPORT_BYTES here.
    REPORT_BYTES(realmJSPathPrefix + "sundries/malloc-heap", KIND_HEAP, sundriesMallocHeap,
                 "The sum of all 'malloc-heap' measurements that are too small to "
                 "be worth showing individually.");
  }

  if (gcTotalOut) {
    *gcTotalOut += gcTotal;
  }
}

static void ReportScriptSourceStats(const JS::ScriptSourceInfo &scriptSourceInfo,
                                    const std::string &path, nsISupports *data, size_t &rtTotal) {
  if (scriptSourceInfo.misc > 0) {
    RREPORT_BYTES(path + "misc", KIND_HEAP, scriptSourceInfo.misc,
                  "Miscellaneous data relating to JavaScript source code.");
  }
}

void ReportJSRuntimeExplicitTreeStats(const JS::RuntimeStats &rtStats, const std::string &rtPath,
                                      nsISupports *data, bool anonymize, size_t *rtTotalOut) {
  size_t gcTotal = 0;

  printf("Zone stats:\n");
  for (size_t i = 0; i < rtStats.zoneStatsVector.length(); i++) {
    printf("Zone %zu:\n", i);
    const JS::ZoneStats &zStats = rtStats.zoneStatsVector[i];
    ReportZoneStats(zStats, data, anonymize, &gcTotal);
  }

  printf("Realm stats:\n");
  for (size_t i = 0; i < rtStats.realmStatsVector.length(); i++) {
    printf("Realm %zu:\n", i);
    const JS::RealmStats &realmStats = rtStats.realmStatsVector[i];
    ReportRealmStats(realmStats, data, &gcTotal);
  }

  // Report the rtStats.runtime numbers under "runtime/", and compute their
  // total for later.

  size_t rtTotal = 0;

  RREPORT_BYTES(rtPath + "runtime/runtime-object", KIND_HEAP, rtStats.runtime.object,
                "The JSRuntime object.");

  RREPORT_BYTES(rtPath + "runtime/atoms-table", KIND_HEAP, rtStats.runtime.atomsTable,
                "The atoms table.");

  RREPORT_BYTES(rtPath + "runtime/atoms-mark-bitmaps", KIND_HEAP, rtStats.runtime.atomsMarkBitmaps,
                "Mark bitmaps for atoms held by each zone.");

  RREPORT_BYTES(rtPath + "runtime/contexts", KIND_HEAP, rtStats.runtime.contexts,
                "JSContext objects and structures that belong to them.");

  RREPORT_BYTES(rtPath + "runtime/temporary", KIND_HEAP, rtStats.runtime.temporary,
                "Transient data (mostly parse nodes) held by the JSRuntime during "
                "compilation.");

  RREPORT_BYTES(rtPath + "runtime/interpreter-stack", KIND_HEAP, rtStats.runtime.interpreterStack,
                "JS interpreter frames.");

  RREPORT_BYTES(rtPath + "runtime/shared-immutable-strings-cache", KIND_HEAP,
                rtStats.runtime.sharedImmutableStringsCache,
                "Immutable strings (such as JS scripts' source text) shared across all "
                "JSRuntimes.");

  RREPORT_BYTES(rtPath + "runtime/shared-intl-data", KIND_HEAP, rtStats.runtime.sharedIntlData,
                "Shared internationalization data.");

  RREPORT_BYTES(rtPath + "runtime/uncompressed-source-cache", KIND_HEAP,
                rtStats.runtime.uncompressedSourceCache, "The uncompressed source code cache.");

  RREPORT_BYTES(rtPath + "runtime/script-data", KIND_HEAP, rtStats.runtime.scriptData,
                "The table holding script data shared in the runtime.");

  RREPORT_BYTES(rtPath + "runtime/tracelogger", KIND_HEAP, rtStats.runtime.tracelogger,
                "The memory used for the tracelogger (per-runtime).");

  std::string nonNotablePath =
      rtPath + string_format("runtime/script-sources/source(scripts=%d, <non-notable files>)/",
                             rtStats.runtime.scriptSourceInfo.numScripts);

  ReportScriptSourceStats(rtStats.runtime.scriptSourceInfo, nonNotablePath, data, rtTotal);

  for (size_t i = 0; i < rtStats.runtime.notableScriptSources.length(); i++) {
    const JS::NotableScriptSourceInfo &scriptSourceInfo = rtStats.runtime.notableScriptSources[i];

    // Escape / to \ before we put the filename into the memory reporter
    // path, because we don't want any forward slashes in the string to
    // count as path separators. Consumers of memory reporters (e.g.
    // about:memory) will convert them back to / after doing path
    // splitting.
    std::string escapedFilename = "";
    if (anonymize) {
      // escapedFilename.AppendPrintf("<anonymized-source-%d>", int(i));
    } else {
      std::string filename(scriptSourceInfo.filename_.get());
      escapedFilename.append(filename);
      // escapedFilename.ReplaceSubstring("/", "\\");
    }

    std::string notablePath =
        rtPath + string_format("runtime/script-sources/source(scripts=%d, %s)/",
                               scriptSourceInfo.numScripts, escapedFilename.c_str());

    ReportScriptSourceStats(scriptSourceInfo, notablePath, data, rtTotal);
  }

  RREPORT_BYTES(rtPath + "runtime/gc/marker", KIND_HEAP, rtStats.runtime.gc.marker,
                "The GC mark stack and gray roots.");

  RREPORT_BYTES(rtPath + "runtime/gc/nursery-committed", KIND_NONHEAP,
                rtStats.runtime.gc.nurseryCommitted, "Memory being used by the GC's nursery.");

  RREPORT_BYTES(rtPath + "runtime/gc/nursery-malloced-buffers", KIND_HEAP,
                rtStats.runtime.gc.nurseryMallocedBuffers,
                "Out-of-line slots and elements belonging to objects in the nursery.");

  RREPORT_BYTES(rtPath + "runtime/gc/store-buffer/vals", KIND_HEAP,
                rtStats.runtime.gc.storeBufferVals, "Values in the store buffer.");

  RREPORT_BYTES(rtPath + "runtime/gc/store-buffer/cells", KIND_HEAP,
                rtStats.runtime.gc.storeBufferCells, "Cells in the store buffer.");

  RREPORT_BYTES(rtPath + "runtime/gc/store-buffer/slots", KIND_HEAP,
                rtStats.runtime.gc.storeBufferSlots, "Slots in the store buffer.");

  RREPORT_BYTES(rtPath + "runtime/gc/store-buffer/whole-cells", KIND_HEAP,
                rtStats.runtime.gc.storeBufferWholeCells, "Whole cells in the store buffer.");

  RREPORT_BYTES(rtPath + "runtime/gc/store-buffer/generics", KIND_HEAP,
                rtStats.runtime.gc.storeBufferGenerics, "Generic things in the store buffer.");

  RREPORT_BYTES(rtPath + "runtime/jit-lazylink", KIND_HEAP, rtStats.runtime.jitLazyLink,
                "IonMonkey compilations waiting for lazy linking.");

  if (rtTotalOut) {
    *rtTotalOut = rtTotal;
  }

  // Report GC numbers that don't belong to a realm.

  // We don't want to report decommitted memory in "explicit", so we just
  // change the leading "explicit/" to "decommitted/".
  std::string rtPath2(rtPath);
  // rtPath2.ReplaceLiteral(0, strlen("explicit"), "decommitted");

  REPORT_GC_BYTES(rtPath2 + "gc-heap/decommitted-arenas", rtStats.gcHeapDecommittedArenas,
                  "GC arenas in non-empty chunks that is decommitted, i.e. it takes up "
                  "address space but no physical memory or swap space.");

  REPORT_GC_BYTES(rtPath + "gc-heap/unused-chunks", rtStats.gcHeapUnusedChunks,
                  "Empty GC chunks which will soon be released unless claimed for new "
                  "allocations.");

  REPORT_GC_BYTES(rtPath + "gc-heap/unused-arenas", rtStats.gcHeapUnusedArenas,
                  "Empty GC arenas within non-empty chunks.");

  REPORT_GC_BYTES(rtPath + "gc-heap/chunk-admin", rtStats.gcHeapChunkAdmin,
                  "Bookkeeping information within GC chunks.");

  // gcTotal is the sum of everything we've reported for the GC heap.  It
  // should equal rtStats.gcHeapChunkTotal.
  printf("gcTotal: %zu, gcHeapChunkTotal: %zu (should be the same)\n", gcTotal,
         rtStats.gcHeapChunkTotal);
  // MOZ_ASSERT(gcTotal == rtStats.gcHeapChunkTotal);
}

class SimpleJSRuntimeStats : public JS::RuntimeStats {
public:
  explicit SimpleJSRuntimeStats(mozilla::MallocSizeOf mallocSizeOf)
      : JS::RuntimeStats(mallocSizeOf) {}

  virtual void initExtraZoneStats(JS::Zone *zone, JS::ZoneStats *zStats,
                                  const JS::AutoRequireNoGC &nogc) override {}

  virtual void initExtraRealmStats(JS::Realm *realm, JS::RealmStats *realmStats,
                                   const JS::AutoRequireNoGC &nogc) override {}
};
