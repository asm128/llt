#pragma once
#include "llc_grid_color.h"

::llc::err_t nnHeatmap(::llc::g8bgra target, ::llc::view<const ::llc::f3_t> values, ::llc::n2u2_t cells, bool autoScale, ::llc::u2_t cellPixels);
