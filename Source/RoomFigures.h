#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// The rapper and the singer shown in the room view. They are vector drawings (SVG), so they stay sharp at any window size.
namespace RoomFigures
{
    inline const char* rapperSvg = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 120 200">
<defs>
<linearGradient id="skin" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#f6cf96"/><stop offset="1" stop-color="#a86b2c"/></linearGradient>
<linearGradient id="cloth" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#737a88"/><stop offset="0.55" stop-color="#2c3038"/><stop offset="1" stop-color="#15171b"/></linearGradient>
<linearGradient id="cloth2" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#565c68"/><stop offset="1" stop-color="#1a1c21"/></linearGradient>
<linearGradient id="amber" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#ffd27a"/><stop offset="1" stop-color="#c77a0c"/></linearGradient>
<linearGradient id="metal" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#e6e9ee"/><stop offset="1" stop-color="#7d838e"/></linearGradient>
</defs>
<!-- legs: baggy trousers -->
<path d="M44 108 C40 130 38 150 37 176 L52 178 C54 158 57 140 60 124 C63 140 67 158 70 178 L85 176 C83 150 80 130 76 108 Z" fill="url(#cloth2)" stroke="#f5a524" stroke-opacity="0.45" stroke-width="0.8"/>
<path d="M60 124 C59 112 60 108 60 108" stroke="#0b0c0e" stroke-width="1" fill="none"/>
<!-- sneakers -->
<path d="M33 176 C31 181 30 186 32 189 L54 189 C55 185 54 180 52 177 Z" fill="#e9ecf1"/>
<path d="M68 177 C66 180 65 185 66 189 L88 189 C90 186 89 181 87 176 Z" fill="#e9ecf1"/>
<path d="M32 187 L54 187 M66 187 L88 187" stroke="#f5a524" stroke-width="2"/>
<!-- left arm (viewer's left), out to the side, gesturing -->
<path d="M42 62 C30 66 20 76 13 88 L20 94 C27 84 36 78 46 76 Z" fill="url(#cloth)" stroke="#f5a524" stroke-opacity="0.55" stroke-width="0.8"/>
<path d="M13 88 C9 91 7 95 8 98 C11 100 15 98 20 94 Z" fill="url(#skin)"/>
<!-- hoodie body -->
<path d="M40 60 C36 70 37 96 42 112 L78 112 C83 96 84 70 80 60 C74 54 66 52 60 52 C54 52 46 54 40 60 Z" fill="url(#cloth)" stroke="#f5a524" stroke-opacity="0.55" stroke-width="0.8"/>
<path d="M50 58 C54 66 66 66 70 58" stroke="#15171b" stroke-width="2.2" fill="none"/>
<path d="M56 64 L55 78 M64 64 L65 78" stroke="#e9ecf1" stroke-width="1.2" stroke-linecap="round"/>
<path d="M46 96 C52 100 68 100 74 96 L75 104 C68 108 52 108 45 104 Z" fill="#15171b" fill-opacity="0.55"/>
<!-- right arm (viewer's right), bent up, holding the mic at the mouth -->
<path d="M78 62 C88 68 93 80 91 92 L82 92 C82 84 80 78 74 74 Z" fill="url(#cloth)" stroke="#f5a524" stroke-opacity="0.55" stroke-width="0.8"/>
<path d="M91 92 C90 80 84 66 72 56 L67 62 C76 70 81 82 82 92 Z" fill="url(#cloth)" stroke="#f5a524" stroke-opacity="0.55" stroke-width="0.8"/>
<path d="M66 53 C69 51 74 53 75 57 C75 61 71 63 67 62 C64 60 64 56 66 53 Z" fill="url(#skin)"/>
<!-- neck, head -->
<path d="M54 44 L54 54 C58 57 62 57 66 54 L66 44 Z" fill="#a86b2c"/>
<path d="M48 30 C48 20 54 15 60 15 C66 15 72 20 72 30 C72 40 67 48 60 48 C53 48 48 40 48 30 Z" fill="url(#skin)"/>
<path d="M48 31 C46 31 46 36 48.5 37 M72 31 C74 31 74 36 71.5 37" stroke="#a86b2c" stroke-width="2.4" fill="none" stroke-linecap="round"/>
<!-- cap, brim to the side -->
<path d="M46.5 27 C46 15 53 9 60 9 C67 9 74 15 73.5 27 C66 24 54 24 46.5 27 Z" fill="url(#amber)"/>
<path d="M72 24 C80 23 88 25 91 29 C84 29 77 29 72 28 Z" fill="#c77a0c"/>
<path d="M60 9 L60 24" stroke="#c77a0c" stroke-width="0.8"/>
<!-- microphone -->
<path d="M71 60 L66 51" stroke="#15171b" stroke-width="5" stroke-linecap="round"/>
<ellipse cx="64.5" cy="47" rx="5" ry="5.4" fill="url(#metal)"/>
<path d="M60.5 44.6 L68.5 49.6 M60 48 L67 52" stroke="#555b66" stroke-width="0.7"/>
<!-- chain -->
<path d="M53 56 C56 70 64 70 67 56" stroke="#ffd27a" stroke-width="1.4" fill="none"/>
<ellipse cx="60" cy="68" rx="2.6" ry="3" fill="#ffd27a"/>
</svg>
)SVG";

    inline const char* singerSvg = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 120 200">
<defs>
<linearGradient id="skin" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#f6cf96"/><stop offset="1" stop-color="#a86b2c"/></linearGradient>
<linearGradient id="cloth" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#737a88"/><stop offset="0.55" stop-color="#2c3038"/><stop offset="1" stop-color="#15171b"/></linearGradient>
<linearGradient id="cloth2" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#565c68"/><stop offset="1" stop-color="#1a1c21"/></linearGradient>
<linearGradient id="amber" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#ffd27a"/><stop offset="1" stop-color="#c77a0c"/></linearGradient>
<linearGradient id="metal" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#e6e9ee"/><stop offset="1" stop-color="#7d838e"/></linearGradient>
</defs>
<!-- mic stand -->
<path d="M84 189 L84 62" stroke="#aab0ba" stroke-width="2.2"/>
<path d="M70 190 L98 190" stroke="#7d838e" stroke-width="3" stroke-linecap="round"/>
<path d="M84 184 L72 190 M84 184 L96 190" stroke="#7d838e" stroke-width="2"/>
<!-- legs: slim trousers -->
<path d="M47 108 C45 132 45 154 46 180 L56 180 C57 160 58 142 60 124 C62 142 63 160 64 180 L74 180 C75 154 75 132 73 108 Z" fill="url(#cloth2)" stroke="#f5a524" stroke-opacity="0.45" stroke-width="0.8"/>
<!-- shoes with a small heel -->
<path d="M43 180 C41 184 41 188 43 190 L57 190 C58 186 57 182 56 180 Z" fill="#15171b"/>
<path d="M64 180 C63 182 62 186 63 190 L77 190 C79 188 79 184 77 180 Z" fill="#15171b"/>
<!-- hair behind the shoulders -->
<path d="M45 30 C42 44 42 58 46 70 L54 62 L66 62 L74 70 C78 58 78 44 75 30 C72 16 48 16 45 30 Z" fill="#23262c"/>
<!-- left arm (viewer's left), relaxed at the side -->
<path d="M43 62 C37 72 36 92 38 108 L45 108 C45 94 46 80 50 70 Z" fill="url(#cloth)" stroke="#f5a524" stroke-opacity="0.55" stroke-width="0.8"/>
<path d="M38 108 C37 113 38 118 41 119 C44 118 45 113 45 108 Z" fill="url(#skin)"/>
<!-- top / jacket, tapered waist -->
<path d="M43 60 C40 72 43 94 47 110 L73 110 C77 94 80 72 77 60 C72 54 66 53 60 53 C54 53 48 54 43 60 Z" fill="url(#cloth)" stroke="#f5a524" stroke-opacity="0.55" stroke-width="0.8"/>
<path d="M52 56 L60 76 L68 56" fill="url(#skin)"/>
<path d="M52 56 L60 76 L68 56" stroke="#15171b" stroke-width="1.6" fill="none"/>
<path d="M60 76 L60 110" stroke="#15171b" stroke-width="1"/>
<path d="M47 104 L73 104" stroke="#f5a524" stroke-width="2"/>
<!-- right arm (viewer's right), raised to the mic on the stand -->
<path d="M76 62 C84 68 88 76 88 84 L80 86 C80 80 77 76 72 73 Z" fill="url(#cloth)" stroke="#f5a524" stroke-opacity="0.55" stroke-width="0.8"/>
<path d="M88 84 C89 76 87 70 84 66 L78 69 C80 74 81 80 80 86 Z" fill="url(#cloth)" stroke="#f5a524" stroke-opacity="0.55" stroke-width="0.8"/>
<path d="M79 60 C82 58 86 60 87 64 C87 68 84 70 80 69 C77 67 77 63 79 60 Z" fill="url(#skin)"/>
<!-- microphone on the stand, at the mouth -->
<path d="M84 64 L72 50" stroke="#15171b" stroke-width="4.6" stroke-linecap="round"/>
<ellipse cx="69.5" cy="47.5" rx="4.8" ry="5.2" fill="url(#metal)"/>
<path d="M65.6 45.5 L73 50 M65.4 48.6 L71.6 52.2" stroke="#555b66" stroke-width="0.7"/>
<!-- neck, head -->
<path d="M55 44 L55 55 C58 58 62 58 65 55 L65 44 Z" fill="#a86b2c"/>
<path d="M49 30 C49 20 54 14 60 14 C66 14 71 20 71 30 C71 40 66 48 60 48 C54 48 49 40 49 30 Z" fill="url(#skin)"/>
<!-- hair on top, parted -->
<path d="M47 32 C44 18 52 9 60 9 C68 9 76 18 73 32 C71 24 66 19 60 20 C54 19 49 24 47 32 Z" fill="#23262c"/>
<path d="M60 10 C57 14 55 18 54 22" stroke="#3c414b" stroke-width="0.9" fill="none"/>
<!-- earring -->
<ellipse cx="49.5" cy="38" rx="1.4" ry="1.4" fill="#ffd27a"/>
</svg>
)SVG";

    inline std::unique_ptr<juce::Drawable> make (const char* svg)
    {
        if (auto xml = juce::parseXML (svg)) return juce::Drawable::createFromSVG (*xml);
        return {};
    }
}
