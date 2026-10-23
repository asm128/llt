/******************************************************************************

                      ===============
        Network:      Adaline Network
                      ===============

        Application:  Pattern Recognition
                      Classification of Digits 0-9

        Author:       Karsten Kutza
        Date:         15.4.96

        Reference:    B. Widrow, M.E. Hoff
                      Adaptive Switching Circuits
                      1960 IRE WESCON Convention Record, IRE, New York, NY,
                      pp. 96-104, 1960

 ******************************************************************************/




#include "llc_nn_internal.h"
#include <cmath>

namespace {
stct SState {
/******************************************************************************
                            D E C L A R A T I O N S
 ******************************************************************************/






#define MIN(x,y)      ((x)<(y) ? (x) : (y))
#define MAX(x,y)      ((x)>(y) ? (x) : (y))

#define LO            -1
#define HI            +1
#define BIAS           1

#define sqr(x)        ((x)*(x))


stct SLayer {                     /* A SLayer OF A SNet:                     */
        llc::s2_t           Units;         /* - number of units in this layer       */
        llc::apod<llc::f3_t> Activation = {};    /* - activation of ith unit              */
        llc::apod<llc::s2_t> Output = {};        /* - output of ith unit                  */
        llc::apod<llc::f3_t> Error = {};         /* - error term of ith unit              */
        llc::img<llc::f3_t> Weight = {};        /* - connection weights to ith unit      */
};

stct SNet {                     /* A SNet:                                */
        llc::pobj<SLayer> InputLayer = {};    /* - input layer                         */
        llc::pobj<SLayer> OutputLayer = {};   /* - output layer                        */
        llc::f3_t          Eta;           /* - learning rate                       */
        llc::f3_t          Error;         /* - total net error                     */
        llc::f3_t          Epsilon;       /* - net error to terminate training     */
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


#define NUM_DATA      10
#define X             5
#define Y             7

#define N             (X * Y)
#define M             10

llc::astatic<llc::sc_t[Y][X+1], NUM_DATA> Pattern =  {{ { " OOO ",
                                                    "O   O",
                                                    "O   O",
                                                    "O   O",
                                                    "O   O",
                                                    "O   O",
                                                    " OOO "  },

                                                  { "  O  ",
                                                    " OO  ",
                                                    "O O  ",
                                                    "  O  ",
                                                    "  O  ",
                                                    "  O  ",
                                                    "  O  "  },

                                                  { " OOO ",
                                                    "O   O",
                                                    "    O",
                                                    "   O ",
                                                    "  O  ",
                                                    " O   ",
                                                    "OOOOO"  },

                                                  { " OOO ",
                                                    "O   O",
                                                    "    O",
                                                    " OOO ",
                                                    "    O",
                                                    "O   O",
                                                    " OOO "  },

                                                  { "   O ",
                                                    "  OO ",
                                                    " O O ",
                                                    "O  O ",
                                                    "OOOOO",
                                                    "   O ",
                                                    "   O "  },

                                                  { "OOOOO",
                                                    "O    ",
                                                    "O    ",
                                                    "OOOO ",
                                                    "    O",
                                                    "O   O",
                                                    " OOO "  },

                                                  { " OOO ",
                                                    "O   O",
                                                    "O    ",
                                                    "OOOO ",
                                                    "O   O",
                                                    "O   O",
                                                    " OOO "  },

                                                  { "OOOOO",
                                                    "    O",
                                                    "    O",
                                                    "   O ",
                                                    "  O  ",
                                                    " O   ",
                                                    "O    "  },

                                                  { " OOO ",
                                                    "O   O",
                                                    "O   O",
                                                    " OOO ",
                                                    "O   O",
                                                    "O   O",
                                                    " OOO "  },

                                                  { " OOO ",
                                                    "O   O",
                                                    "O   O",
                                                    " OOOO",
                                                    "    O",
                                                    "O   O",
                                                    " OOO "  } }};

llc::astatic<llc::s2_t[N], NUM_DATA> Input = {};
llc::astatic<llc::s2_t[M], NUM_DATA> Output = 
                      
                                  {{ {HI, LO, LO, LO, LO, LO, LO, LO, LO, LO},
                                    {LO, HI, LO, LO, LO, LO, LO, LO, LO, LO},
                                    {LO, LO, HI, LO, LO, LO, LO, LO, LO, LO},
                                    {LO, LO, LO, HI, LO, LO, LO, LO, LO, LO},
                                    {LO, LO, LO, LO, HI, LO, LO, LO, LO, LO},
                                    {LO, LO, LO, LO, LO, HI, LO, LO, LO, LO},
                                    {LO, LO, LO, LO, LO, LO, HI, LO, LO, LO},
                                    {LO, LO, LO, LO, LO, LO, LO, HI, LO, LO},
                                    {LO, LO, LO, LO, LO, LO, LO, LO, HI, LO},
                                    {LO, LO, LO, LO, LO, LO, LO, LO, LO, HI}  }};

FILE * f = {};
llc::u2_t RandomState = 4711;
llc::s2_t RandomNext() { RandomState = RandomState * 214013U + 2531011U; rtrn (RandomState >> 16) & 32767; }
~SState() { if(f) fclose(f); }


llc::err_t InitializeApplication(SNet* Net)
{
  llc::s2_t n,i,j;

  Net->Eta     = 0.001;
  Net->Epsilon = 0.0001;

  for (n=0; n<NUM_DATA; n++) {
    for (i=0; i<Y; i++) {
      for (j=0; j<X; j++) {
        Input[n][i*X+j] = (Pattern[n][i][j] == 'O') ? HI : LO;
      }
    }
  }
  if_true_fe(fopen_s(&f, "ADALINE.txt", "w"));
  rtrn 0;
}


void WriteInput(SNet* /*Net*/, llc::view<llc::s2_t> argInput)
{
  llc::s2_t i;
   
  for (i=0; i<N; i++) {
    if (i%X == 0) {
      fprintf(f, "\n");
    }
    fprintf(f, "%c", (argInput[i] == HI) ? 'O' : ' ');
  }
  fprintf(f, " -> ");
}


void WriteOutput(SNet* /*Net*/, llc::view<llc::s2_t> argOutput)
{
  llc::s2_t i;
  llc::s2_t Count, Index = 0;
   
  Count = 0;
  for (i=0; i<M; i++) {
    if (argOutput[i] == HI) {
      Count++;
      Index = i;
    }
  }
  if (Count == 1)
    fprintf(f, "%i\n", Index);
  else
    fprintf(f, "%s\n", "invalid");
}





/******************************************************************************
                          I N I T I A L I Z A T I O N
 ******************************************************************************/


llc::err_t GenerateNetwork(SNet* Net)
{

  if_null_fe(Net->InputLayer.create());
  if_null_fe(Net->OutputLayer.create());

  Net->InputLayer->Units       = N;
  if_fail_fe(Net->InputLayer->Output.resize(N+1, 0));
  Net->InputLayer->Output[0]   = BIAS;

  Net->OutputLayer->Units      = M;
  if_fail_fe(Net->OutputLayer->Activation.resize(M+1, 0));
  if_fail_fe(Net->OutputLayer->Output.resize(M+1, 0));
  if_fail_fe(Net->OutputLayer->Error.resize(M+1, 0));
  if_fail_fe(Net->OutputLayer->Weight.resize(llc::n2u2_t{(llc::u2_t)(N+1), (llc::u2_t)(M+1)}, 0));

  Net->Eta     = 0.1;
  Net->Epsilon = 0.01;
  rtrn 0;
}


void RandomWeights(SNet* Net)
{
  llc::s2_t i,j;
   
  for (i=1; i<=Net->OutputLayer->Units; i++) {
    for (j=0; j<=Net->InputLayer->Units; j++) {
      Net->OutputLayer->Weight[i][j] = RandomEqualREAL(-0.5, 0.5);
    }
  }
}


void SetInput(SNet* Net, llc::view<llc::s2_t> argInput, llc::s2_t Protocoling)
{
  llc::s2_t i;
   
  for (i=1; i<=Net->InputLayer->Units; i++) {
    Net->InputLayer->Output[i] = argInput[i-1];
  }
  if (Protocoling) {
    WriteInput(Net, argInput);
  }
}


void GetOutput(SNet* Net, llc::view<llc::s2_t> argOutput, llc::s2_t Protocoling)
{
  llc::s2_t i;
   
  for (i=1; i<=Net->OutputLayer->Units; i++) {
    argOutput[i-1] = Net->OutputLayer->Output[i];
  }
  if (Protocoling) {
    WriteOutput(Net, argOutput);
  }
}


/******************************************************************************
                     P R O P A G A T I N G   S I G N A L S
 ******************************************************************************/


void PropagateNet(SNet* Net)
{
  llc::s2_t  i,j;
  llc::f3_t Sum;

  for (i=1; i<=Net->OutputLayer->Units; i++) {
    Sum = 0;
    for (j=0; j<=Net->InputLayer->Units; j++) {
      Sum += Net->OutputLayer->Weight[i][j] * Net->InputLayer->Output[j];
    }
    Net->OutputLayer->Activation[i] = Sum;
    if (Sum >= 0)
      Net->OutputLayer->Output[i] = HI;
    else
      Net->OutputLayer->Output[i] = LO;
  }
}


/******************************************************************************
                       A D J U S T I N G   W E I G H T S
 ******************************************************************************/


void ComputeOutputError(SNet* Net, llc::view<llc::s2_t> Target)
{
  llc::s2_t  i;
  llc::f3_t Err;
   
  Net->Error = 0;
  for (i=1; i<=Net->OutputLayer->Units; i++) {
    Err = Target[i-1] - Net->OutputLayer->Activation[i];
    Net->OutputLayer->Error[i] = Err;
    Net->Error += 0.5 * sqr(Err);
  }
}


void AdjustWeights(SNet* Net)
{
  llc::s2_t  i,j;
  llc::s2_t  Out;
  llc::f3_t Err;
   
  for (i=1; i<=Net->OutputLayer->Units; i++) {
    for (j=0; j<=Net->InputLayer->Units; j++) {
      Out = Net->InputLayer->Output[j];
      Err = Net->OutputLayer->Error[i];
      Net->OutputLayer->Weight[i][j] += Net->Eta * Err * Out;
    }
  }
}


/******************************************************************************
                      S I M U L A T I N G   T H E   N E T
 ******************************************************************************/


void SimulateNet(SNet* Net, llc::view<llc::s2_t> argInput, llc::view<llc::s2_t> Target, llc::s2_t Training, llc::s2_t Protocoling)
{
  llc::astatic<llc::s2_t, M> resultOutput = {};
   
  SetInput(Net, argInput, Protocoling);
  PropagateNet(Net);
  GetOutput(Net, resultOutput, Protocoling);
   
  ComputeOutputError(Net, Target);
  if (Training)
    AdjustWeights(Net);
}


/******************************************************************************
                                    M A I N
 ******************************************************************************/




SNet Network = {};
void Snapshot(llc::SNN & run) { llc::nnWeights(run, Network.OutputLayer->Weight); }
llc::s2_t Phase = 0, Cursor = 0;
llc::f3_t MaximumError = 0;
bool Converged = true;
llc::err_t Begin() {
 InitializeRandoms(); if_fail_fe(GenerateNetwork(&Network)); RandomWeights(&Network);
 rtrn InitializeApplication(&Network);
}
llc::err_t Step(llc::SNN & run) {
 if(Phase == 0) {
  SimulateNet(&Network, Input[Cursor], Output[Cursor], false, false);
  MaximumError = llc::max(MaximumError, Network.Error);
  Converged = Converged && Network.Error < Network.Epsilon;
  if(++Cursor == NUM_DATA) { run.Metric = MaximumError; Cursor = 0; Phase = Converged ? 2 : 1; }
 } else if(Phase == 1) {
  const auto n = RandomEqualINT(0, NUM_DATA - 1);
  SimulateNet(&Network, Input[n], Output[n], true, false);
  if(++Cursor == 10 * NUM_DATA) { Cursor = 0; Phase = 0; MaximumError = 0; Converged = true; }
 } else {
  SimulateNet(&Network, Input[Cursor], Output[Cursor], false, true);
  ++Cursor;
 }
 run.Width = X; run.Count = N;
 for(llc::u2_t i = 0; i < N; ++i) run.Values[i] = Network.InputLayer->Output[i+1];
 rtrn Phase == 2 && Cursor == NUM_DATA ? 1 : 0;
}

};
} // namespace
llc::err_t llc::nnCreateAdaline(SNN & run) { rtrn nnCreate<SState>(run); }
