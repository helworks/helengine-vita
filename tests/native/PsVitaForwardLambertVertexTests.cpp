#include <cstddef>
#include <type_traits>

#include "helcpp_config.hpp"

// Select test-only numeric policies without modifying the retained generated configuration.
#ifdef VITA_TEST_STORAGE_BITS
#undef HE_CPP_REAL_STORAGE_BITS
#define HE_CPP_REAL_STORAGE_BITS VITA_TEST_STORAGE_BITS
#endif
#ifdef VITA_TEST_ARITHMETIC_BITS
#undef HE_CPP_REAL_ARITHMETIC_BITS
#define HE_CPP_REAL_ARITHMETIC_BITS VITA_TEST_ARITHMETIC_BITS
#endif

#include "Real.hpp"
#include "platform/psvita/rendering/PsVitaForwardLambertVertex.hpp"

using Vertex = helengine::psvita::rendering::PsVitaForwardLambertVertex;

static_assert(sizeof(float) == 4u);
#ifdef VITA_TEST_STORAGE_BITS
static_assert(sizeof(Real) == VITA_TEST_STORAGE_BITS / 8u);
#endif
static_assert(std::is_standard_layout_v<Vertex>);
static_assert(std::is_same_v<decltype(Vertex::Position), float[3]>);
static_assert(std::is_same_v<decltype(Vertex::Normal), float[3]>);
static_assert(std::is_same_v<decltype(Vertex::TexCoord), float[2]>);
static_assert(sizeof(Vertex) == 32u, "GXM F32 forward streams contain exactly eight floats.");
static_assert(offsetof(Vertex, Position) == 0u);
static_assert(offsetof(Vertex, Normal) == 12u);
static_assert(offsetof(Vertex, TexCoord) == 24u);
static_assert(Vertex::PositionOffset == 0u);
static_assert(Vertex::NormalOffset == 12u);
static_assert(Vertex::TexCoordOffset == 24u);
static_assert(Vertex::StreamStride == 32u);

/// Validates the fixed GPU stream layout independently of configured CPU scalar storage.
int main() {
    return 0;
}
