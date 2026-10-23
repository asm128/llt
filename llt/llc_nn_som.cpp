/******************************************************************************

                      ===================
        Network:      Self-Organizing Map
                      ===================

        Application:  Control
                      Pole Balancing Problem

        Author:       Karsten Kutza
        Date:         6.6.96

        Reference:    T. Kohonen	
                      Self-Organized Formation
                      of Topologically Correct Feature Maps
                      Biological Cybernetics, 43, pp. 59-69, 1982

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

#define PI            (2*asin(1))
#define sqr(x)        ((x)*(x))


stct SLayer {                     /* A SLayer OF A SNet:                     */
        llc::s2_t           Units;         /* - number of units in this layer       */
        llc::apod<llc::f3_t> Output = {};        /* - output of ith unit                  */
        llc::img<llc::f3_t> Weight = {};        /* - connection weights to ith unit      */
        llc::apod<llc::f3_t> StepSize = {};      /* - size of search steps of ith unit    */
        llc::apod<llc::f3_t> dScoreMean = {};    /* - mean score delta of ith unit        */
};

stct SNet {                     /* A SNet:                                */
        llc::pobj<SLayer> InputLayer = {};    /* - input layer                         */
        llc::pobj<SLayer> KohonenLayer = {};  /* - Kohonen layer                       */
        llc::pobj<SLayer> OutputLayer = {};   /* - output layer                        */
        llc::s2_t           Winner;        /* - last winner in Kohonen layer        */
        llc::f3_t          Alpha;         /* - learning rate for Kohonen layer     */
        llc::f3_t          Alpha_;        /* - learning rate for output layer      */
        llc::f3_t          Alpha__;       /* - learning rate for step sizes        */
        llc::f3_t          Gamma;         /* - smoothing factor for score deltas   */
        llc::f3_t          Sigma;         /* - parameter for width of neighborhood */
};


/******************************************************************************
        R A N D O M S   D R A W N   F R O M   D I S T R I B U T I O N S
 ******************************************************************************/


void InitializeRandoms()
{
  RandomState = 4715;
}      


llc::f3_t RandomEqualREAL(llc::f3_t Low, llc::f3_t High)
{
  return ((llc::f3_t) RandomNext() / 32767) * (High-Low) + Low;
}      


      


/******************************************************************************
               A P P L I C A T I O N - S P E C I F I C   C O D E
 ******************************************************************************/


llc::s2_t ROWS = 25, COLS = 25;
SState(const llc::SNNConfig & config) : ROWS((llc::s2_t)config.MapSize.y), COLS((llc::s2_t)config.MapSize.x) {}

#define N             2
#define C             (ROWS * COLS)
#define M             1

#define TRAIN_STEPS   10000
#define BALANCED      100

FILE * f = {};
llc::u2_t RandomState = 4711;
llc::s2_t RandomNext() { RandomState = RandomState * 214013U + 2531011U; rtrn (RandomState >> 16) & 32767; }
~SState() { if(f) fclose(f); }


llc::err_t InitializeApplication(SNet* Net)
{
  llc::s2_t i;
   
  for (i=0; i<Net->KohonenLayer->Units; i++) {
    Net->KohonenLayer->StepSize[i] = 1;
    Net->KohonenLayer->dScoreMean[i] = 0;
  }
  if_true_fe(fopen_s(&f, "SOM.txt", "w"));
  rtrn 0;
}








/******************************************************************************
                     S I M U L A T I N G   T H E   P O L E
 ******************************************************************************/

                                     /* SIMULATION PARAMETERS FOR THE SPole:   */
#define L             1              /* - length of pole [m]                  */
#define Mc            1              /* - mass of cart [kg]                   */
#define Mp            1              /* - mass of pole [kg]                   */
#define G             9.81           /* - acceleration due to gravity [m/s\xB0]  */
#define T             0.1            /* - time step [s]                       */
#define STEPS         10             /* - simulation steps in one time step   */


