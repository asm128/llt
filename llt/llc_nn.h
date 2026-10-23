#pragma once
#include "llc_array_static.h"
#include "llc_n2.h"

namespace llc {
 stct SNN {
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
 stxp u2_t NN_COUNT = 8;
 vcst_t nnName(u2_t network);
 vcst_t nnDescription(u2_t network);
 vcst_t nnReport(u2_t network);
 // A unit is one fixed-size sample, resonance attempt, neuron update or sampling attempt.
 // Zero budget is a no-op. Return 1 when complete, 0 when pending, negative on failure.
 err_t nnBegin(SNN & run, u2_t network);
 err_t nnStep(SNN & run, u2_t budget = 1);
 err_t nnReset(SNN & run);
}
