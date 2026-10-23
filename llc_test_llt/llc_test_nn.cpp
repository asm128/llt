#include "llc_nn.h"
#include "llc_nn_view.h"
#include "llc_test_core.h"
#include "llc_log.h"
#include "llc_png.h"
#include "llc_file.h"
#include <cstdio>
#include <cmath>

GDEFINE_ENUM_TYPE(NN_TEST_RESULT, ::llc::u0_t);
GDEFINE_ENUM_VALUED(NN_TEST_RESULT, STATE, 0, "NN reset, ownership, step or replay contract failed.");
GDEFINE_ENUM_VALUED(NN_TEST_RESULT, TOPOLOGY, 1, "NN topology change or validation contract failed.");
GDEFINE_ENUM_VALUED(NN_TEST_RESULT, IMAGE, 2, "Image learning, preview isolation or replay contract failed.");
GDEFINE_ENUM_VALUED(NN_TEST_RESULT, HEATMAP, 3, "Heatmap lost a signed endpoint.");

llc::err_t testNN(llc::ATestError & results, llc::vcst_t outputFolder) {
 for(llc::u2_t network = 0; network < llc::NN_COUNT; ++network) {
  llc::SNN serial, batch;
  if_fail_fe(llc::nnBegin(serial, network));
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_STATE, (serial.Painter.get_ref() != nullptr) != (network == llc::NN_IMAGE));
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_STATE, llc::nnStep(serial, 0) || serial.Steps);
  for(llc::u2_t i = 0; i < 256 && !serial.Complete; ++i) if_fail_fe(llc::nnStep(serial, 1));
  const auto values = serial.Inspection.Values;
  const auto weights = serial.Inspection.Weights;
  const auto before = serial.Inspection.WeightsBefore;
  const auto changes = serial.Inspection.WeightChanges;
  const auto activations = serial.Inspection.Activations;
  const auto errors = serial.Inspection.ErrorTerms;
  const auto steps = serial.Steps;
  const auto count = serial.Inspection.Count;
  const auto size = serial.Inspection.WeightSize;
  const auto metric = serial.Metric;
  const auto complete = serial.Complete;
  if(complete) { LLC_TEST_REQUIRE(results, NN_TEST_RESULT_STATE, llc::nnStep(serial, 256) != 1 || serial.Steps != steps); }
  if_fail_fe(llc::nnReset(serial));
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_STATE, serial.Model.State || serial.Inspection.Count || serial.Steps || serial.Inspection.WeightSize.x || serial.Complete);
  if_fail_fe(llc::nnBegin(batch, network));
  if_fail_fe(llc::nnStep(batch, 256));
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_STATE, batch.Steps != steps || batch.Inspection.Count != count || batch.Inspection.WeightSize != size || batch.Metric != metric || batch.Complete != complete);
  for(llc::u2_t i = 0; i < count; ++i) LLC_TEST_REQUIRE(results, NN_TEST_RESULT_STATE, !std::isfinite(batch.Inspection.Values[i]) || batch.Inspection.Values[i] != values[i]);
  for(llc::u2_t i = 0; i < size.x * size.y; ++i) LLC_TEST_REQUIRE(results, NN_TEST_RESULT_STATE, !std::isfinite(batch.Inspection.Weights[i]) || batch.Inspection.Weights[i] != weights[i]);
  for(llc::u2_t i = 0; i < size.x * size.y; ++i) {
   LLC_TEST_REQUIRE(results, NN_TEST_RESULT_STATE, batch.Inspection.WeightsBefore[i] != before[i] || batch.Inspection.WeightChanges[i] != changes[i]);
   if(batch.Inspection.BeforeSize == size) LLC_TEST_REQUIRE(results, NN_TEST_RESULT_STATE, batch.Inspection.WeightChanges[i] != batch.Inspection.Weights[i] - batch.Inspection.WeightsBefore[i]);
  }
  if(network == llc::NN_BACKPROPAGATION) {
   LLC_TEST_REQUIRE(results, NN_TEST_RESULT_STATE, !batch.Inspection.Inspected || !batch.Inspection.Training);
   LLC_TEST_REQUIRE(results, NN_TEST_RESULT_STATE, batch.Inspection.LayerUnits[0] != 30 || batch.Inspection.LayerUnits[1] != 10 || batch.Inspection.LayerUnits[2] != 1);
   for(llc::u2_t i = 0; i < 96; ++i) {
    LLC_TEST_REQUIRE(results, NN_TEST_RESULT_STATE, !std::isfinite(batch.Inspection.Activations[i]) || batch.Inspection.Activations[i] != activations[i]);
    LLC_TEST_REQUIRE(results, NN_TEST_RESULT_STATE, !std::isfinite(batch.Inspection.ErrorTerms[i]) || batch.Inspection.ErrorTerms[i] != errors[i]);
   }
   LLC_TEST_REQUIRE(results, NN_TEST_RESULT_STATE, batch.Inspection.Prediction != batch.Inspection.Activations[64]);
   const auto residual = batch.Inspection.Target - batch.Inspection.Prediction;
   LLC_TEST_REQUIRE(results, NN_TEST_RESULT_STATE, std::abs(batch.Inspection.SampleLoss - 0.5 * residual * residual) > 1e-12);
   // Reach the first evaluation sample without running training to convergence.
   if_fail_fe(llc::nnStep(batch, 1501 - 256));
   LLC_TEST_REQUIRE(results, NN_TEST_RESULT_STATE, batch.Inspection.Training || !batch.Inspection.Inspected);
   for(llc::u2_t i = 0; i < 64; ++i) LLC_TEST_REQUIRE(results, NN_TEST_RESULT_STATE, batch.Inspection.ErrorTerms[i] != 0);
   for(llc::u2_t i = 0; i < batch.Inspection.WeightSize.x * batch.Inspection.WeightSize.y; ++i) LLC_TEST_REQUIRE(results, NN_TEST_RESULT_STATE, batch.Inspection.WeightChanges[i] != 0);

  }
  if_fail_fe(llc::nnReset(batch));

 }
 for(const llc::u2_t units : {1U, 10U, 32U}) {
  llc::SNN run;
  llc::SNNConfig config;
  config.HiddenUnits = units;
  if_fail_fe(llc::nnBegin(run, llc::NN_BACKPROPAGATION, config));
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_TOPOLOGY, run.Steps || run.Inspection.LayerUnits[1] != units || run.Inspection.Count != 30);
  if_fail_fe(llc::nnStep(run, 16));
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_TOPOLOGY, !std::isfinite(run.Inspection.SampleLoss));
  const auto weights = run.Inspection.Weights;
  if_fail_fe(llc::nnBegin(run, llc::NN_BACKPROPAGATION, run.Config));
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_TOPOLOGY, run.Steps || run.Inspection.BeforeSize.x || run.Inspection.Inspected || run.Config.HiddenUnits != units);
  if_fail_fe(llc::nnStep(run, 16));
  for(llc::u2_t i = 0; i < run.Inspection.WeightSize.x * run.Inspection.WeightSize.y; ++i) LLC_TEST_REQUIRE(results, NN_TEST_RESULT_TOPOLOGY, run.Inspection.Weights[i] != weights[i]);
  config.HiddenUnits = 0;
  const auto state = run.Model.State;
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_TOPOLOGY, llc::nnBegin(run, llc::NN_BACKPROPAGATION, config) >= 0 || run.Model.State != state || run.Steps != 16);
  config.HiddenUnits = 33;
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_TOPOLOGY, llc::nnBegin(run, llc::NN_BACKPROPAGATION, config) >= 0 || run.Model.State != state);
 }
 const llc::n2u2_t maps[] = {{1, 1}, {3, 7}, {25, 25}};
 for(const auto size : maps) {
  llc::SNN run;
  llc::SNNConfig config;
  config.MapSize = size;
  if_fail_fe(llc::nnBegin(run, llc::NN_SOM, config));
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_TOPOLOGY, run.Steps || run.Inspection.Width != size.x || run.Inspection.Count != size.x * size.y);
  if_fail_fe(llc::nnStep(run, 128));
  const auto values = run.Inspection.Values;
  if_fail_fe(llc::nnBegin(run, llc::NN_SOM, run.Config));
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_TOPOLOGY, run.Steps || run.Inspection.BeforeSize.x || run.Config.MapSize != size);
  for(llc::u2_t i = 0; i < 128; ++i) if_fail_fe(llc::nnStep(run));
  for(llc::u2_t i = 0; i < run.Inspection.Count; ++i) LLC_TEST_REQUIRE(results, NN_TEST_RESULT_TOPOLOGY, !std::isfinite(run.Inspection.Values[i]) || run.Inspection.Values[i] != values[i]);
  const auto state = run.Model.State;
  config.MapSize = {26, 1};
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_TOPOLOGY, llc::nnBegin(run, llc::NN_SOM, config) >= 0 || run.Model.State != state);
  config.MapSize = {1, 0};
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_TOPOLOGY, llc::nnBegin(run, llc::NN_SOM, config) >= 0 || run.Model.State != state);
 }

 {
  llc::SNN painter;
  if_fail_fe(llc::nnBegin(painter, llc::NN_IMAGE));
  llc::img8bgra source;
  if_fail_fe(source.resize(llc::n2u2_t{8, 4}));
  for(llc::u2_t y = 0; y < 4; ++y) for(llc::u2_t x = 0; x < 8; ++x)
   source[y][x] = {128, (llc::u0_t)(32 + 60 * y), (llc::u0_t)(24 + 28 * x), 255};
  if_fail_fe(llc::nnImageSource(painter, source.View));
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_IMAGE, painter.Steps || painter.Painter->Samples || painter.Inspection.LayerUnits[0] != 2 || painter.Inspection.LayerUnits[2] != 3);
  if_fail_fe(llc::nnImagePreview(painter));
  const auto initial = painter.Painter->MSE;
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_IMAGE, painter.Painter->Pass != 1 || painter.Painter->Samples || painter.Steps);
  if_fail_fe(llc::nnStep(painter, 4096));
  const auto samples = painter.Painter->Samples;
  const auto inspected = painter.Inspection;
  if_fail_fe(llc::nnImageInspect(painter, 0));
  if_fail_fe(llc::nnImagePreview(painter));
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_IMAGE, painter.Painter->Samples != samples || painter.Steps != 4096);
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_IMAGE, painter.Inspection.Revision != inspected.Revision || painter.Inspection.Training != inspected.Training);
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_IMAGE, painter.Inspection.Target != inspected.Target || painter.Inspection.Prediction != inspected.Prediction || painter.Inspection.SampleLoss != inspected.SampleLoss);
  for(llc::u2_t i = 0; i < inspected.Activations.size(); ++i)
   LLC_TEST_REQUIRE(results, NN_TEST_RESULT_IMAGE, painter.Inspection.Activations[i] != inspected.Activations[i] || painter.Inspection.ErrorTerms[i] != inspected.ErrorTerms[i]);
  for(llc::u2_t i = 0; i < inspected.Weights.size(); ++i)
   LLC_TEST_REQUIRE(results, NN_TEST_RESULT_IMAGE, painter.Inspection.Weights[i] != inspected.Weights[i] || painter.Inspection.WeightsBefore[i] != inspected.WeightsBefore[i] || painter.Inspection.WeightChanges[i] != inspected.WeightChanges[i]);
  if_fail_fe(llc::nnImageInspect(painter, 0));
  if_fail_fe(llc::nnStep(painter));
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_IMAGE, painter.Steps != 4097 || painter.Painter->Samples != samples || painter.Inspection.Revision != inspected.Revision);
  for(llc::u2_t i = 0; i < inspected.Weights.size(); ++i)
   LLC_TEST_REQUIRE(results, NN_TEST_RESULT_IMAGE, painter.Inspection.WeightsBefore[i] != inspected.WeightsBefore[i] || painter.Inspection.WeightChanges[i] != inspected.WeightChanges[i]);
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_IMAGE, !std::isfinite(painter.Painter->MSE) || painter.Painter->MSE >= initial * 0.75);
  const auto learned = painter.Painter->MSE;
  llc::img8bgra prediction;
  if_fail_fe(prediction.resize(source.metrics()));
  for(llc::u2_t i = 0; i < prediction.size(); ++i) prediction.Texels[i] = painter.Painter->Prediction.Texels[i];
  if_fail_fe(llc::nnImageInspect(painter, 1));
  if_fail_fe(llc::nnImagePreview(painter));
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_IMAGE, painter.Painter->MSE != learned || painter.Painter->Samples != samples);
  for(llc::u2_t i = 0; i < prediction.size(); ++i) LLC_TEST_REQUIRE(results, NN_TEST_RESULT_IMAGE, prediction.Texels[i] != painter.Painter->Prediction.Texels[i]);
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_IMAGE, llc::nnImageInspect(painter, painter.Config.HiddenUnits) >= 0);
  if_fail_fe(llc::nnImageSource(painter, source.View));
  if_fail_fe(llc::nnImagePreview(painter));
  for(llc::u2_t i = 0; i < 4096; ++i) if_fail_fe(llc::nnStep(painter));
  if_fail_fe(llc::nnImageInspect(painter, 0));
  if_fail_fe(llc::nnImagePreview(painter));
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_IMAGE, painter.Painter->MSE != learned || painter.Painter->Samples != samples);
  llc::au0_t png;
  if_fail_fe(llc::pngFileWrite(source.View, png));
  llc::string imagePath;
  if_fail_fe(llc::pathNameCompose(outputFolder, llc::vcst_t{"NN_IMAGE_SOURCE.png"}, imagePath));
  if_fail_fe(llc::fileFromMemory(imagePath, png));
  if_fail_fe(png.clear());
  if_fail_fe(llc::pngFileWrite(painter.Painter->Prediction.View, png));
  if_fail_fe(imagePath.clear());
  if_fail_fe(llc::pathNameCompose(outputFolder, llc::vcst_t{"NN_IMAGE_LEARNED.png"}, imagePath));
  if_fail_fe(llc::fileFromMemory(imagePath, png));
  if_fail_fe(llc::nnBegin(painter, llc::NN_ADALINE));
  LLC_TEST_REQUIRE(results, NN_TEST_RESULT_STATE, painter.Painter.get_ref() != nullptr || !painter.Model.State);

 }
 rtrn 0;
}