stct SPole {                     /* STATE VARIABLES OF A SPole:            */
        llc::f3_t          x;             /* - position of cart [m]                */
        llc::f3_t          xDot;          /* - velocity of cart [m/s]              */
        llc::f3_t          w;             /* - angle of pole [\xB0]                   */
        llc::f3_t          wDot;          /* - angular velocity of pole [\xB0/s]      */
        llc::f3_t          F;             /* - force applied to cart               */
};                              /*   during current time step [N]        */





void SimulatePole(SPole* Pole)
{
  llc::s2_t  s;
  llc::f3_t x, xDot, xDotDot;           
  llc::f3_t w, wDot, wDotDot;                   
  llc::f3_t F;                    

  x    = Pole->x;
  xDot = Pole->xDot;
  w    = (Pole->w    / 180) * PI;
  wDot = (Pole->wDot / 180) * PI;
  F    = Pole->F;
  for (s=0; s<STEPS; s++) {

    wDotDot = (G*sin(w) + cos(w) * ((-F - Mp*L*sqr(wDot)*sin(w)) / (Mc+Mp))) / 
              (L * ((llc::f3_t) 4/3 - (Mp*sqr(cos(w))) / (Mc+Mp)));

    xDotDot = (F + Mp*L * (sqr(wDot)*sin(w) - wDotDot*cos(w))) / (Mc+Mp);

    x    += (T / STEPS) * xDot;
    xDot += (T / STEPS) * xDotDot;
    w    += (T / STEPS) * wDot;
    wDot += (T / STEPS) * wDotDot;
  }
  Pole->x    = x;
  Pole->xDot = xDot;
  Pole->w    = (w    / PI) * 180;
  Pole->wDot = (wDot / PI) * 180;
}


llc::s2_t PoleStillBalanced(SPole* Pole)
{
  return (Pole->w >= -60) && (Pole->w <= 60);
}


llc::f3_t ScoreOfPole(SPole* Pole)
{
  return -sqr(Pole->w);
}


/******************************************************************************
                          I N I T I A L I Z A T I O N
 ******************************************************************************/


llc::err_t GenerateNetwork(SNet* Net)
{

  if_null_fe(Net->InputLayer.create());
  if_null_fe(Net->KohonenLayer.create());
  if_null_fe(Net->OutputLayer.create());

  Net->InputLayer->Units        = N;
  if_fail_fe(Net->InputLayer->Output.resize(N, 0));
      
  Net->KohonenLayer->Units      = C;
  if_fail_fe(Net->KohonenLayer->Output.resize(C, 0));
  if_fail_fe(Net->KohonenLayer->Weight.resize(llc::n2u2_t{(llc::u2_t)(N), (llc::u2_t)(C)}, 0));
  if_fail_fe(Net->KohonenLayer->StepSize.resize(C, 0));
  if_fail_fe(Net->KohonenLayer->dScoreMean.resize(C, 0));
      
  Net->OutputLayer->Units       = M;
  if_fail_fe(Net->OutputLayer->Output.resize(M, 0));
  if_fail_fe(Net->OutputLayer->Weight.resize(llc::n2u2_t{(llc::u2_t)(C), (llc::u2_t)(M)}, 0));
  rtrn 0;
}


void RandomWeights(SNet* Net)
{
  llc::s2_t i,j;
   
  for (i=0; i<Net->KohonenLayer->Units; i++) {
    for (j=0; j<Net->InputLayer->Units; j++) {
      Net->KohonenLayer->Weight[i][j] = RandomEqualREAL(-30, 30);
    }
  }
  for (i=0; i<Net->OutputLayer->Units; i++) {
    for (j=0; j<Net->KohonenLayer->Units; j++) {
      Net->OutputLayer->Weight[i][j] = 0;
    }
  }
}


void SetInput(SNet* Net, llc::view<llc::f3_t> argInput)
{
  llc::s2_t i;
   
  for (i=0; i<Net->InputLayer->Units; i++) {
    Net->InputLayer->Output[i] = argInput[i];
  }
}


void GetOutput(SNet* Net, llc::view<llc::f3_t> argOutput)
{
  llc::s2_t i;
   
  for (i=0; i<Net->OutputLayer->Units; i++) {
    argOutput[i] = Net->OutputLayer->Output[i];
  }
}


