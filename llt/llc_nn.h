#pragma once
#include "llc_array_static.h"
#include "llc_n2.h"
#include "llc_img_color.h"
#include "llc_grid_color.h"

namespace llc {
 stct SNNConfig {
  u2_t HiddenUnits = 10; // BPN only; input/output dimensions belong to the dataset.
  n2u2_t MapSize = {25, 25}; // SOM columns/rows.
 };
 stct SNN {
  SNNConfig Config = {};
  img8bgra ImageSource = {}, ImagePrediction = {}, ImageError = {}, ImageInfluence = {};
  u3_t ImageSamples = {}, ImagePass = {};
  u2_t ImageCursor = {}, ImageNeuron = {};
  f3_t ImageMSE = {};
  void * State = {};
  err_t (*Advance)(void *, SNN &) = {};
  void (*Destroy)(void *) = {};
  u2_t Network = {}, Width = {}, Count = {};
  u3_t Steps = {};
  f3_t Metric = {};
  astatic<f3_t, 625> Values = {};
  astatic<f3_t, 1024> Weights = {};
  astatic<f3_t, 1024> WeightsBefore = {}, WeightChanges = {};
  n2u2_t WeightSize = {};
  n2u2_t BeforeSize = {};
  // BPN rows: input, hidden, output; bias excluded, unused cells zero.
  astatic<f3_t, 96> Activations = {}, ErrorTerms = {};
  astatic<u2_t, 3> LayerUnits = {};
  f3_t Target = {}, Prediction = {}, SampleLoss = {};
  bool Inspected = {}, Training = {};
  bool Complete = {};
  err_t Error = {};
  SNN() = default;
  SNN(cnst SNN &) = delete;
  SNN & operator=(cnst SNN &) = delete;
  ~SNN() { if(Destroy) Destroy(State); }
 };
 stxp u2_t NN_IMAGE = 8, NN_COUNT = 9;
 vcst_t nnName(u2_t network);
 vcst_t nnDescription(u2_t network);
 vcst_t nnReport(u2_t network);
 // A unit is one fixed-size sample, resonance attempt, neuron update or sampling attempt.
 // Zero budget is a no-op. Return 1 when complete, 0 when pending, negative on failure.
 // Invalid sizes fail before touching the current session. BPN: 1..32; SOM: 1..25 per axis.
 err_t nnBegin(SNN & run, u2_t network, const SNNConfig & config = {});
 err_t nnStep(SNN & run, u2_t budget = 1);
 err_t nnReset(SNN & run);
 // Image source: 1..64 pixels per axis, opaque RGB. Resets training; copies before reset.
 err_t nnImageSource(SNN & run, gc8bgra source);
 // Zero-based hidden neuron; refresh its ablation view without changing weights.
 err_t nnImageInspect(SNN & run, u2_t neuron);
 // Render at most 64 pixels at fixed weights; no training or Steps increment.
 err_t nnImagePreview(SNN & run);
}