::llc::err_t testNNHeatmap(::llc::ATestError & results, ::llc::vcst_t) {
 ::llc::img8bgra image;
 ::llc::astatic<::llc::f3_t, 625> values = {};
 for(::llc::u2_t i = 0; i < values.size(); ++i) values[i] = i < 312 ? -1 : 1;
 const ::llc::n2u2_t targets[] = {{1, 1}, {48, 100}, {240, 200}};
 const ::llc::n2u2_t grids[] = {{25, 25}, {625, 1}, {1, 625}, {3, 7}};
 for(const auto size : targets) {
  if_fail_fe(image.resize(size));
  for(const auto cells : grids)
   for(const ::llc::u2_t zoom : {0U, 1U, 32U}) if_fail_fe(nnHeatmap(image.View, values, cells, true, zoom));
 }
 // Fit must retain both ends of a dense strip in the visible image.
 if_fail_fe(nnHeatmap(image.View, values, {625, 1}, true, 0));
 bool positive = false, negative = false;
 for(const auto color : image.Texels) {
  positive |= color.b == 244 && color.r == 70;
  negative |= color.r == 244 && color.b == 38;
 }
 LLC_TEST_REQUIRE(results, NN_TEST_RESULT_HEATMAP, !positive || !negative);
 rtrn 0;
}