/******************************************************************************
                     P R O P A G A T I N G   S I G N A L S
 ******************************************************************************/


void PropagateToKohonen(SNet* Net)
{
  llc::s2_t  i,j;
  llc::f3_t Out, Weight, Sum, MinOut;

  for (i=0; i<Net->KohonenLayer->Units; i++) {
    Sum = 0;
    for (j=0; j<Net->InputLayer->Units; j++) {
      Out = Net->InputLayer->Output[j];
      Weight = Net->KohonenLayer->Weight[i][j];
      Sum += sqr(Out - Weight);
    }
    Net->KohonenLayer->Output[i] = sqrt(Sum);
  }
  MinOut = MAX_REAL;
  for (i=0; i<Net->KohonenLayer->Units; i++) {
    if (Net->KohonenLayer->Output[i] < MinOut)
      MinOut = Net->KohonenLayer->Output[Net->Winner = i];
  }
}


void PropagateToOutput(SNet* Net)
{
  llc::s2_t i;

  for (i=0; i<Net->OutputLayer->Units; i++) {
    Net->OutputLayer->Output[i] = Net->OutputLayer->Weight[i][Net->Winner];
  }
}


void PropagateNet(SNet* Net)
{
  PropagateToKohonen(Net);
  PropagateToOutput(Net);
}


/******************************************************************************
                        T R A I N I N G   T H E   N E T
 ******************************************************************************/


llc::f3_t Neighborhood(SNet* Net, llc::s2_t i)
{
  llc::s2_t  iRow, iCol, jRow, jCol;
  llc::f3_t Distance;

  iRow = i / COLS; jRow = Net->Winner / COLS;
  iCol = i % COLS; jCol = Net->Winner % COLS;

  Distance = sqrt(sqr(iRow-jRow) + sqr(iCol-jCol));

  return exp(-sqr(Distance) / (2*sqr(Net->Sigma)));
}


void TrainKohonen(SNet* Net, llc::view<llc::f3_t> argInput)
{
  llc::s2_t  i,j;
  llc::f3_t Out, Weight, Lambda = 0, StepSize;

  for (i=0; i<Net->KohonenLayer->Units; i++) {
    for (j=0; j<Net->InputLayer->Units; j++) {
      Out = argInput[j];
      Weight = Net->KohonenLayer->Weight[i][j];
      Lambda = Neighborhood(Net, i);
      Net->KohonenLayer->Weight[i][j] += Net->Alpha * Lambda * (Out - Weight);
    }
    StepSize = Net->KohonenLayer->StepSize[i];
    Net->KohonenLayer->StepSize[i] += Net->Alpha__ * Lambda * -StepSize;
  }
}


void TrainOutput(SNet* Net, llc::view<llc::f3_t> argOutput)
{
  llc::s2_t  i,j;
  llc::f3_t Out, Weight, Lambda;

  for (i=0; i<Net->OutputLayer->Units; i++) {
    for (j=0; j<Net->KohonenLayer->Units; j++) {
      Out = argOutput[i];
      Weight = Net->OutputLayer->Weight[i][j];
      Lambda = Neighborhood(Net, j);
      Net->OutputLayer->Weight[i][j] += Net->Alpha_ * Lambda * (Out - Weight);
    }
  }
}


void TrainUnits(SNet* Net, llc::view<llc::f3_t> argInput, llc::view<llc::f3_t> argOutput)
{
  TrainKohonen(Net, argInput);
  TrainOutput(Net, argOutput);
}





/******************************************************************************
                                    M A I N
 ******************************************************************************/




