#include "llc_nn_view.h"
#include "llc_log.h"
#include "llc_minmax.h"
#include <cmath>

::llc::err_t nnHeatmap(::llc::g8bgra target, ::llc::view<const ::llc::f3_t> values, ::llc::n2u2_t cells, bool autoScale, ::llc::u2_t cellPixels) {
 if_fail_fe(target.fill(::llc::bgra{22, 18, 16}));
 if(!cells.x || !cells.y || !values.size()) rtrn 0;
 double scale = 1;
 if(autoScale) { scale = 0.000001; for(auto v : values) if(std::isfinite(v)) scale = ::llc::max(scale, std::abs(v)); }
 const auto size = target.metrics();
 if(size.x < 48 || size.y < 100) rtrn 0;
 const double cell = cellPixels ? double(cellPixels) : ::llc::min(double(size.x - 24) / cells.x, double(size.y - 96) / cells.y);
 // Keep thin strips visible while subsampling their long axis to fit.
 const double cellX = !cellPixels && cells.x <= size.x - 24 ? ::llc::max(1.0, cell) : cell;
 const double cellY = !cellPixels && cells.y <= size.y - 96 ? ::llc::max(1.0, cell) : cell;
 const double originX = (size.x - cellX * cells.x) / 2, originY = 64 + (size.y - 64 - cellY * cells.y) / 2;
 for(::llc::u2_t py = 64; py < size.y; ++py)
 for(::llc::u2_t px = 0; px < size.x; ++px) {
  const double gx = (px + 0.5 - originX) / cellX, gy = (py + 0.5 - originY) / cellY;
  if(gx < 0 || gy < 0 || gx >= cells.x || gy >= cells.y) continue;
  const auto x = (::llc::u2_t)gx, y = (::llc::u2_t)gy;
  if(cell >= 4 && ((gx - x) * cell < 1 || (gy - y) * cell < 1)) continue;
  const auto index = y * cells.x + x;
  if(index >= values.size()) continue;
  const auto value = std::isfinite(values[index]) ? values[index] : 0;
  const auto magnitude = ::llc::min(1.0, std::abs(value) / scale);
  const ::llc::bgra tone = value >= 0 ? ::llc::bgra{244, 202, 70} : ::llc::bgra{38, 138, 244};
  const ::llc::bgra color = tone * (0.12 + 0.88 * magnitude);
  target[py][px] = color;
 }
 rtrn 0;
}
