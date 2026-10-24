#include "phase4_archive_types.h"
#include "phase4_hid_types.h"
static_assert(alignof(za::boot::hid::NpadFullKeyStateStorage) == 8);
static_assert(alignof(za::boot::archive::TrpfsRange) == 8);