SNet Network = {};
void Snapshot(llc::SNN & run) {
 llc::nnWeights(run, Network.OutputLayer->Weight);
 run.Width = (llc::u2_t)COLS; run.Count = (llc::u2_t)C;
 for(llc::u2_t i = 0; i < run.Count; ++i) run.Values[i] = Network.OutputLayer->Weight[0][i];
}
SPole LivePole = {};
llc::s2_t Phase = 0, Trained = 0, Tick = 0, ReportRow = 0;
llc::f3_t PreviousAngle = 0, CurrentAngle = 0;
llc::astatic<llc::f3_t, N> SampleInput = {};
llc::astatic<llc::f3_t, M> SampleOutput = {};
llc::err_t Begin() {
 InitializeRandoms(); if_fail_fe(GenerateNetwork(&Network)); RandomWeights(&Network); rtrn InitializeApplication(&Network);
}
llc::err_t Step(llc::SNN & run) {
 if(Phase == 0) {
  if(Trained >= TRAIN_STEPS) { Phase = 4; fprintf(f, "\n\n\n"); }
  else {
   LivePole = {}; LivePole.w = RandomEqualREAL(-30, 30);
   if(LivePole.w == 0) rtrn 0;
   Tick = 0;
   fprintf(f, " Time     Angle     Force\n");
   fprintf(f, "%4.1fs    %5.1f\xB0    %5.1fN\n", Tick * T, LivePole.w, LivePole.F);
   PreviousAngle = LivePole.w; SimulatePole(&LivePole); CurrentAngle = LivePole.w; Phase = 1;
  }
 } else if(Phase == 1) {
  if(!PoleStillBalanced(&LivePole) || Tick >= BALANCED) {
   if(PoleStillBalanced(&LivePole)) fprintf(f, "Pole still balanced after %0.1fs ...\n\n", Tick * T);
   else fprintf(f, "Pole fallen after %0.1fs ...\n\n", (Tick+1) * T);
   Phase = 0;
  } else {
   ++Trained; ++Tick;
   Network.Alpha = Network.Alpha_ = 0.5 * pow(0.01, (llc::f3_t)Trained / TRAIN_STEPS);
   Network.Alpha__ = 0.005; Network.Gamma = 0.05;
   Network.Sigma = 6.0 * pow(0.2, (llc::f3_t)Trained / TRAIN_STEPS);
   SampleInput[0] = PreviousAngle; SampleInput[1] = CurrentAngle;
   SetInput(&Network, SampleInput); PropagateNet(&Network); GetOutput(&Network, SampleOutput);
   LivePole.F = SampleOutput[0]; Phase = 2;
  }
 } else if(Phase == 2) {
  const auto x = RandomEqualREAL(-30, 30);
  const auto fx = (1 / (sqrt(2*PI)*10)) * exp(-sqr(x) / 200);
  if(fx >= RandomEqualREAL(0, 1)) { LivePole.F += Network.KohonenLayer->StepSize[Network.Winner] * x; Phase = 3; }
 } else if(Phase == 3) {
  fprintf(f, "%4.1fs    %5.1f\xB0    %5.1fN\n", Tick * T, LivePole.w, LivePole.F);
  PreviousAngle = LivePole.w;
  const auto oldScore = ScoreOfPole(&LivePole);
  SimulatePole(&LivePole); CurrentAngle = LivePole.w;
  const auto delta = ScoreOfPole(&LivePole) - oldScore;
  const auto mean = Network.KohonenLayer->dScoreMean[Network.Winner];
  if(delta > mean) {
   llc::astatic<llc::f3_t, M> target = {LivePole.F};
   TrainUnits(&Network, SampleInput, target);
  }
  Network.KohonenLayer->dScoreMean[Network.Winner] += Network.Gamma * (delta - mean);
  Phase = 1;
 } else {
  for(llc::s2_t c = 0; c < COLS; ++c) {
   const auto i = ReportRow * COLS + c;
   fprintf(f, "([%5.1f\xB0, %5.1f\xB0], %5.1fN)    ", Network.KohonenLayer->Weight[i][0], Network.KohonenLayer->Weight[i][1], Network.OutputLayer->Weight[0][i]);
  }
  fprintf(f, "\n"); ++ReportRow;
 }
 run.Width = COLS; run.Count = C; run.Metric = LivePole.w;
 for(llc::u2_t i = 0; i < (llc::u2_t)C; ++i) run.Values[i] = Network.OutputLayer->Weight[0][i];
 rtrn ReportRow == ROWS ? 1 : 0;
}

};
} // namespace
llc::err_t llc::nnCreateSom(SNN & run) { rtrn nnCreate<SState>(run, run.Config); }
