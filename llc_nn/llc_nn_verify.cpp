#include "llc_nn.h"
#include "llc_log.h"
#include "llc_png.h"
#include "llc_file.h"
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
 for(const llc::u2_t units : {1U, 10U, 32U}) {
  llc::SNN run;
  llc::SNNConfig config;
  config.HiddenUnits = units;
  if_fail_fe(llc::nnBegin(run, 4, config));
  if_true_fe(run.Steps || run.LayerUnits[1] != units || run.Count != 30);
  if_fail_fe(llc::nnStep(run, 16));
  if_true_fe(!std::isfinite(run.SampleLoss));
  const auto weights = run.Weights;
  if_fail_fe(llc::nnBegin(run, 4, run.Config));
  if_true_fe(run.Steps || run.BeforeSize.x || run.Inspected || run.Config.HiddenUnits != units);
  if_fail_fe(llc::nnStep(run, 16));
  for(llc::u2_t i = 0; i < run.WeightSize.x * run.WeightSize.y; ++i) if_true_fe(run.Weights[i] != weights[i]);
  config.HiddenUnits = 0;
  const auto state = run.State;
  if_true_fe(llc::nnBegin(run, 4, config) >= 0 || run.State != state || run.Steps != 16);
  config.HiddenUnits = 33;
  if_true_fe(llc::nnBegin(run, 4, config) >= 0 || run.State != state);
 }
 const llc::n2u2_t maps[] = {{1, 1}, {3, 7}, {25, 25}};
 for(const auto size : maps) {
  llc::SNN run;
  llc::SNNConfig config;
  config.MapSize = size;
  if_fail_fe(llc::nnBegin(run, 7, config));
  if_true_fe(run.Steps || run.Width != size.x || run.Count != size.x * size.y);
  if_fail_fe(llc::nnStep(run, 128));
  const auto values = run.Values;
  if_fail_fe(llc::nnBegin(run, 7, run.Config));
  if_true_fe(run.Steps || run.BeforeSize.x || run.Config.MapSize != size);
  for(llc::u2_t i = 0; i < 128; ++i) if_fail_fe(llc::nnStep(run));
  for(llc::u2_t i = 0; i < run.Count; ++i) if_true_fe(!std::isfinite(run.Values[i]) || run.Values[i] != values[i]);
  const auto state = run.State;
  config.MapSize = {26, 1};
  if_true_fe(llc::nnBegin(run, 7, config) >= 0 || run.State != state);
  config.MapSize = {1, 0};
  if_true_fe(llc::nnBegin(run, 7, config) >= 0 || run.State != state);
 }
 fprintf(report, "Topology: PASS BPN hidden 1/10/32, SOM 1x1/3x7/25x25; reset/replay, serial/batch equivalence, invalid sizes preserve live session\n");
 {
  llc::SNN painter;
  if_fail_fe(llc::nnBegin(painter, llc::NN_IMAGE));
  llc::img8bgra source;
  if_fail_fe(source.resize(llc::n2u2_t{8, 4}));
  for(llc::u2_t y = 0; y < 4; ++y) for(llc::u2_t x = 0; x < 8; ++x)
   source[y][x] = {128, (llc::u0_t)(32 + 60 * y), (llc::u0_t)(24 + 28 * x), 255};
  if_fail_fe(llc::nnImageSource(painter, source.View));
  if_true_fe(painter.Steps || painter.ImageSamples || painter.LayerUnits[0] != 2 || painter.LayerUnits[2] != 3);
  if_fail_fe(llc::nnImagePreview(painter));
  const auto initial = painter.ImageMSE;
  if_true_fe(painter.ImagePass != 1 || painter.ImageSamples || painter.Steps);
  if_fail_fe(llc::nnStep(painter, 4096));
  const auto samples = painter.ImageSamples;
  if_fail_fe(llc::nnImageInspect(painter, 0));
  if_fail_fe(llc::nnImagePreview(painter));
  if_true_fe(painter.ImageSamples != samples || painter.Steps != 4096);
  if_true_fe(!std::isfinite(painter.ImageMSE) || painter.ImageMSE >= initial * 0.75);
  const auto learned = painter.ImageMSE;
  llc::img8bgra prediction;
  if_fail_fe(prediction.resize(source.metrics()));
  for(llc::u2_t i = 0; i < prediction.size(); ++i) prediction.Texels[i] = painter.ImagePrediction.Texels[i];
  if_fail_fe(llc::nnImageInspect(painter, 1));
  if_fail_fe(llc::nnImagePreview(painter));
  if_true_fe(painter.ImageMSE != learned || painter.ImageSamples != samples);
  for(llc::u2_t i = 0; i < prediction.size(); ++i) if_true_fe(prediction.Texels[i] != painter.ImagePrediction.Texels[i]);
  if_true_fe(llc::nnImageInspect(painter, painter.Config.HiddenUnits) >= 0);
  if_fail_fe(llc::nnImageSource(painter, source.View));
  if_fail_fe(llc::nnImagePreview(painter));
  for(llc::u2_t i = 0; i < 4096; ++i) if_fail_fe(llc::nnStep(painter));
  if_fail_fe(llc::nnImageInspect(painter, 0));
  if_fail_fe(llc::nnImagePreview(painter));
  if_true_fe(painter.ImageMSE != learned || painter.ImageSamples != samples);
  llc::au0_t png;
  if_fail_fe(llc::pngFileWrite(source.View, png));
  if_fail_fe(llc::fileFromMemory("NN_IMAGE_SOURCE.png", png));
  if_fail_fe(png.clear());
  if_fail_fe(llc::pngFileWrite(painter.ImagePrediction.View, png));
  if_fail_fe(llc::fileFromMemory("NN_IMAGE_LEARNED.png", png));
  fprintf(report, "Image painter: PASS 2->hidden->3, source/reset, fixed-weight preview, neuron selection preserves prediction, serial/batch replay; MSE %.8f -> %.8f after %llu samples\n", initial, learned, (unsigned long long)samples);
 }
 if_true_fe(fflush(report) || ferror(report));
 rtrn 0;
}
