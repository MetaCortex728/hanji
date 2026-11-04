// SPDX-License-Identifier: Apache-2.0
// Compiles the engine-independent core into this module. The core is built and tested on its own with
// strict warnings (see core/ and the CI); here Unreal's warning settings are suspended for it.
#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START
#include "clip.cpp"
THIRD_PARTY_INCLUDES_END
