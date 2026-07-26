# PS Vita Text Effects Design

## Goal

Make every PS Vita UI text drawable render the same authored effects as the Windows command-list path: one optional shadow, four optional cardinal outline copies, and the foreground glyph.

## Root Cause

`PsVitaRenderManager2D::DrawText` currently reads only the text color and queues one glyph quad. It ignores the generated text properties for shadow offset/color and outline scale/color.

## Design

The PS Vita text renderer will build the same ordered passes as `TextRenderEffectPassBuilder`:

1. Add the shadow glyph when either shadow-offset coordinate is non-zero.
2. Add left, right, top, and bottom outline glyphs when outline scale is positive.
3. Add the foreground glyph.

Each pass uses the existing glyph texture coordinates and dimensions. It offsets the glyph position and uses the pass color, while retaining the original composed 2D render order. Queuing passes in this order keeps the foreground glyph above its effects and preserves ordering against other UI drawables.

## Scope and Validation

Only `PsVitaRenderManager2D::DrawText` and a focused source-audit test will change. The test will require all text-effect properties, the shadow pass, the four cardinal outline offsets, and the foreground pass. A Vita build and Vita3K launch will verify the rendered result.
