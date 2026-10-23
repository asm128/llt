/******************************************************************************

                      ====================================================
        Network:      Backpropagation Network with Bias Terms and Momentum
                      ====================================================

        Application:  Time-Series Forecasting
                      Prediction of the Annual Number of Sunspots

        Author:       Karsten Kutza
        Date:         17.4.96

        Reference:    D.E. Rumelhart, G.E. Hinton, R.J. Williams
                      Learning Internal Representations by Error Propagation
                      in:
                      D.E. Rumelhart, J.L. McClelland (Eds.)
                      Parallel Distributed Processing, Volume 1
                      MIT Press, Cambridge, MA, pp. 318-362, 1986

 ******************************************************************************/




#include "llc_nn_internal.h"
#include <cmath>

namespace {
stct SState {
/******************************************************************************
                            D E C L A R A T I O N S
 ******************************************************************************/






#define MIN_REAL      -HUGE_VAL
#define MAX_REAL      +HUGE_VAL
#define MIN(x,y)      ((x)<(y) ? (x) : (y))
#define MAX(x,y)      ((x)>(y) ? (x) : (y))

#define LO            0.1
#define HI            0.9
#define BIAS          1

#define sqr(x)        ((x)*(x))


stct SLayer {                     /* A SLayer OF A SNet:                     */
        llc::s2_t           Units;         /* - number of units in this layer       */
        llc::apod<llc::f3_t> Output = {};        /* - output of ith unit                  */
        llc::apod<llc::f3_t> Error = {};         /* - error term of ith unit              */
        llc::img<llc::f3_t> Weight = {};        /* - connection weights to ith unit      */
        llc::img<llc::f3_t> WeightSave = {};    /* - saved weights for stopped training  */
        llc::img<llc::f3_t> dWeight = {};       /* - last weight deltas for momentum     */
};

stct SNet {                     /* A SNet:                                */
        llc::aobj<llc::pobj<SLayer>> Layer = {};         /* - layers of this net                  */
        llc::pobj<SLayer> InputLayer = {};    /* - input layer                         */
        llc::pobj<SLayer> OutputLayer = {};   /* - output layer                        */
        llc::f3_t          Alpha;         /* - momentum factor                     */
        llc::f3_t          Eta;           /* - learning rate                       */
        llc::f3_t          Gain;          /* - gain of sigmoid function            */
        llc::f3_t          Error;         /* - total net error                     */
};


/******************************************************************************
        R A N D O M S   D R A W N   F R O M   D I S T R I B U T I O N S
 ******************************************************************************/


void InitializeRandoms()
{
  RandomState = 4711;
}


llc::s2_t RandomEqualINT(llc::s2_t Low, llc::s2_t High)
{
  return RandomNext() % (High-Low+1) + Low;
}      


llc::f3_t RandomEqualREAL(llc::f3_t Low, llc::f3_t High)
{
  return ((llc::f3_t) RandomNext() / 32767) * (High-Low) + Low;
}      


/******************************************************************************
               A P P L I C A T I O N - S P E C I F I C   C O D E
 ******************************************************************************/


#define NUM_LAYERS    3
#define N             30
#define M             1
llc::astatic<llc::s2_t, NUM_LAYERS> Units =  {{N, 10, M}};
bool ImageMode = false, Painting = true;
llc::u2_t PaintCursor = 0, SincePaint = 0;
llc::f3_t PaintError = 0;
SState(const llc::SNNConfig & config, bool imageMode = false) : ImageMode(imageMode) {
 Units[1] = (llc::s2_t)config.HiddenUnits;
 if(ImageMode) { Units[0] = 2; Units[2] = 3; }
}

#define FIRST_YEAR    1700
#define NUM_YEARS     280

#define TRAIN_LWB     (N)
#define TRAIN_UPB     (179)
#define TRAIN_YEARS   (TRAIN_UPB - TRAIN_LWB + 1)
#define TEST_LWB      (180)
#define TEST_UPB      (259)
#define TEST_YEARS    (TEST_UPB - TEST_LWB + 1)
#define EVAL_LWB      (260)
#define EVAL_UPB      (NUM_YEARS - 1)
#define EVAL_YEARS    (EVAL_UPB - EVAL_LWB + 1)

llc::astatic<llc::f3_t, NUM_YEARS> Sunspots_ = {};
llc::astatic<llc::f3_t, NUM_YEARS> Sunspots =  {{

                        0.0262,  0.0575,  0.0837,  0.1203,  0.1883,  0.3033,  
                        0.1517,  0.1046,  0.0523,  0.0418,  0.0157,  0.0000,  
                        0.0000,  0.0105,  0.0575,  0.1412,  0.2458,  0.3295,  
                        0.3138,  0.2040,  0.1464,  0.1360,  0.1151,  0.0575,  
                        0.1098,  0.2092,  0.4079,  0.6381,  0.5387,  0.3818,  
                        0.2458,  0.1831,  0.0575,  0.0262,  0.0837,  0.1778,  
                        0.3661,  0.4236,  0.5805,  0.5282,  0.3818,  0.2092,  
                        0.1046,  0.0837,  0.0262,  0.0575,  0.1151,  0.2092,  
                        0.3138,  0.4231,  0.4362,  0.2495,  0.2500,  0.1606,  
                        0.0638,  0.0502,  0.0534,  0.1700,  0.2489,  0.2824,  
                        0.3290,  0.4493,  0.3201,  0.2359,  0.1904,  0.1093,  
                        0.0596,  0.1977,  0.3651,  0.5549,  0.5272,  0.4268,  
                        0.3478,  0.1820,  0.1600,  0.0366,  0.1036,  0.4838,  
                        0.8075,  0.6585,  0.4435,  0.3562,  0.2014,  0.1192,  
                        0.0534,  0.1260,  0.4336,  0.6904,  0.6846,  0.6177,  
                        0.4702,  0.3483,  0.3138,  0.2453,  0.2144,  0.1114,  
                        0.0837,  0.0335,  0.0214,  0.0356,  0.0758,  0.1778,  
                        0.2354,  0.2254,  0.2484,  0.2207,  0.1470,  0.0528,  
                        0.0424,  0.0131,  0.0000,  0.0073,  0.0262,  0.0638,  
                        0.0727,  0.1851,  0.2395,  0.2150,  0.1574,  0.1250,  
                        0.0816,  0.0345,  0.0209,  0.0094,  0.0445,  0.0868,  
                        0.1898,  0.2594,  0.3358,  0.3504,  0.3708,  0.2500,  
                        0.1438,  0.0445,  0.0690,  0.2976,  0.6354,  0.7233,  
                        0.5397,  0.4482,  0.3379,  0.1919,  0.1266,  0.0560,  
                        0.0785,  0.2097,  0.3216,  0.5152,  0.6522,  0.5036,  
                        0.3483,  0.3373,  0.2829,  0.2040,  0.1077,  0.0350,  
                        0.0225,  0.1187,  0.2866,  0.4906,  0.5010,  0.4038,  
                        0.3091,  0.2301,  0.2458,  0.1595,  0.0853,  0.0382,  
                        0.1966,  0.3870,  0.7270,  0.5816,  0.5314,  0.3462,  
                        0.2338,  0.0889,  0.0591,  0.0649,  0.0178,  0.0314,  
                        0.1689,  0.2840,  0.3122,  0.3332,  0.3321,  0.2730,  
                        0.1328,  0.0685,  0.0356,  0.0330,  0.0371,  0.1862,  
                        0.3818,  0.4451,  0.4079,  0.3347,  0.2186,  0.1370,  
                        0.1396,  0.0633,  0.0497,  0.0141,  0.0262,  0.1276,  
                        0.2197,  0.3321,  0.2814,  0.3243,  0.2537,  0.2296,  
                        0.0973,  0.0298,  0.0188,  0.0073,  0.0502,  0.2479,  
                        0.2986,  0.5434,  0.4215,  0.3326,  0.1966,  0.1365,  
                        0.0743,  0.0303,  0.0873,  0.2317,  0.3342,  0.3609,  
                        0.4069,  0.3394,  0.1867,  0.1109,  0.0581,  0.0298,  
                        0.0455,  0.1888,  0.4168,  0.5983,  0.5732,  0.4644,  
                        0.3546,  0.2484,  0.1600,  0.0853,  0.0502,  0.1736,  
                        0.4843,  0.7929,  0.7128,  0.7045,  0.4388,  0.3630,  
                        0.1647,  0.0727,  0.0230,  0.1987,  0.7411,  0.9947,  
                        0.9665,  0.8316,  0.5873,  0.2819,  0.1961,  0.1459,  
                        0.0534,  0.0790,  0.2458,  0.4906,  0.5539,  0.5518,  
                        0.5465,  0.3483,  0.3603,  0.1987,  0.1804,  0.0811,  
                        0.0659,  0.1428,  0.4838,  0.8127 

                      }};

llc::f3_t                  Mean;
llc::f3_t                  TrainError;
llc::f3_t                  TrainErrorPredictingMean;
llc::f3_t                  TestError;
llc::f3_t                  TestErrorPredictingMean;

FILE * f = {};
llc::u2_t RandomState = 4711;
llc::s2_t RandomNext() { RandomState = RandomState * 214013U + 2531011U; rtrn (RandomState >> 16) & 32767; }
~SState() { if(f) fclose(f); }


void NormalizeSunspots()
{
  llc::s2_t  Year;
  llc::f3_t Min, Max;
	
  Min = MAX_REAL;
  Max = MIN_REAL;
  for (Year=0; Year<NUM_YEARS; Year++) {
    Min = MIN(Min, Sunspots[Year]);
    Max = MAX(Max, Sunspots[Year]);
  }
  Mean = 0;
  for (Year=0; Year<NUM_YEARS; Year++) {
    Sunspots_[Year] = 
    Sunspots [Year] = ((Sunspots[Year]-Min) / (Max-Min)) * (HI-LO) + LO;
    Mean += Sunspots[Year] / NUM_YEARS;
  }
}


llc::err_t InitializeApplication(SNet* Net)
{
  llc::s2_t  Year, i;
  llc::f3_t Out, Err;

  Net->Alpha = 0.5;
  Net->Eta   = 0.05;
  Net->Gain  = 1;

  NormalizeSunspots();
  TrainErrorPredictingMean = 0;
  for (Year=TRAIN_LWB; Year<=TRAIN_UPB; Year++) {
    for (i=0; i<M; i++) {
      Out = Sunspots[Year+i];
      Err = Mean - Out;
      TrainErrorPredictingMean += 0.5 * sqr(Err);
    }
  }
  TestErrorPredictingMean = 0;
  for (Year=TEST_LWB; Year<=TEST_UPB; Year++) {
    for (i=0; i<M; i++) {
      Out = Sunspots[Year+i];
      Err = Mean - Out;
      TestErrorPredictingMean += 0.5 * sqr(Err);
    }
  }
  if_true_fe(fopen_s(&f, "BPN.txt", "w"));
  rtrn 0;
}





/******************************************************************************
                          I N I T I A L I Z A T I O N
 ******************************************************************************/


llc::err_t GenerateNetwork(SNet* Net)
{
  llc::s2_t l;

  if_fail_fe(Net->Layer.resize(NUM_LAYERS));
   
  for (l=0; l<NUM_LAYERS; l++) {
    if_null_fe(Net->Layer[l].create());
      
    Net->Layer[l]->Units      = Units[l];
    if_fail_fe(Net->Layer[l]->Output.resize(Units[l]+1, 0));
    if_fail_fe(Net->Layer[l]->Error.resize(Units[l]+1, 0));
    if(l) if_fail_fe(Net->Layer[l]->Weight.resize(llc::n2u2_t{(llc::u2_t)(Units[l-1]+1), (llc::u2_t)(Units[l]+1)}, 0));
    if(l) if_fail_fe(Net->Layer[l]->WeightSave.resize(llc::n2u2_t{(llc::u2_t)(Units[l-1]+1), (llc::u2_t)(Units[l]+1)}, 0));
    if(l) if_fail_fe(Net->Layer[l]->dWeight.resize(llc::n2u2_t{(llc::u2_t)(Units[l-1]+1), (llc::u2_t)(Units[l]+1)}, 0));
    Net->Layer[l]->Output[0]  = BIAS;
      
  }
  Net->InputLayer  = Net->Layer[0];
  Net->OutputLayer = Net->Layer[NUM_LAYERS - 1];
  Net->Alpha       = 0.9;
  Net->Eta         = 0.25;
  Net->Gain        = 1;
  rtrn 0;
}


void RandomWeights(SNet* Net)
{
  llc::s2_t l,i,j;
   
  for (l=1; l<NUM_LAYERS; l++) {
    for (i=1; i<=Net->Layer[l]->Units; i++) {
      for (j=0; j<=Net->Layer[l-1]->Units; j++) {
        Net->Layer[l]->Weight[i][j] = RandomEqualREAL(-0.5, 0.5);
      }
    }
  }
}


void SetInput(SNet* Net, llc::view<llc::f3_t> argInput)
{
  llc::s2_t i;
   
  for (i=1; i<=Net->InputLayer->Units; i++) {
    Net->InputLayer->Output[i] = argInput[i-1];
  }
}


void GetOutput(SNet* Net, llc::view<llc::f3_t> argOutput)
{
  llc::s2_t i;
   
  for (i=1; i<=Net->OutputLayer->Units; i++) {
    argOutput[i-1] = Net->OutputLayer->Output[i];
  }
}


/******************************************************************************
            S U P P O R T   F O R   S T O P P E D   T R A I N I N G
 ******************************************************************************/


void SaveWeights(SNet* Net)
{
  llc::s2_t l,i,j;

  for (l=1; l<NUM_LAYERS; l++) {
    for (i=1; i<=Net->Layer[l]->Units; i++) {
      for (j=0; j<=Net->Layer[l-1]->Units; j++) {
        Net->Layer[l]->WeightSave[i][j] = Net->Layer[l]->Weight[i][j];
      }
    }
  }
}


void RestoreWeights(SNet* Net)
{
  llc::s2_t l,i,j;

  for (l=1; l<NUM_LAYERS; l++) {
    for (i=1; i<=Net->Layer[l]->Units; i++) {
      for (j=0; j<=Net->Layer[l-1]->Units; j++) {
        Net->Layer[l]->Weight[i][j] = Net->Layer[l]->WeightSave[i][j];
      }
    }
  }
}


/******************************************************************************
                     P R O P A G A T I N G   S I G N A L S
 ******************************************************************************/


void PropagateLayer(SNet* Net, SLayer* Lower, SLayer* Upper)
{
  llc::s2_t  i,j;
  llc::f3_t Sum;

  for (i=1; i<=Upper->Units; i++) {
    Sum = 0;
    for (j=0; j<=Lower->Units; j++) {
      Sum += Upper->Weight[i][j] * Lower->Output[j];
    }
    Upper->Output[i] = 1 / (1 + exp(-Net->Gain * Sum));
  }
}


void PropagateNet(SNet* Net)
{
  llc::s2_t l;
   
  for (l=0; l<NUM_LAYERS-1; l++) {
    PropagateLayer(Net, Net->Layer[l], Net->Layer[l+1]);
  }
}


/******************************************************************************
                  B A C K P R O P A G A T I N G   E R R O R S
 ******************************************************************************/


void ComputeOutputError(SNet* Net, llc::view<llc::f3_t> Target)
{
  llc::s2_t  i;
  llc::f3_t Out, Err;
   
  Net->Error = 0;
  for (i=1; i<=Net->OutputLayer->Units; i++) {
    Out = Net->OutputLayer->Output[i];
    Err = Target[i-1]-Out;
    Net->OutputLayer->Error[i] = Net->Gain * Out * (1-Out) * Err;
    Net->Error += 0.5 * sqr(Err);
  }
}


void BackpropagateLayer(SNet* Net, SLayer* Upper, SLayer* Lower)
{
  llc::s2_t  i,j;
  llc::f3_t Out, Err;
   
  for (i=1; i<=Lower->Units; i++) {
    Out = Lower->Output[i];
    Err = 0;
    for (j=1; j<=Upper->Units; j++) {
      Err += Upper->Weight[j][i] * Upper->Error[j];
    }
    Lower->Error[i] = Net->Gain * Out * (1-Out) * Err;
  }
}


void BackpropagateNet(SNet* Net)
{
  llc::s2_t l;
   
  for (l=NUM_LAYERS-1; l>1; l--) {
    BackpropagateLayer(Net, Net->Layer[l], Net->Layer[l-1]);
  }
}


void AdjustWeights(SNet* Net)
{
  llc::s2_t  l,i,j;
  llc::f3_t Out, Err, dWeight;
   
  for (l=1; l<NUM_LAYERS; l++) {
    for (i=1; i<=Net->Layer[l]->Units; i++) {
      for (j=0; j<=Net->Layer[l-1]->Units; j++) {
        Out = Net->Layer[l-1]->Output[j];
        Err = Net->Layer[l]->Error[i];
        dWeight = Net->Layer[l]->dWeight[i][j];
        Net->Layer[l]->Weight[i][j] += Net->Eta * Err * Out + Net->Alpha * dWeight;
        Net->Layer[l]->dWeight[i][j] = Net->Eta * Err * Out;
      }
    }
  }
}


/******************************************************************************
                      S I M U L A T I N G   T H E   N E T
 ******************************************************************************/


bool Inspected = false, LastTraining = false;
llc::f3_t LastTarget = 0;
void SimulateNet(SNet* Net, llc::view<llc::f3_t> argInput, llc::view<llc::f3_t> argOutput, llc::view<llc::f3_t> Target, llc::s2_t Training)
{
  Inspected = true; LastTraining = Training != 0; LastTarget = Target[0];
  SetInput(Net, argInput);
  PropagateNet(Net);
  GetOutput(Net, argOutput);
   
  ComputeOutputError(Net, Target);
  if (Training) {
    BackpropagateNet(Net);
    AdjustWeights(Net);
  }
}











/******************************************************************************
                                    M A I N
 ******************************************************************************/




SNet Network = {};
llc::err_t ImageStep(llc::SNN & run) {
 const auto size = run.Painter->Source.metrics();
 if_true_fe(!size.x || !size.y);
 const llc::u2_t count = size.x * size.y;
 const llc::u2_t budget = Painting ? llc::min(64U, count - PaintCursor) : 1U;
 for(llc::u2_t unit = 0; unit < budget; ++unit) {
  const llc::u2_t pixel = Painting ? PaintCursor++ : (llc::u2_t)RandomEqualINT(0, (llc::s2_t)count - 1);
  const auto source = run.Painter->Source.Texels[pixel];
  llc::astatic<llc::f3_t, 2> coordinates = {{size.x > 1 ? 2.0 * (pixel % size.x) / (size.x - 1) - 1 : 0,
   size.y > 1 ? 2.0 * (pixel / size.x) / (size.y - 1) - 1 : 0}};
  llc::astatic<llc::f3_t, 3> target = {{source.r / 255.0, source.g / 255.0, source.b / 255.0}}, output = {};
  SimulateNet(&Network, coordinates, output, target, !Painting);
  if(!Painting) { ++run.Painter->Samples; continue; }
  llc::astatic<llc::u0_t, 3> rgb = {}, error = {}, influence = {};
  for(llc::u2_t channel = 0; channel < 3; ++channel) {
   const auto difference = output[channel] - target[channel];
   PaintError += difference * difference;
   rgb[channel] = (llc::u0_t)(255 * output[channel] + 0.5);
   error[channel] = (llc::u0_t)(255 * llc::min(1.0, std::abs(difference) * 4));
   // Exact output with this hidden neuron's weighted contribution removed, bias retained.
   llc::f3_t sum = Network.OutputLayer->Weight[channel + 1][0];
   for(llc::u2_t hidden = 1; hidden <= (llc::u2_t)Units[1]; ++hidden)
    if(hidden != run.Painter->Neuron + 1) sum += Network.OutputLayer->Weight[channel + 1][hidden] * Network.Layer[1]->Output[hidden];
   const auto without = 1 / (1 + exp(-Network.Gain * sum));
   influence[channel] = (llc::u0_t)(255 * llc::min(1.0, std::abs(output[channel] - without) * 4));
  }
  run.Painter->Prediction.Texels[pixel] = {rgb[2], rgb[1], rgb[0], 255};
  run.Painter->Error.Texels[pixel] = {error[2], error[1], error[0], 255};
  run.Painter->Influence.Texels[pixel] = {influence[2], influence[1], influence[0], 255};
 }
 run.Painter->Cursor = PaintCursor;
 if(Painting && PaintCursor == count) {
  run.Painter->MSE = run.Metric = PaintError / (3 * count);
  ++run.Painter->Pass; Painting = false; SincePaint = 0;
 } else if(!Painting && ++SincePaint >= 256) {
  Painting = true; PaintCursor = 0; PaintError = 0;
 }
 rtrn 0;
}
void Snapshot(llc::SNN & run) {
 // Preview evaluation must not replace the last published training operation.
 if(ImageMode && Inspected && !LastTraining) rtrn;
 run.Inspection.Width = ImageMode ? 2 : 10; run.Inspection.Count = (llc::u2_t)Units[0];
 for(llc::u2_t i = 0; i < run.Inspection.Count; ++i) run.Inspection.Values[i] = Network.InputLayer->Output[i+1];
 llc::nnWeights(run, Network.Layer[1]->Weight);
 run.Inspection.Inspected = Inspected; run.Inspection.Training = LastTraining;
 run.Inspection.Target = LastTarget; run.Inspection.Prediction = Network.OutputLayer->Output[1]; run.Inspection.SampleLoss = Network.Error;
 run.Inspection.Activations = {}; run.Inspection.ErrorTerms = {};
 for(llc::u2_t l = 0; l < NUM_LAYERS; ++l) {
  run.Inspection.LayerUnits[l] = (llc::u2_t)Network.Layer[l]->Units;
  for(llc::u2_t unit = 0; unit < run.Inspection.LayerUnits[l] && unit < 32; ++unit) {
   run.Inspection.Activations[l * 32 + unit] = Network.Layer[l]->Output[unit + 1];
   // Hidden errors are stale during evaluation; never expose those as current.
   if(l && (LastTraining || l == NUM_LAYERS - 1)) run.Inspection.ErrorTerms[l * 32 + unit] = Network.Layer[l]->Error[unit + 1];
  }
 }
}
llc::s2_t Phase = 0, Cursor = 0;
llc::f3_t MinTestError = HUGE_VAL;
bool FinalTest = false;
llc::err_t Begin() {
 InitializeRandoms(); if_fail_fe(GenerateNetwork(&Network)); RandomWeights(&Network);
 if(ImageMode) { Network.Eta = 0.05; rtrn 0; }
 rtrn InitializeApplication(&Network);
}
llc::err_t Step(llc::SNN & run) {
 if(ImageMode) rtrn ImageStep(run);
 llc::astatic<llc::f3_t, M> prediction = {}, closed = {};
 if(Phase == 0) {
  const auto year = RandomEqualINT(TRAIN_LWB, TRAIN_UPB);
  SimulateNet(&Network, {&Sunspots[year-N], N}, prediction, {&Sunspots[year], M}, true);
  if(++Cursor == 10 * TRAIN_YEARS) { Phase = 1; Cursor = TRAIN_LWB; TrainError = TestError = 0; }
 } else if(Phase == 1) {
  SimulateNet(&Network, {&Sunspots[Cursor-N], N}, prediction, {&Sunspots[Cursor], M}, false);
  if(Cursor <= TRAIN_UPB) TrainError += Network.Error; else TestError += Network.Error;
  if(++Cursor > TEST_UPB) {
   fprintf(f, "\nNMSE is %0.3f on Training Set and %0.3f on Test Set", TrainError / TrainErrorPredictingMean, TestError / TestErrorPredictingMean);
   run.Metric = TestError / TestErrorPredictingMean;
   if(FinalTest) {
    Phase = 2; Cursor = EVAL_LWB;
    fprintf(f, "\n\n\nYear    Sunspots    Open-Loop Prediction    Closed-Loop Prediction\n\n");
   } else {
    Phase = 0; Cursor = 0;
    if(TestError < MinTestError) { fprintf(f, " - saving Weights ..."); MinTestError = TestError; SaveWeights(&Network); }
    else if(TestError > 1.2 * MinTestError) {
     fprintf(f, " - stopping Training and restoring Weights ..."); RestoreWeights(&Network);
     FinalTest = true; Phase = 1; Cursor = TRAIN_LWB; TrainError = TestError = 0;
    }
   }
  }
 } else {
  SimulateNet(&Network, {&Sunspots[Cursor-N], N}, prediction, {&Sunspots[Cursor], M}, false);
  SimulateNet(&Network, {&Sunspots_[Cursor-N], N}, closed, {&Sunspots_[Cursor], M}, false);
  Sunspots_[Cursor] = closed[0];
  fprintf(f, "%d       %0.3f                   %0.3f                     %0.3f\n", FIRST_YEAR + Cursor, Sunspots[Cursor], prediction[0], closed[0]);
  ++Cursor;
 }
 run.Inspection.Width = 10; run.Inspection.Count = N;
 for(llc::u2_t i = 0; i < N; ++i) run.Inspection.Values[i] = Network.InputLayer->Output[i+1];
 rtrn Phase == 2 && Cursor > EVAL_UPB ? 1 : 0;
}

};
} // namespace
llc::err_t llc::nnCreateBpn(SNN & run) { rtrn nnCreate<SState>(run, run.Config); }
llc::err_t llc::nnCreateImage(SNN & run) {
 if(!run.Painter.get_ref()) if_null_fe(run.Painter.create());
 if(!run.Painter->Source.size()) {
  if_fail_fe(run.Painter->Source.resize(n2u2_t{64, 64}));
  for(u2_t y = 0; y < 64; ++y) for(u2_t x = 0; x < 64; ++x) {
   const double dx = (x - 31.5) / 32, dy = (y - 31.5) / 32;
   const double glow = exp(-5 * (dx * dx + dy * dy));
   run.Painter->Source[y][x] = {(u0_t)(220 * glow), (u0_t)(40 + 180 * y / 63), (u0_t)(40 + 180 * x / 63), 255};
  }
 }
 for(auto * image : {&run.Painter->Prediction, &run.Painter->Error, &run.Painter->Influence}) {
  if_fail_fe(image->resize(run.Painter->Source.metrics()));
  if_fail_fe(image->View.fill(bgra{22, 18, 16, 255}));
 }
 rtrn nnCreate<SState>(run, run.Config, true);
}
llc::err_t llc::nnImageSource(SNN & run, gc8bgra source) {
 if_true_fe(run.Network != NN_IMAGE || !run.Painter.get_ref() || !source.metrics().x || !source.metrics().y || source.metrics().x > 64 || source.metrics().y > 64);
 img8bgra copy;
 if_fail_fe(copy.resize(source.metrics()));
 for(u2_t i = 0; i < copy.size(); ++i) copy.Texels[i] = source.begin()[i];
 if_fail_fe(run.Painter->Source.resize(copy.metrics()));
 for(u2_t i = 0; i < copy.size(); ++i) run.Painter->Source.Texels[i] = copy.Texels[i];
 rtrn nnBegin(run, NN_IMAGE, run.Config);
}
llc::err_t llc::nnImageInspect(SNN & run, u2_t neuron) {
 if_true_fe(run.Network != NN_IMAGE || !run.Model.State || neuron >= run.Config.HiddenUnits);
 run.Painter->Neuron = neuron;
 SState & state = *((gref<SState>*)run.Model.State)->Instance;
 state.Painting = true; state.PaintCursor = 0; state.PaintError = 0;
 run.Painter->Cursor = 0;
 rtrn run.Painter->Influence.View.fill(bgra{22, 18, 16, 255});
}
llc::err_t llc::nnImagePreview(SNN & run) {
 if_true_fe(run.Network != NN_IMAGE || !run.Model.State);
 SState & state = *((gref<SState>*)run.Model.State)->Instance;
 if(state.Painting) if_fail_fe(state.ImageStep(run));
 rtrn 0;
}
