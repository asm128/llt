/******************************************************************************

                      ===========================
        Network:      Adaptive Resonance Theory 1
                      ===========================

        Application:  Brain Modeling
                      Stability-Plasticity Demonstration

        Author:       Karsten Kutza
        Date:         27.6.96

        Reference:    G.A. Carpenter, S. Grossberg
                      A Massively Parallel Architecture
                      for a Self-Organizing Neural Pattern Recognition Machine
                      Computer Vision, Graphics, and Image Processing, 37,
                      pp. 54-115, 1987

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


stct SLayer {                     /* A SLayer OF A SNet:                     */
        llc::s2_t           Units;         /* - number of units in this layer       */
        llc::apod<llc::s2_t> Output = {};        /* - output of ith unit                  */
        llc::img<llc::f3_t> Weight = {};        /* - connection weights to ith unit      */
        llc::apod<llc::s2_t> Inhibited = {};     /* - inhibition status of ith F2 unit    */
};

stct SNet {                     /* A SNet:                                */
        llc::pobj<SLayer> F1 = {};            /* - F1 layer                            */
        llc::pobj<SLayer> F2 = {};            /* - F2 layer                            */
        llc::s2_t           Winner;        /* - last winner in F2 layer             */
        llc::f3_t          A1;            /* - A parameter for F1 layer            */
        llc::f3_t          B1;            /* - B parameter for F1 layer            */
        llc::f3_t          C1;            /* - C parameter for F1 layer            */
        llc::f3_t          D1;            /* - D parameter for F1 layer            */
        llc::f3_t          L;             /* - L parameter for net                 */
        llc::f3_t          Rho;           /* - vigilance parameter                 */
};


/******************************************************************************
               A P P L I C A T I O N - S P E C I F I C   C O D E
 ******************************************************************************/


#define N             5
#define M             10

#define NO_WINNER     M

#define NUM_DATA      30

llc::astatic<llc::sc_t[N+1], NUM_DATA> Pattern =  {{ "   O ",
                                               "  O O",
                                               "    O",
                                               "  O O",
                                               "    O",
                                               "  O O",
                                               "    O",
                                               " OO O",
                                               " OO  ",
                                               " OO O",
                                               " OO  ",
                                               "OOO  ",
                                               "OO   ",
                                               "O    ",
                                               "OO   ",
                                               "OOO  ",
                                               "OOOO ",
                                               "OOOOO",
                                               "O    ",
                                               " O   ",
                                               "  O  ",
                                               "   O ",
                                               "    O",
                                               "  O O",
                                               " OO O",
                                               " OO  ",
                                               "OOO  ",
                                               "OO   ",
                                               "OOOO ",
                                               "OOOOO"  }};

llc::astatic<llc::s2_t[N], NUM_DATA> Input = {};
llc::astatic<llc::s2_t[M], NUM_DATA> Output = {};

FILE * f = {};
llc::u2_t RandomState = 4711;
llc::s2_t RandomNext() { RandomState = RandomState * 214013U + 2531011U; rtrn (RandomState >> 16) & 32767; }
~SState() { if(f) fclose(f); }


llc::err_t InitializeApplication(SNet* Net)
{
  llc::s2_t n,i,j;

  for (n=0; n<NUM_DATA; n++) {
    for (i=0; i<N; i++) {
      Input[n][i] = (Pattern[n][i] == 'O');
    }
  }
  Net->A1  = 1;
  Net->B1  = 1.5;
  Net->C1  = 5;
  Net->D1  = 0.9;
  Net->L   = 3;
  Net->Rho = 0.9;
  for (i=0; i<Net->F1->Units; i++) {
    for (j=0; j<Net->F2->Units; j++) {
      Net->F1->Weight[i][j] = (Net->B1 - 1) / Net->D1 + 0.2;
      Net->F2->Weight[j][i] = Net->L / (Net->L - 1 + N) - 0.1;
    }
  }
  if_true_fe(fopen_s(&f, "ART1.txt", "w"));
  rtrn 0;
}


void WriteInput(SNet* /*Net*/, llc::view<llc::s2_t> argInput)
{
  llc::s2_t i;
   
  for (i=0; i<N; i++) {
    fprintf(f, "%c", (argInput[i]) ? 'O' : ' ');
  }
  fprintf(f, " -> ");
}


void WriteOutput(SNet* Net, llc::view<llc::s2_t> /*Output*/)
{
  if (Net->Winner != NO_WINNER)
    fprintf(f, "Class %i\n", Net->Winner);
  else
    fprintf(f, "new Input and all Classes exhausted\n");
}





/******************************************************************************
                          I N I T I A L I Z A T I O N
 ******************************************************************************/


llc::err_t GenerateNetwork(SNet* Net)
{

  if_null_fe(Net->F1.create());
  if_null_fe(Net->F2.create());
      
  Net->F1->Units     = N;
  if_fail_fe(Net->F1->Output.resize(N, 0));
  if_fail_fe(Net->F1->Weight.resize(llc::n2u2_t{(llc::u2_t)(M), (llc::u2_t)(N)}, 0));

  Net->F2->Units     = M;
  if_fail_fe(Net->F2->Output.resize(M, 0));
  if_fail_fe(Net->F2->Weight.resize(llc::n2u2_t{(llc::u2_t)(N), (llc::u2_t)(M)}, 0));
  if_fail_fe(Net->F2->Inhibited.resize(M, 0));
  rtrn 0;
}


