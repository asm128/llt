#pragma once
#include "llc_grid_color.h"

namespace llc
{
	tplt<tpnm _tValues>
	sttc	llc::err_t	nnHeatmap(g8bgra target, view<const _tValues> values, n2u2_t cells, bool autoScale, u2_t cellPixels) {
		 if_fail_fe(target.fill(bgra{22, 18, 16}));
		 if_zero_vwf(1, cells.x && cells.y && values.size(), "cells.x:%u, cells.y:%u, values.size():%u", cells.x, cells.y, values.size());
		 f3_t scale = 1;
		 if(autoScale) { scale = 0.000001; for(auto v : values) if(std::isfinite(v)) scale = max(scale, std::abs(v)); }
		 const auto size = target.metrics();
		 if(size.x < 48 || size.y < 100) rtrn 0;
		 f3_c cell = cellPixels ? f3_t(cellPixels) : min(f3_t(size.x - 24) / cells.x, f3_t(size.y - 96) / cells.y);
		 // Keep thin strips visible while subsampling their long axis to fit.
		 f3_c cellX = !cellPixels && cells.x <= size.x - 24 ? max(1.0, cell) : cell;
		 f3_c cellY = !cellPixels && cells.y <= size.y - 96 ? max(1.0, cell) : cell;
		 f3_c originX = (size.x - cellX * cells.x) / 2, originY = 64 + (size.y - 64 - cellY * cells.y) / 2;
		 for(u2_t py = 64; py < size.y; ++py)
		 for(u2_t px = 0; px < size.x; ++px) {
			 f3_c gx = (px + 0.5 - originX) / cellX, gy = (py + 0.5 - originY) / cellY;
			 if(gx < 0 || gy < 0 || gx >= cells.x || gy >= cells.y)
				 continue;
			 const auto x = (u2_t)gx, y = (u2_t)gy;
			 if(cell >= 4 && ((gx - x) * cell < 1 || (gy - y) * cell < 1))
				 continue;
			 const auto index = y * cells.x + x;
			 if(index >= values.size())
				 continue;
			 const auto value = std::isfinite(values[index]) ? values[index] : 0;
			 const auto magnitude = min(1.0, std::abs(value) / scale);
			 const bgra tone = value >= 0 ? bgra{244, 202, 70} : bgra{38, 138, 244};
			 const bgra color = tone * (0.12 + 0.88 * magnitude);
			 target[py][px] = color;
		 }
		 rtrn 0;
	}
} // namespace