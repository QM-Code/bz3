#pragma once

#include <RmlUi/Core/Element.h>

#include "common/i18n.hpp"

namespace ui::rmlui {

void ApplyTranslations(Rml::Element *root, const karma::i18n::I18n &i18n);

} // namespace ui::rmlui
