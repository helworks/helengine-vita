#include "platform/psvita/rendering/PsVitaDrawOrder.hpp"
#include "platform/psvita/rendering/PsVitaQueuedQuad.hpp"
#include "platform/psvita/rendering/PsVitaSolidColorVertex.hpp"
#include <cassert>
#include <cstdio>
#include <type_traits>
#include <vector>

using namespace helengine::psvita::rendering;

/// Verifies that actual request ordinals and command fields retain ordered interleaving across large text menus and cameras.
int main() {
    static_assert(std::is_same_v<decltype(PsVitaQueuedQuad::RenderOrder), std::uint64_t>);
    static_assert(std::is_same_v<decltype(PsVitaSolidColorVertex::RenderOrder), std::uint64_t>);
    PsVitaDrawRequestOrder order;
    const auto firstShape = ComposePsVitaDrawOrder(0, order.Next());
    const auto firstText = ComposePsVitaDrawOrder(0, order.Next());
    const auto secondShape = ComposePsVitaDrawOrder(0, order.Next());
    assert(firstShape < firstText && firstText < secondShape);
    PsVitaQueuedQuad glyphs[3] {};
    for (auto& glyph : glyphs) glyph.RenderOrder = firstText;
    assert(glyphs[0].RenderOrder == glyphs[1].RenderOrder && glyphs[1].RenderOrder == glyphs[2].RenderOrder);
    std::uint64_t previous = secondShape;
    for (int index = 0; index < 1024; ++index) {
        const auto key = ComposePsVitaDrawOrder(0, order.Next());
        assert(key > previous);
        previous = key;
    }
    assert(previous > 512 && ComposePsVitaDrawOrder(1, 1) > previous);
    assert(ComposePsVitaDrawOrder(255, 1) > ComposePsVitaDrawOrder(254, 0xffffffffu));
    assert(ComposePsVitaDrawOrder(0, 0xffffffffu) < ComposePsVitaDrawOrder(1, 0));
    order.Reset();
    assert(order.Next() == 1);
    std::puts("PS Vita hierarchy request/camera order preserves interleaving, full ordinals and frame reset.");
}
