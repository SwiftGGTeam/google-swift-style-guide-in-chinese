//===--- GlobalObjects.cpp - Statically-initialized objects ---------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
//  Objects that are allocated at global scope instead of on the heap,
//  and statically initialized to avoid synchronization costs, are
//  defined here.
//
//===----------------------------------------------------------------------===//

#include "../SwiftShims/GlobalObjects.h"
#include "swift/Runtime/Metadata.h"
#include "swift/Runtime/Debug.h"
#include <stdlib.h>
#include <random>

namespace swift {
// FIXME(ABI)#76 : does this declaration need SWIFT_RUNTIME_STDLIB_INTERFACE?
// _direct type metadata for Swift._EmptyArrayStorage
SWIFT_RUNTIME_STDLIB_INTERFACE
ClassMetadata CLASS_METADATA_SYM(s18_EmptyArrayStorage);

// _direct type metadata for Swift._RawNativeDictionaryStorage
SWIFT_RUNTIME_STDLIB_INTERFACE
ClassMetadata CLASS_METADATA_SYM(s27_RawNativeDictionaryStorage);

// _direct type metadata for Swift._RawNativeSetStorage
SWIFT_RUNTIME_STDLIB_INTERFACE
ClassMetadata CLASS_METADATA_SYM(s20_RawNativeSetStorage);
} // namespace swift

SWIFT_RUNTIME_STDLIB_INTERFACE
swift::_SwiftEmptyArrayStorage swift::_swiftEmptyArrayStorage = {
  // HeapObject header;
  {
    &swift::CLASS_METADATA_SYM(s18_EmptyArrayStorage), // isa pointer
  },
  
  // _SwiftArrayBodyStorage body;
  {
    0, // int count;                                    
    1  // unsigned int _capacityAndFlags; 1 means elementTypeIsBridgedVerbatim
  }
};

SWIFT_RUNTIME_STDLIB_INTERFACE
swift::_SwiftEmptyDictionaryStorage swift::_swiftEmptyDictionaryStorage = {
  // HeapObject header;
  {
    &swift::CLASS_METADATA_SYM(s27_RawNativeDictionaryStorage), // isa pointer
  },
  
  // _SwiftDictionaryBodyStorage body;
  {
    // We set the capacity to 1 so that there's an empty hole to search.
    // Any insertion will lead to a real storage being allocated, because 
    // Dictionary guarantees there's always another empty hole after insertion.
    1, // int capacity;                               
    0, // int count;
    
    // _SwiftUnsafeBitMap initializedEntries
    {
      &swift::_swiftEmptyDictionaryStorage.entries, // unsigned int* values;
      1 // int bitCount; (1 so there's something for iterators to read)
    },
    
    (void*)1, // void* keys; (non-null garbage)
    (void*)1  // void* values; (non-null garbage)
  },

  0 // int entries; (zero'd bits)
};

SWIFT_RUNTIME_STDLIB_INTERFACE
swift::_SwiftEmptySetStorage swift::_swiftEmptySetStorage = {
  // HeapObject header;
  {
    &swift::CLASS_METADATA_SYM(s20_RawNativeSetStorage), // isa pointer
  },
  
  // _SwiftDictionaryBodyStorage body;
  {
    // We set the capacity to 1 so that there's an empty hole to search.
    // Any insertion will lead to a real storage being allocated, because 
    // Dictionary guarantees there's always another empty hole after insertion.
    1, // int capacity;                                    
    0, // int count;
    
    // _SwiftUnsafeBitMap initializedEntries
    {
      &swift::_swiftEmptySetStorage.entries, // unsigned int* values;
      1 // int bitCount; (1 so there's something for iterators to read)
    },
    
    (void*)1 // void* keys; (non-null garbage)
  },

  0 // int entries; (zero'd bits)
};

#if defined(__Fuchsia__)
#include <zircon/syscalls.h>
static __swift_uint64_t randomUInt64() {
  __swift_uint64_t returnval;
  size_t remaining = sizeof(__swift_uint64_t);
  unsigned char* offset = reinterpret_cast<unsigned char*>(&returnval);
  size_t actual;
  size_t read_len;
  do {
    // We can only read a limited number of bytes via the syscall at a time.
    read_len =
        remaining > ZX_CPRNG_DRAW_MAX_LEN ? ZX_CPRNG_DRAW_MAX_LEN : remaining;

    zx_status_t status = zx_cprng_draw(offset, read_len, &actual);

    // Decrement the remainder and update the pointer offset in the buffer.
    remaining -= actual;
    offset += actual;

  } while (remaining > 0);
  return returnval;
}
#endif

static swift::_SwiftHashingParameters initializeHashingParameters() {
  // Setting the environment variable SWIFT_DETERMINISTIC_HASHING to "1"
  // disables randomized hash seeding. This is useful in cases we need to ensure
  // results are repeatable, e.g., in certain test environments.  (Note that
  // even if the seed override is enabled, hash values aren't guaranteed to
  // remain stable across even minor stdlib releases.)
  auto determinism = getenv("SWIFT_DETERMINISTIC_HASHING");
  if (determinism && 0 == strcmp(determinism, "1")) {
    return { 0, 0, true };
  }
#if defined(__APPLE__)
  // Use arc4random if available.
  __swift_uint64_t seed0 = 0, seed1 = 0;
  arc4random_buf(&seed0, sizeof(seed0));
  arc4random_buf(&seed1, sizeof(seed1));
  return { seed0, seed1, false };
#elif defined(__Fuchsia__)
  return { randomUInt64(), randomUInt64(), false };
#else
  std::random_device randomDevice;
  std::mt19937_64 engine(randomDevice());
  std::uniform_int_distribution<__swift_uint64_t> distribution;
  return { distribution(engine), distribution(engine), false };
#endif
}

SWIFT_ALLOWED_RUNTIME_GLOBAL_CTOR_BEGIN
swift::_SwiftHashingParameters swift::_swift_stdlib_Hashing_parameters =
  initializeHashingParameters();
SWIFT_ALLOWED_RUNTIME_GLOBAL_CTOR_END


SWIFT_RUNTIME_STDLIB_INTERFACE
void swift::_swift_instantiateInertHeapObject(void *address,
                                              const HeapMetadata *metadata) {
  ::new (address) HeapObject{metadata};
}

namespace llvm { namespace hashing { namespace detail {
  // An extern variable expected by LLVM's hashing templates. We don't link any
  // LLVM libs into the runtime, so define it as a weak symbol.
  //
  // Systems that compile this code into a dynamic library will do so with
  // hidden visibility, making this all internal to the dynamic library.
  // Systems that statically link the Swift runtime into applications (e.g. on
  // Linux) need this to handle the case when the app already uses LLVM.
  size_t LLVM_ATTRIBUTE_WEAK fixed_seed_override = 0;
} // namespace detail
} // namespace hashing
} // namespace llvm
