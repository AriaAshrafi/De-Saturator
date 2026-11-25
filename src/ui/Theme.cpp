#include "Theme.h"

#include "DeSaturatorAssets.h"

namespace desat::ui
{
namespace
{
// The parent brand's colours (design/build/ui/core.js, COLOR).
const juce::Colour paper { 0xffecebe6 }, paperLine { 0xffd6d4cb }, ink { 0xff0c0c0b }, inkLine { 0xff2a2a27 };
const juce::Colour graphite { 0xff8a8983 }, graphiteDeep { 0xff5f5e59 }, klein { 0xff2e40ff }, kleinLight { 0xff7c88ff };

juce::Font font (const char* data, int size, float px)
{
    // Point height is the em size, which is what the design's "px" means for type.
    const auto typeface = juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    return juce::Font (juce::FontOptions().withTypeface (typeface).withPointHeight (px));
}

/** Reads the one path of a design SVG. The files are outlines with a viewBox and a fill, nothing else,
    so they are drawn as paths: placement then follows the spec exactly (origin, baseline, box). */
Mark mark (const char* data, int size)
{
    Mark m;
    const auto xml = juce::parseXML (juce::String::fromUTF8 (data, size));
    if (xml == nullptr)
    {
        jassertfalse;
        return m;
    }

    juce::StringArray box;
    box.addTokens (xml->getStringAttribute ("viewBox"), " ,", "");
    box.removeEmptyStrings();
    if (box.size() == 4)
        m.viewBox = { box[0].getFloatValue(), box[1].getFloatValue(), box[2].getFloatValue(), box[3].getFloatValue() };

    if (const auto* path = xml->getChildByName ("path"))
    {
        m.path = juce::Drawable::parseSVGPath (path->getStringAttribute ("d"));
        m.fill = juce::Colour::fromString ("ff" + path->getStringAttribute ("fill", "#000000").trimCharactersAtStart ("#"));
    }

    jassert (! m.path.isEmpty());
    return m;
}
} // namespace

Theme::Theme()
{
    namespace assets = DeSaturatorAssets;

    lightPalette = { paper, ink, graphiteDeep, paperLine, ink, ink, ink, ink, klein, ink, paper, paper, ink, paper, 0.16f };
    darkPalette = { ink, paper, graphite, inkLine, paper, paper, paper, paper, kleinLight, paper, ink, ink, paper, ink, 0.2f };

    type.title = font (assets::AnybodyBold700_ttf, assets::AnybodyBold700_ttfSize, 15.0f);
    type.button = font (assets::AnybodySemiBold600_ttf, assets::AnybodySemiBold600_ttfSize, 13.0f);
    type.label = font (assets::AnybodySemiBold600_ttf, assets::AnybodySemiBold600_ttfSize, 12.0f);
    type.tooltip = font (assets::AnybodyMedium500_ttf, assets::AnybodyMedium500_ttfSize, 12.0f);
    type.value = font (assets::DepartureMonoRegular_otf, assets::DepartureMonoRegular_otfSize, 11.0f);
    type.valueLarge = font (assets::DepartureMonoRegular_otf, assets::DepartureMonoRegular_otfSize, 22.0f);

    wordmarkLight = mark (assets::wordmarkheader_svg, assets::wordmarkheader_svgSize);
    wordmarkDark = mark (assets::wordmarkheaderdark_svg, assets::wordmarkheaderdark_svgSize);
    creditLight = mark (assets::credit_svg, assets::credit_svgSize);
    creditDark = mark (assets::creditdark_svg, assets::creditdark_svgSize);
}
} // namespace desat::ui
