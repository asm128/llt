#pragma once
#include "llc_nn.h"
#include "llc_log.h"
#include "llc_image.h"
#include "llc_array_obj.h"
#include "llc_ptr_obj.h"
#include <cstdio>

namespace llc {
 template<typename TCell> void nnWeights(SNN & run, const img<TCell> & weights) {
  ++run.Inspection.Revision;
  run.Inspection.WeightSize = {min(weights.metrics().x, 32U), min(weights.metrics().y, 32U)};
  for(u2_t y = 0; y < run.Inspection.WeightSize.y; ++y)
   for(u2_t x = 0; x < run.Inspection.WeightSize.x; ++x)
    run.Inspection.Weights[y * run.Inspection.WeightSize.x + x] = weights[y][x];
 }
 template<typename T, typename... TArgs> err_t nnCreate(SNN & run, const TArgs &... args) {
  gref<T> * state = {};
  if_null_fe(ref_create(&state, args...));
  run.Model.State = state;
  run.Model.Destroy = [](void * p) -> err_t { gref<T> * reference = (gref<T>*)p; rtrn ref_release(&reference); };
  run.Model.Advance = [](void * p, SNN & output) -> err_t {
   T & data = *((gref<T>*)p)->Instance;
   cnst err_t result = data.Step(output);
   data.Snapshot(output);
   if_true_fe(data.f && ferror(data.f));
   if(result == 1 && data.f) {
    FILE * report = data.f; data.f = nullptr;
    if_true_fe(fclose(report));
   }
   return result;
  };
  cnst err_t result = state->Instance->Begin();
  if(result < 0) { nnReset(run); run.Error = result; }
  else state->Instance->Snapshot(run);
  return result;
 }
 err_t nnCreateAdaline(SNN &);
 err_t nnCreateArt1(SNN &);
 err_t nnCreateBam(SNN &);
 err_t nnCreateBoltzman(SNN &);
 err_t nnCreateBpn(SNN &);
 err_t nnCreateCpn(SNN &);
 err_t nnCreateHopfield(SNN &);
 err_t nnCreateSom(SNN &);
 err_t nnCreateImage(SNN &);
}
