/*****************************************************************************
 *
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2013 Artem Pavlenko
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *****************************************************************************/

#ifndef MAPNIK_ICU_SHAPER_HPP
#define MAPNIK_ICU_SHAPER_HPP

// mapnik
#include <mapnik/text/text_properties.hpp>
#include <mapnik/text/text_line.hpp>
#include <mapnik/text/face.hpp>
#include <mapnik/debug.hpp>

// stl
#include <list>

// icu
#include <unicode/unistr.h>
#include <unicode/ushape.h>
#include <unicode/schriter.h>


namespace mapnik
{

struct icu_shaper
{
static void shape_text(text_line & line,
                       text_itemizer & itemizer,
                       std::map<unsigned,double> & width_map,
                       face_manager_freetype & font_manager,
                       double scale_factor )
{
    unsigned start = line.first_char();
    unsigned end = line.last_char();
    mapnik::value_unicode_string const& text = itemizer.text();
    size_t length = end - start;
    if (!length) return;
    line.reserve(length);
    std::list<text_item> const& list = itemizer.itemize(start, end);

    UErrorCode err = U_ZERO_ERROR;
    mapnik::value_unicode_string shaped;
    mapnik::value_unicode_string reordered;

    for (auto const& text_item : list)
    {
        face_set_ptr face_set = font_manager.get_face_set(text_item.format->face_name, text_item.format->fontset);
        double size = text_item.format->text_size * scale_factor;
        face_set->set_character_sizes(size);
        for (auto const& face : *face_set)
        {
            UBiDi *bidi = ubidi_openSized(length, 0, &err);
            ubidi_setPara(bidi, text.getBuffer(), length, UBIDI_DEFAULT_LTR, 0, &err);
            ubidi_writeReordered(bidi, reordered.getBuffer(length),
                                 length, UBIDI_DO_MIRRORING, &err);
            ubidi_close(bidi);
            reordered.releaseBuffer(length);

            int32_t num_char = u_shapeArabic(reordered.getBuffer(), length,
                                             shaped.getBuffer(length), length,
                                             U_SHAPE_LETTERS_SHAPE | U_SHAPE_LENGTH_FIXED_SPACES_NEAR |
                                             U_SHAPE_TEXT_DIRECTION_VISUAL_LTR, &err);
            if (num_char < 0)
            {
                MAPNIK_LOG_ERROR(icu_shaper) << " u_shapeArabic returned negative num_char " << num_char;
            }
            std::size_t num_chars = static_cast<std::size_t>(num_char);
            shaped.releaseBuffer(length);
            bool shaped_status = true;
            if (U_SUCCESS(err) && (num_chars == length))
            {
                U_NAMESPACE_QUALIFIER StringCharacterIterator iter(shaped);
                unsigned i = 0;
                for (iter.setToStart(); iter.hasNext();)
                {
                    UChar ch = iter.nextPostInc();
                    glyph_info tmp;
                    tmp.offset.clear();
                    tmp.char_index = i;
                    tmp.glyph_index = FT_Get_Char_Index(face->get_face(), ch);
                    if (tmp.glyph_index == 0)
                    {
                        shaped_status = false;
                        break;
                    }
                    tmp.face = face;
                    tmp.format = text_item.format;
                    face->glyph_dimensions(tmp);
                    width_map[i] += tmp.width;
                    line.add_glyph(tmp, scale_factor);
                    ++i;
                }
            }
            if (!shaped_status) continue;
            line.update_max_char_height(face->get_char_height());
            return;
        }
    }
}

};
} //namespace mapnik

#endif // MAPNIK_ICU_SHAPER_HPP