void SetInput(SNet* Net, llc::view<llc::s2_t> argInput)
{
  llc::s2_t  i;
  llc::f3_t Activation;
   
  for (i=0; i<Net->F1->Units; i++) {
    Activation = argInput[i] / (1 + Net->A1 * (argInput[i] + Net->B1) + Net->C1);
    Net->F1->Output[i] = (Activation > 0);
  }
}


void GetOutput(SNet* Net, llc::view<llc::s2_t> argOutput)
{
  llc::s2_t i;
   
  for (i=0; i<Net->F2->Units; i++) {
    argOutput[i] = Net->F2->Output[i];
  }
}


/******************************************************************************
                     P R O P A G A T I N G   S I G N A L S
 ******************************************************************************/


void PropagateToF2(SNet* Net)
{
  llc::s2_t  i,j;
  llc::f3_t Sum, MaxOut;
   
  MaxOut = MIN_REAL;
  Net->Winner = NO_WINNER;
  for (i=0; i<Net->F2->Units; i++) {
    if (! Net->F2->Inhibited[i]) {
      Sum = 0;
      for (j=0; j<Net->F1->Units; j++) {
        Sum += Net->F2->Weight[i][j] * Net->F1->Output[j];
      }
      if (Sum > MaxOut) {
        MaxOut = Sum;
        Net->Winner = i;
      }
    }
    Net->F2->Output[i] = false;
  }
  if (Net->Winner != NO_WINNER)
    Net->F2->Output[Net->Winner] = true;
}


void PropagateToF1(SNet* Net, llc::view<llc::s2_t> argInput)
{
  llc::s2_t  i;
  llc::f3_t Sum, Activation;
   
  for (i=0; i<Net->F1->Units; i++) {
    Sum = Net->F1->Weight[i][Net->Winner] * Net->F2->Output[Net->Winner];
    Activation = (argInput[i] + Net->D1 * Sum - Net->B1) /
                 (1 + Net->A1 * (argInput[i] + Net->D1 * Sum) + Net->C1);
    Net->F1->Output[i] = (Activation > 0);
  }
}


/******************************************************************************
                        A D J U S T I N G   W E I G H T S
 ******************************************************************************/


llc::f3_t Magnitude(SNet* Net, llc::view<llc::s2_t> argInput)
{
  llc::s2_t  i;
  llc::f3_t Magnitude;

  Magnitude = 0;
  for (i=0; i<Net->F1->Units; i++) {
    Magnitude += argInput[i];
  }
  return Magnitude;
}


void AdjustWeights(SNet* Net)
{
  llc::s2_t  i;
  llc::f3_t MagnitudeInput_;

  for (i=0; i<Net->F1->Units; i++) {
    if (Net->F1->Output[i]) {
      MagnitudeInput_ = Magnitude(Net, Net->F1->Output);
      Net->F1->Weight[i][Net->Winner] = 1;
      Net->F2->Weight[Net->Winner][i] = Net->L / (Net->L - 1 + MagnitudeInput_);
    }
    else {
      Net->F1->Weight[i][Net->Winner] = 0;
      Net->F2->Weight[Net->Winner][i] = 0;
    }
  }
}


/******************************************************************************
                      S I M U L A T I N G   T H E   N E T
 ******************************************************************************/





/******************************************************************************
                                    M A I N
 ******************************************************************************/




SNet Network = {};
void Snapshot(llc::SNN & run) { llc::nnWeights(run, Network.F2->Weight); }
llc::s2_t Cursor = 0;
bool Started = false;
llc::err_t Begin() { if_fail_fe(GenerateNetwork(&Network)); rtrn InitializeApplication(&Network); }
llc::err_t Step(llc::SNN & run) {
 if(!Started) {
  WriteInput(&Network, Input[Cursor]);
  for(llc::s2_t i = 0; i < Network.F2->Units; ++i) Network.F2->Inhibited[i] = false;
  Started = true;
 }
 SetInput(&Network, Input[Cursor]); PropagateToF2(&Network); GetOutput(&Network, Output[Cursor]);
 bool resonance = false;
 const bool exhausted = Network.Winner == NO_WINNER;
 if(!exhausted) {
  PropagateToF1(&Network, Input[Cursor]);
  if(Magnitude(&Network, Network.F1->Output) / Magnitude(&Network, Input[Cursor]) < Network.Rho)
   Network.F2->Inhibited[Network.Winner] = true;
  else resonance = true;
 }
 if(resonance) AdjustWeights(&Network);
 run.Width = N; run.Count = N; run.Metric = Network.Winner;
 for(llc::u2_t i = 0; i < N; ++i) run.Values[i] = Network.F1->Output[i];
 if(resonance || exhausted) { WriteOutput(&Network, Output[Cursor]); ++Cursor; Started = false; }
 rtrn Cursor == NUM_DATA ? 1 : 0;
}

};
} // namespace
llc::err_t llc::nnCreateArt1(SNN & run) { rtrn nnCreate<SState>(run); }
