#include "llc_nn_internal.h"

namespace {
 cnst llc::vcst_t Names[] = {"Adaline", "ART1", "BAM", "Boltzmann", "Backpropagation", "Counterpropagation", "Hopfield", "SOM", "Image painter"};
 cnst llc::vcst_t Descriptions[] = {
  "Digit recognition | input pixels | metric: maximum error",
  "Pattern clustering | resonating input | metric: winning class",
  "Names and phone numbers | phone bits | metric: sample index",
  "Traveling salesman | tour assignment | metric: temperature (length when done)",
  "Sunspot forecasting | input window | metric: test NMSE",
  "Rotation detection | input pixels | metric: angle in degrees",
  "Image recall | recalled pixels | metric: unchanged updates",
  "Pole balancing | learned forces | metric: pole angle in degrees",
  "Coordinates -> RGB | fit an image | metric: full-image RGB MSE"
 };
 cnst llc::vcst_t Reports[] = {"ADALINE.txt","ART1.txt","BAM.txt","BOLTZMAN.txt","BPN.txt","CPN.txt","HOPFIELD.txt","SOM.txt", ""};
}
llc::vcst_t llc::nnName(u2_t n) { return n < NN_COUNT ? Names[n] : vcst_t{}; }
llc::vcst_t llc::nnDescription(u2_t n) { return n < NN_COUNT ? Descriptions[n] : vcst_t{}; }
llc::vcst_t llc::nnReport(u2_t n) { return n < NN_COUNT ? Reports[n] : vcst_t{}; }
llc::err_t llc::nnReset(SNN & run) {
 if_fail_fe(run.Model.Clear());
 run.Inspection = {};
 run.Steps = 0; run.Metric = 0; run.Complete = false; run.Error = 0;
 if(run.Painter.get_ref()) {
  run.Painter->Samples = run.Painter->Pass = 0;
  run.Painter->Cursor = run.Painter->Neuron = 0; run.Painter->MSE = 0;
 }
 return 0;
}
llc::err_t llc::nnBegin(SNN & run, u2_t network, const SNNConfig & config) {
 if_true_fe(network >= NN_COUNT);
 if_true_fe(!config.HiddenUnits || config.HiddenUnits > 32);
 if_true_fe(!config.MapSize.x || config.MapSize.x > 25 || !config.MapSize.y || config.MapSize.y > 25);
 if_true_fe(network != llc::NN_BACKPROPAGATION && network != NN_IMAGE && config.HiddenUnits != 10);
 if_true_fe(network != llc::NN_SOM && (config.MapSize.x != 25 || config.MapSize.y != 25));
 const SNNConfig requested = config; // config may alias run.Config.
 if_fail_fe(nnReset(run));
 if(network != NN_IMAGE) if_fail_fe(run.Painter.clear());
 run.Config = requested;
 run.Network = network;
 sttc err_t (*cnst create[])(SNN &) = {nnCreateAdaline,nnCreateArt1,nnCreateBam,nnCreateBoltzman,nnCreateBpn,nnCreateCpn,nnCreateHopfield,nnCreateSom,nnCreateImage};
 run.Error = create[network](run);
 return run.Error;
}
llc::err_t llc::nnStep(SNN & run, u2_t budget) {
 if(run.Error < 0) return run.Error;
 if(run.Complete) return 1;
 if(!budget) return 0;
 if_true_fe(!run.Model.State || !run.Model.Advance);
 for(u2_t i = 0; i < budget; ++i) {
  cnst auto previousSize = run.Inspection.WeightSize;
  cnst auto previousWeights = run.Inspection.Weights;
  cnst auto previousRevision = run.Inspection.Revision;
  cnst err_t result = run.Model.Advance(run.Model.State, run);
  if(run.Inspection.Revision != previousRevision) {
   run.Inspection.BeforeSize = previousSize;
   run.Inspection.WeightsBefore = previousWeights;
   run.Inspection.WeightChanges = {};
   if(run.Inspection.BeforeSize == run.Inspection.WeightSize)
    for(u2_t cell = 0; cell < run.Inspection.WeightSize.x * run.Inspection.WeightSize.y; ++cell)
     run.Inspection.WeightChanges[cell] = run.Inspection.Weights[cell] - run.Inspection.WeightsBefore[cell];
  }
  ++run.Steps;
  if(result < 0) { run.Error = result; return result; }
  if(result == 1) { run.Complete = true; return 1; }
 }
 return 0;
}
