#include "llc_nn.h"
#include "llc_log.h"
#include <cstdio>
#include <cmath>

// Runs in the GUI host's Debug setup; no separate executable or runtime.
llc::err_t nnVerify() {
 FILE * report = {};
 if_true_fe(fopen_s(&report, "NN_CHECKS.txt", "w"));
 struct SClose { FILE * File; ~SClose() { fclose(File); } } close = {report};
 for(llc::u2_t network = 0; network < llc::NN_COUNT; ++network) {
  llc::SNN serial, batch;
  if_fail_fe(llc::nnBegin(serial, network));
  if_true_fe(llc::nnStep(serial, 0) || serial.Steps);
  for(llc::u2_t i = 0; i < 256 && !serial.Complete; ++i) if_fail_fe(llc::nnStep(serial, 1));
  const auto values = serial.Values;
  const auto weights = serial.Weights;
  const auto before = serial.WeightsBefore;
  const auto changes = serial.WeightChanges;
  const auto activations = serial.Activations;
  const auto errors = serial.ErrorTerms;
  const auto steps = serial.Steps;
  const auto count = serial.Count;
  const auto size = serial.WeightSize;
  const auto metric = serial.Metric;
  const auto complete = serial.Complete;
  if(complete) { if_true_fe(llc::nnStep(serial, 256) != 1 || serial.Steps != steps); }
  if_fail_fe(llc::nnReset(serial));
  if_true_fe(serial.State || serial.Count || serial.Steps || serial.WeightSize.x || serial.Complete);
  if_fail_fe(llc::nnBegin(batch, network));
  if_fail_fe(llc::nnStep(batch, 256));
  if_true_fe(batch.Steps != steps || batch.Count != count || batch.WeightSize != size || batch.Metric != metric || batch.Complete != complete);
  for(llc::u2_t i = 0; i < count; ++i) if_true_fe(!std::isfinite(batch.Values[i]) || batch.Values[i] != values[i]);
  for(llc::u2_t i = 0; i < size.x * size.y; ++i) if_true_fe(!std::isfinite(batch.Weights[i]) || batch.Weights[i] != weights[i]);
  for(llc::u2_t i = 0; i < size.x * size.y; ++i) {
   if_true_fe(batch.WeightsBefore[i] != before[i] || batch.WeightChanges[i] != changes[i]);
   if(batch.BeforeSize == size) if_true_fe(batch.WeightChanges[i] != batch.Weights[i] - batch.WeightsBefore[i]);
  }
  if(network == 4) {
   if_true_fe(!batch.Inspected || !batch.Training);
   if_true_fe(batch.LayerUnits[0] != 30 || batch.LayerUnits[1] != 10 || batch.LayerUnits[2] != 1);
   for(llc::u2_t i = 0; i < 96; ++i) {
    if_true_fe(!std::isfinite(batch.Activations[i]) || batch.Activations[i] != activations[i]);
    if_true_fe(!std::isfinite(batch.ErrorTerms[i]) || batch.ErrorTerms[i] != errors[i]);
   }
   if_true_fe(batch.Prediction != batch.Activations[64]);
   const auto residual = batch.Target - batch.Prediction;
   if_true_fe(std::abs(batch.SampleLoss - 0.5 * residual * residual) > 1e-12);
   // Reach the first evaluation sample without running training to convergence.
   if_fail_fe(llc::nnStep(batch, 1501 - 256));
   if_true_fe(batch.Training || !batch.Inspected);
   for(llc::u2_t i = 0; i < 64; ++i) if_true_fe(batch.ErrorTerms[i] != 0);
   for(llc::u2_t i = 0; i < batch.WeightSize.x * batch.WeightSize.y; ++i) if_true_fe(batch.WeightChanges[i] != 0);
   fprintf(report, "BPN inspectors: PASS activation/error snapshots, prediction/loss, training/evaluation transition, no stale hidden errors\n");
  }
  if_fail_fe(llc::nnReset(batch));
  const auto name = llc::nnName(network);
  fprintf(report, "%.*s: PASS zero budget, reset, deterministic 1-step/batch equivalence, finite snapshot%s; %llu steps\n",
   (int)name.size(), name.begin(), complete ? ", completion idempotence" : "", (unsigned long long)steps);
 }
 if_true_fe(fflush(report) || ferror(report));
 rtrn 0;
}
