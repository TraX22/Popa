// Popa's version, in one place. Used by Popa.rc (the version resource that
// Explorer shows in the file's Properties > Details) and by the updater,
// which reads it back from the resource at run time.
// Plain #defines because the resource compiler only understands those.
#pragma once

#define POPA_VERSION_MAJOR 1
#define POPA_VERSION_MINOR 0
#define POPA_VERSION_PATCH 0

// Turns a macro's value into a string literal: POPA_STR(0) -> "0".
// Two levels are needed so the macro is expanded before being quoted.
#define POPA_STR_INNER(x) #x
#define POPA_STR(x) POPA_STR_INNER(x)

#define POPA_VERSION_STRING \
    POPA_STR(POPA_VERSION_MAJOR) "." POPA_STR(POPA_VERSION_MINOR) "." POPA_STR(POPA_VERSION_PATCH)
