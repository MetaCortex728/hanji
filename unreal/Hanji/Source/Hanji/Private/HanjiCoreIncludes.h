// SPDX-License-Identifier: Apache-2.0
// The core's headers, included with Unreal's warning settings suspended.
#pragma once

#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START
#include "hanji/clip.hpp"
#include "hanji/clock.hpp"
#include "hanji/geometry.hpp"
#include "hanji/policy.hpp"
#include "hanji/readers.hpp"
THIRD_PARTY_INCLUDES_END

namespace HanjiConvert
{
	inline hanji::ViewRotation View(const FRotator& R) { return {R.Pitch, R.Yaw, R.Roll}; }
	inline FVector Vector(const hanji::Vec3& V) { return FVector(V.x, V.y, V.z); }
	inline hanji::Vec3 Vec(const FVector& V) { return {V.X, V.Y, V.Z}; }
	inline FString String(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
	inline std::string Std(const FString& S) { return std::string(TCHAR_TO_UTF8(*S)); }
}
