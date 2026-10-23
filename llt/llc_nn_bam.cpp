/******************************************************************************

                      ================================
        Network:      Bidirectional Associative Memory
                      ================================

        Application:  Heteroassociative Memory
                      Association of Names and Phone Numbers

        Author:       Karsten Kutza
        Date:         24.1.96

        Reference:    B. Kosko
                      Bidirectional Associative Memories
                      IEEE Transactions on Systems, Man, and Cybernetics, 18,
                      pp. 49-60, 1988

 ******************************************************************************/




#include "llc_nn_internal.h"
#include <cmath>

namespace {
stct SState {
/******************************************************************************
                            D E C L A R A T I O N S
 ******************************************************************************/






#define LO            -1
#define HI            +1

#define BINARY(x)     ((x)==LO ? false : true)
#define BIPOLAR(x)    ((x)==false ? LO : HI)


stct SLayer {                     /* A SLayer OF A SNet:                     */
        llc::s2_t           Units;         /* - number of units in this layer       */
        llc::apod<llc::s2_t> Output = {};        /* - output of ith unit                  */
        llc::img<llc::s2_t> Weight = {};        /* - connection weights to ith unit      */
};

stct SNet {                     /* A SNet:                                */
        llc::pobj<SLayer> X = {};             /* - X layer                             */
        llc::pobj<SLayer> Y = {};             /* - Y layer                             */
};


/******************************************************************************
        R A N D O M S   D R A W N   F R O M   D I S T R I B U T I O N S
 ******************************************************************************/


void InitializeRandoms()
{
  RandomState = 4711;
}


llc::s2_t RandomEqualBOOL()
{
  return RandomNext() % 2;
}      


/******************************************************************************
               A P P L I C A T I O N - S P E C I F I C   C O D E
 ******************************************************************************/


#define NUM_DATA      3
#define IN_CHARS      5
#define OUT_CHARS     7

#define BITS_PER_CHAR 6
#define FIRST_CHAR    ' '

#define N             (IN_CHARS  * BITS_PER_CHAR)
#define M             (OUT_CHARS * BITS_PER_CHAR)

llc::astatic<llc::sc_t[IN_CHARS+1], NUM_DATA> Names =  {{ "TINA ",
                                                      "ANTJE", 
                                                      "LISA "  }};
                                     
llc::astatic<llc::sc_t[IN_CHARS+1], NUM_DATA> Names_ =  {{ "TINE ",
                                                      "ANNJE", 
                                                      "RITA "  }};

llc::astatic<llc::sc_t[OUT_CHARS+1], NUM_DATA> Phones =  {{ "6843726",
                                                      "8034673",
                                                      "7260915"  }};

llc::astatic<llc::s2_t[N], NUM_DATA> Input = {};
llc::astatic<llc::s2_t[N], NUM_DATA> Input_ = {};
llc::astatic<llc::s2_t[M], NUM_DATA> Output = {};

FILE * f = {};
llc::u2_t RandomState = 4711;
llc::s2_t RandomNext() { RandomState = RandomState * 214013U + 2531011U; rtrn (RandomState >> 16) & 32767; }
~SState() { if(f) fclose(f); }


llc::err_t InitializeApplication(SNet* /*Net*/)
{
  llc::s2_t n,i,j,a,a_;

  for (n=0; n<NUM_DATA; n++) {
    for (i=0; i<IN_CHARS; i++) {
      a  = Names [n][i] - FIRST_CHAR;
      a_ = Names_[n][i] - FIRST_CHAR;
      for (j=0; j<BITS_PER_CHAR; j++) {
        Input [n][i*BITS_PER_CHAR+j] = BIPOLAR(a  % 2);
        Input_[n][i*BITS_PER_CHAR+j] = BIPOLAR(a_ % 2);
        a  /= 2;
        a_ /= 2;
      }
    }
    for (i=0; i<OUT_CHARS; i++) {
      a = Phones[n][i] - FIRST_CHAR;
      for (j=0; j<BITS_PER_CHAR; j++) {
        Output[n][i*BITS_PER_CHAR+j] = BIPOLAR(a % 2);
        a /= 2;
      }
    }
  }
  if_true_fe(fopen_s(&f, "BAM.txt", "w"));
  rtrn 0;
}


void WriteLayer(SLayer* Layer)
{
  llc::s2_t i,j,a,p;
   
  for (i=0; i<(Layer->Units / BITS_PER_CHAR); i++) {
    a = 0;
    p = 1;
    for (j=0; j<BITS_PER_CHAR; j++) {
      a += BINARY(Layer->Output[i*BITS_PER_CHAR+j]) * p;
      p *= 2;  
    }
    fprintf(f, "%c", a + FIRST_CHAR);
  }
}





/******************************************************************************
                          I N I T I A L I Z A T I O N
 ******************************************************************************/


llc::err_t GenerateNetwork(SNet* Net)
{

  if_null_fe(Net->X.create());
  if_null_fe(Net->Y.create());
      
  Net->X->Units  = N;
  if_fail_fe(Net->X->Output.resize(N, 0));
  if_fail_fe(Net->X->Weight.resize(llc::n2u2_t{(llc::u2_t)(M), (llc::u2_t)(N)}, 0));

  Net->Y->Units  = M;
  if_fail_fe(Net->Y->Output.resize(M, 0));
  if_fail_fe(Net->Y->Weight.resize(llc::n2u2_t{(llc::u2_t)(N), (llc::u2_t)(M)}, 0));
  rtrn 0;
}


void CalculateWeights(SNet* Net)
{
  llc::s2_t i,j,n;
  llc::s2_t Weight;

  for (i=0; i<Net->X->Units; i++) {
    for (j=0; j<Net->Y->Units; j++) {
      Weight = 0;
      for (n=0; n<NUM_DATA; n++) {
        Weight += Input[n][i] * Output[n][j];
      }
      Net->X->Weight[i][j] = Weight;
      Net->Y->Weight[j][i] = Weight;
    }
  }
}


void SetInput(SLayer* Layer, llc::view<llc::s2_t> argInput)
{
  llc::s2_t i;
   
  for (i=0; i<Layer->Units; i++) {
    Layer->Output[i] = argInput[i];
  }
  WriteLayer(Layer);
}


void SetRandom(SLayer* Layer)
{
  llc::s2_t i;
   
  for (i=0; i<Layer->Units; i++) {
    Layer->Output[i] = BIPOLAR(RandomEqualBOOL());
  }
  WriteLayer(Layer);
}


void GetOutput(SLayer* Layer, llc::view<llc::s2_t> argOutput)
{
  llc::s2_t i;
   
  for (i=0; i<Layer->Units; i++) {
    argOutput[i] = Layer->Output[i];
  }
  WriteLayer(Layer);
}


/******************************************************************************
                     P R O P A G A T I N G   S I G N A L S
 ******************************************************************************/


llc::s2_t PropagateLayer(SLayer* From, SLayer* To)
{
  llc::s2_t  i,j;
  llc::s2_t  Sum, Out = 0;
  llc::s2_t Stable;

  Stable = true;
  for (i=0; i<To->Units; i++) {
    Sum = 0;
    for (j=0; j<From->Units; j++) {
      Sum += To->Weight[i][j] * From->Output[j];
    }
    if (Sum != 0) {
      if (Sum < 0) Out = LO;
      if (Sum > 0) Out = HI;
      if (Out != To->Output[i]) {
        Stable = false;
        To->Output[i] = Out;
      }
    }
  }
  return Stable;
}





/******************************************************************************
                      S I M U L A T I N G   T H E   N E T
 ******************************************************************************/





/******************************************************************************
                                    M A I N
 ******************************************************************************/




SNet Network = {};
void Snapshot(llc::SNN & run) { llc::nnWeights(run, Network.Y->Weight); }
llc::s2_t Cursor = 0;
bool Started = false;
llc::astatic<llc::s2_t, N> ResultInput = {};
llc::astatic<llc::s2_t, M> ResultOutput = {};
llc::err_t Begin() { InitializeRandoms(); if_fail_fe(GenerateNetwork(&Network)); if_fail_fe(InitializeApplication(&Network)); CalculateWeights(&Network); rtrn 0; }
llc::err_t Step(llc::SNN & run) {
 const bool reverse = Cursor >= NUM_DATA && Cursor < 2 * NUM_DATA;
 SLayer * from = reverse ? Network.Y.operator->() : Network.X.operator->();
 SLayer * to = reverse ? Network.X.operator->() : Network.Y.operator->();
 if(!Started) {
  if(reverse) SetInput(from, Output[Cursor % NUM_DATA]);
  else SetInput(from, Cursor < NUM_DATA ? Input[Cursor] : Input_[Cursor % NUM_DATA]);
  fprintf(f, " -> "); SetRandom(to); fprintf(f, "  |  "); Started = true;
 }
 const bool stable1 = PropagateLayer(from, to);
 const bool stable2 = PropagateLayer(to, from);
 run.Width = BITS_PER_CHAR; run.Count = M; run.Metric = Cursor;
 for(llc::u2_t i = 0; i < M; ++i) run.Values[i] = Network.Y->Output[i];
 if(stable1 && stable2) {
  if(reverse) GetOutput(from, ResultOutput); else GetOutput(from, ResultInput);
  fprintf(f, " -> ");
  if(reverse) GetOutput(to, ResultInput); else GetOutput(to, ResultOutput);
  fprintf(f, "\n\n"); ++Cursor; Started = false;
 }
 rtrn Cursor == 3 * NUM_DATA ? 1 : 0;
}

};
} // namespace
llc::err_t llc::nnCreateBam(SNN & run) { rtrn nnCreate<SState>(run); }
