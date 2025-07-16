#include <msdf-atlas-gen/msdf-atlas-gen.h>
#include "PKStringUtilities.h"
#include "PKAssetWriter.h"
#include "PKFileVersionUtilities.h"

namespace PKAssets::Font
{
    using namespace msdf_atlas;

    int WriteFont(const char* pathSrc, const char* pathDst)
    {
        if (!PKVersionUtilities::IsFileOutOfDate(pathSrc, pathDst))
        {
            return 1;
        }

        auto filename = StringUtilities::ReadFileName(pathSrc);
        printf("Preprocessing font: %s \n", filename.c_str());

        msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();

        if (ft == nullptr)
        {
            return -1;
        }

        msdfgen::FontHandle* font = msdfgen::loadFont(ft, pathSrc);

        if (font == nullptr)
        {
            msdfgen::deinitializeFreetype(ft);
            return -1;
        }

        std::vector<GlyphGeometry> glyphs;
        std::vector<PKFontCharacter> characters;
        std::string charactersString;
        TightAtlasPacker packer;
        GeneratorAttributes attributes;
        const double maxCornerAngle = 3.0;
        const double loadEmSize = 16.0;
        const double invScale = 1.0f / loadEmSize; //packer.getScale();
        int width = 0, height = 0;

        FontGeometry fontGeometry(&glyphs);
        fontGeometry.loadCharset(font, loadEmSize, Charset::ASCII);
        
        // Apply MSDF edge coloring. See edge-coloring.h for other coloring strategies.
        for (auto& glyph : glyphs)
        {
            glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, maxCornerAngle, 0);
        }

        packer.setDimensionsConstraint(DimensionsConstraint::SQUARE);
        packer.setScale(loadEmSize);
        packer.setPixelRange(PK_FONT_MSDF_UNIT);
        packer.setMiterLimit(1.0);
        packer.pack(glyphs.data(), glyphs.size());
        packer.getDimensions(width, height);

        ImmediateAtlasGenerator<float, 4, &mtsdfGenerator, BitmapAtlasStorage<byte, 4>> generator(width, height);
        generator.setAttributes(attributes);
        generator.setThreadCount(4);
        generator.generate(glyphs.data(), glyphs.size());

        characters.reserve(glyphs.size());

        for (auto& glyph : glyphs)
        {
            GlyphBox box = glyph;
            PKFontCharacter character;
            character.advance = box.advance * invScale;
            character.rect[0] = box.bounds.l * invScale;
            character.rect[1] = box.bounds.b * invScale;
            character.rect[2] = (box.bounds.r - box.bounds.l) * invScale;
            character.rect[3] = (box.bounds.t - box.bounds.b) * invScale;
            character.texrect[0] = box.rect.x;
            character.texrect[1] = box.rect.y;
            character.texrect[2] = box.rect.w;
            character.texrect[3] = box.rect.h;
            character.unicode = (uint16_t)glyph.getCodepoint();
            character.isWhiteSpace = glyph.isWhitespace();
            characters.push_back(character);
            charactersString.push_back((char)character.unicode);
        }

        printf("    Characters: %s\n", charactersString.c_str());
        printf("    Width: %i\n", width);
        printf("    Height: %i\n", height);
        printf("    Glyph Count: %i\n", (int)glyphs.size());

        auto buffer = PKAssetBuffer();
        buffer.header->type = PKAssetType::Font;
        WriteName(buffer.header->name, filename.c_str());

        auto pkFont = buffer.Allocate<PKFont>();
        auto pCharacters = buffer.Write(characters.data(), characters.size());
        pkFont->characters.Set(buffer.data(), pCharacters.get());
        pkFont->characterCount= characters.size();

        msdfgen::BitmapConstRef<byte, 4> bitmap = generator.atlasStorage();
        auto pAtlasData = buffer.Write(bitmap.pixels, bitmap.width * bitmap.height * 4);
        pkFont->atlasData.Set(buffer.data(), pAtlasData.get());
        pkFont->atlasResolution[0] = bitmap.width;
        pkFont->atlasResolution[1] = bitmap.height;
        pkFont->atlasDataSize = bitmap.width* bitmap.height * 4;

        msdfgen::destroyFont(font);
        msdfgen::deinitializeFreetype(ft);

        return WriteAsset(pathDst, buffer, false);
    }
}