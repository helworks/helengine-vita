#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE
#include "float2.hpp"
#include "float3.hpp"
#include "float4.hpp"

// Generated Bepu support for some multi-scene builds still refers to the
// narrow System.Numerics vectors even when the portable numeric surface only
// emitted helengine's float vector types.
using Vector2 = ::float2;
using Vector3 = ::float3;
using Vector4 = ::float4;
#endif
