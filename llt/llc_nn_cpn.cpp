/******************************************************************************

                      ==========================
        Network:      Counterpropagation Network
                      ==========================

        Application:  Vision
                      Determination of the Angle of Rotation

        Author:       Karsten Kutza
        Date:         30.4.96

        Reference:    R. Hecht-Nielsen
                      Counterpropagation Networks
                      Proceedings of the IEEE International Conference on
                      Neural Networks, II, IEEE Press, New York, NY, pp. 19-32,
                      1987

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

#define LO            0
#define HI            1

#define PI            (2*asin(1))
#define sqr(x)        ((x)*(x))


stct SLayer {                     /* A SLayer OF A SNet:                     */
        llc::s2_t           Units;         /* - number of units in this layer       */
        llc::apod<llc::f3_t> Output = {};        /* - output of ith unit                  */
        llc::img<llc::f3_t> Weight = {};        /* - connection weights to ith unit      */
        llc::apod<llc::s2_t> Winner = {};        /* - marker for winning instar           */
};

stct SNet {                     /* A SNet:                                */
        llc::pobj<SLayer> InputLayer = {};    /* - input layer                         */
        llc::pobj<SLayer> InstarLayer = {};   /* - instar layer                        */
        llc::pobj<SLayer> OutstarLayer = {};  /* - outstar layer                       */
        llc::s2_t           Winners;       /* - allowed number of winning instars   */
        llc::f3_t          Alpha;         /* - hidden layer learning rate          */
        llc::f3_t          Beta;          /* - output layer learning rate          */
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


/******************************************************************************
               A P P L I C A T I O N - S P E C I F I C   C O D E
 ******************************************************************************/


#define NUM_DATA      8
#define X             11
#define Y             11

#define N             (X * Y)
#define C             NUM_DATA
#define M             2

llc::astatic<llc::sc_t[Y][X+1], NUM_DATA> Pattern =   {{ { "           ",
                                                     "           ",
                                                     "     O     ",
                                                     "     O     ",
                                                     "    OOO    ",
                                                     "    OOO    ",
                                                     "    OOO    ",
                                                     "   OOOOO   ",
                                                     "   OOOOO   ",
                                                     "           ",
                                                     "           "  },

                                                   { "           ",
                                                     "           ",
                                                     "        O  ",
                                                     "       O   ",
                                                     "     OOO   ",
                                                     "    OOO    ",
                                                     "   OOO     ",
                                                     " OOOOO     ",
                                                     "OOOOO      ",
                                                     "           ",
                                                     "           "  },

                                                   { "           ",
                                                     "           ",
                                                     "           ",
                                                     "  OO       ",
                                                     "  OOOOO    ",
                                                     "  OOOOOOO  ",
                                                     "  OOOOO    ",
                                                     "  OO       ",
                                                     "           ",
                                                     "           ",
                                                     "           "  },

                                                   { "           ",
                                                     "           ",
                                                     "OOOOO      ",
                                                     " OOOOO     ",
                                                     "   OOO     ",
                                                     "    OOO    ",
                                                     "     OOO   ",
                                                     "       O   ",
                                                     "        O  ",
                                                     "           ",
                                                     "           "  },

                                                   { "           ",
                                                     "           ",
                                                     "   OOOOO   ",
                                                     "   OOOOO   ",
                                                     "    OOO    ",
                                                     "    OOO    ",
                                                     "    OOO    ",
                                                     "     O     ",
                                                     "     O     ",
                                                     "           ",
                                                     "           "  },

                                                   { "           ",
                                                     "           ",
                                                     "      OOOOO",
                                                     "     OOOOO ",
                                                     "     OOO   ",
                                                     "    OOO    ",
                                                     "   OOO     ",
                                                     "   O       ",
                                                     "  O        ",
                                                     "           ",
                                                     "           "  },

                                                   { "           ",
                                                     "           ",
                                                     "           ",
                                                     "       OO  ",
                                                     "    OOOOO  ",
                                                     "  OOOOOOO  ",
                                                     "    OOOOO  ",
                                                     "       OO  ",
                                                     "           ",
                                                     "           ",
                                                     "           "  },

                                                   { "           ",
                                                     "           ",
                                                     "  O        ",
                                                     "   O       ",
                                                     "   OOO     ",
                                                     "    OOO    ",
                                                     "     OOO   ",
                                                     "     OOOOO ",
                                                     "      OOOOO",
                                                     "           ",
                                                     "           "  } }};

llc::astatic<llc::sc_t[Y][X+1], NUM_DATA> Pattern_ =  {{ { "           ",
                                                     "           ",
                                                     "     O     ",
                                                     "     O     ",
                                                     "     O     ",
                                                     "    OOO    ",
                                                     "    OOO    ",
                                                     "    OOO    ",
                                                     "   OOOOO   ",
                                                     "           ",
                                                     "           "  },

                                                   { "           ",
                                                     "           ",
                                                     "     O     ",
                                                     "     O     ",
                                                     "    O O    ",
                                                     "    O O    ",
                                                     "    O O    ",
                                                     "   O   O   ",
                                                     "   O   O   ",
                                                     "           ",
                                                     "           "  },

                                                   { "           ",
                                                     "           ",
                                                     "           ",
                                                     "     O     ",
                                                     "    OOO    ",
                                                     "    OOO    ",
                                                     "    OOO    ",
                                                     "   OOOOO   ",
                                                     "           ",
                                                     "           ",
                                                     "           "  },

                                                   { "           ",
                                                     "           ",
                                                     "           ",
                                                     "           ",
                                                     "     O     ",
                                                     "     O     ",
                                                     "     O     ",
                                                     "    OOO    ",
                                                     "           ",
                                                     "           ",
                                                     "           "  },

                                                   { "           ",
                                                     "  O        ",
                                                     "     O     ",
                                                     "     O     ",
                                                     "    OOO    ",
                                                     "    OO     ",
                                                     "    OOO   O",
                                                     "    OOOO   ",
                                                     "   OOOOO   ",
                                                     "           ",
                                                     "       O   "  },

                                                   { "           ",
                                                     "           ",
                                                     "     O     ",
                                                     "     O     ",
                                                     "    OOO    ",
                                                     "    OOO    ",
                                                     "    OOO    ",
                                                     "   OOOOO   ",
                                                     "   OOOOO   ",
                                                     "           ",
                                                     "           "  },

                                                   { "           ",
                                                     "           ",
                                                     "       O   ",
                                                     "      O    ",
                                                     "    OOO    ",
                                                     "    OOO    ",
                                                     "   OOO     ",
                                                     "  OOOOO    ",
                                                     " OOOOO     ",
                                                     "           ",
                                                     "           "  },

                                                   { "           ",
                                                     "           ",
                                                     "        O  ",
                                                     "       O   ",
                                                     "     OOO   ",
                                                     "    OOO    ",
                                                     "   OOO     ",
                                                     " OOOOO     ",
                                                     "OOOOO      ",
                                                     "           ",
                                                     "           "  } }};
                                                    
llc::astatic<llc::f3_t[N], NUM_DATA> Input = {};
llc::astatic<llc::f3_t[N], NUM_DATA> Input_ = {};
llc::astatic<llc::f3_t[M], NUM_DATA> Output = {};

FILE * f = {};
llc::u2_t RandomState = 4711;
llc::s2_t RandomNext() { RandomState = RandomState * 214013U + 2531011U; rtrn (RandomState >> 16) & 32767; }
~SState() { if(f) fclose(f); }


void NormalizeInput()
{
  llc::s2_t  n,i;
  llc::f3_t Length, Length_;

  for (n=0; n<NUM_DATA; n++) {
    Length  = 0;
    Length_ = 0;
    for (i=0; i<N; i++) {
      Length  += sqr(Input [n][i]);
      Length_ += sqr(Input_[n][i]);
    }
    Length  = sqrt(Length);
    Length_ = sqrt(Length_);
    for (i=0; i<N; i++) {
      Input [n][i] /= Length;
      Input_[n][i] /= Length_;
    }
  }
}


llc::err_t InitializeApplication(SNet* /*Net*/)
{
  llc::s2_t n,i,j;

  for (n=0; n<NUM_DATA; n++) {
    for (i=0; i<Y; i++) {
      for (j=0; j<X; j++) {
        Input [n][i*X+j] = (Pattern [n][i][j] == 'O') ? HI : LO;
        Input_[n][i*X+j] = (Pattern_[n][i][j] == 'O') ? HI : LO;
      }
    }
  }
  NormalizeInput();
  for (n=0; n<NUM_DATA; n++) {
    Output[n][0] = sin(n * 0.25 * PI);
    Output[n][1] = cos(n * 0.25 * PI);
  }
  if_true_fe(fopen_s(&f, "CPN.txt", "w"));
  rtrn 0;
}


void WriteInput(SNet* /*Net*/, llc::view<llc::f3_t> argInput)
{
  llc::s2_t i;
   
  for (i=0; i<N; i++) {
    if (i%X == 0) {
      fprintf(f, "\n");
    }
    fprintf(f, "%c", (argInput[i] != LO) ? 'O' : ' ');
  }
  fprintf(f, " -> ");
}


void WriteOutput(SNet* /*Net*/, llc::view<llc::f3_t> argOutput)
{
  llc::f3_t Angle;

  Angle = (atan2(argOutput[0], argOutput[1]) / PI) * 180;
  if (Angle < 0)
    Angle = Angle + 360;

  fprintf(f, "%0.0f\xB0\n", Angle);
}





/******************************************************************************
                          I N I T I A L I Z A T I O N
 ******************************************************************************/


llc::err_t GenerateNetwork(SNet* Net)
{

  if_null_fe(Net->InputLayer.create());
  if_null_fe(Net->InstarLayer.create());
  if_null_fe(Net->OutstarLayer.create());

  Net->InputLayer->Units    = N;
  if_fail_fe(Net->InputLayer->Output.resize(N, 0));
      
  Net->InstarLayer->Units   = C;
  if_fail_fe(Net->InstarLayer->Output.resize(C, 0));
  if_fail_fe(Net->InstarLayer->Weight.resize(llc::n2u2_t{(llc::u2_t)(N), (llc::u2_t)(C)}, 0));
  if_fail_fe(Net->InstarLayer->Winner.resize(C, 0));
      
  Net->OutstarLayer->Units  = M;
  if_fail_fe(Net->OutstarLayer->Output.resize(M, 0));
  if_fail_fe(Net->OutstarLayer->Weight.resize(llc::n2u2_t{(llc::u2_t)(C), (llc::u2_t)(M)}, 0));

  Net->Winners = 1;
  Net->Alpha   = 0.1;
  Net->Beta    = 0.1;
  rtrn 0;
}


void SetInput(SNet* Net, llc::view<llc::f3_t> argInput, llc::s2_t Protocoling)
{
  llc::s2_t i;
   
  for (i=0; i<Net->InputLayer->Units; i++) {
    Net->InputLayer->Output[i] = argInput[i];
  }
  if (Protocoling) {
    WriteInput(Net, argInput);
  }
}


void GetOutput(SNet* Net, llc::view<llc::f3_t> argOutput, llc::s2_t Protocoling)
{
  llc::s2_t i;
   
  for (i=0; i<Net->OutstarLayer->Units; i++) {
    argOutput[i] = Net->OutstarLayer->Output[i];
  }
  if (Protocoling) {
    WriteOutput(Net, argOutput);
  }
}


/******************************************************************************
                     P R O P A G A T I N G   S I G N A L S
 ******************************************************************************/


void PropagateToInstars(SNet* Net)
{
  llc::s2_t  w,i,j;
  llc::f3_t Sum, SumWinners, MaxOut;
  llc::s2_t  Winner = 0;

  for (i=0; i<Net->InstarLayer->Units; i++) {
    Sum = 0;
    for (j=0; j<Net->InputLayer->Units; j++) {
      Sum += Net->InstarLayer->Weight[i][j] * Net->InputLayer->Output[j];
    }
    Net->InstarLayer->Output[i] = Sum;
    Net->InstarLayer->Winner[i] = false;
  }
  SumWinners = 0;
  for (w=0; w<Net->Winners; w++) {
    MaxOut = MIN_REAL;
    for (i=0; i<Net->InstarLayer->Units; i++) {
      if (! Net->InstarLayer->Winner[i] && Net->InstarLayer->Output[i] > MaxOut)
        MaxOut = Net->InstarLayer->Output[Winner = i];
    }
    Net->InstarLayer->Winner[Winner] = true;
    SumWinners += Net->InstarLayer->Output[Winner];
  }
  for (i=0; i<Net->InstarLayer->Units; i++) {
    if (Net->InstarLayer->Winner[i])
      Net->InstarLayer->Output[i] = Net->InstarLayer->Output[i] / SumWinners;
    else
      Net->InstarLayer->Output[i] = 0;
  }
}


void PropagateToOutstars(SNet* Net)
{
  llc::s2_t  i,j;
  llc::f3_t Sum;

  for (i=0; i<Net->OutstarLayer->Units; i++) {
    Sum = 0;
    for (j=0; j<Net->InstarLayer->Units; j++) {  
      Sum += Net->OutstarLayer->Weight[i][j] * Net->InstarLayer->Output[j];
    }
    Net->OutstarLayer->Output[i] = Sum;
  }
}


void PropagateNet(SNet* Net)
{
  PropagateToInstars(Net);
  PropagateToOutstars(Net);
}


/******************************************************************************
                        T R A I N I N G   T H E   N E T
 ******************************************************************************/











/******************************************************************************
                      S I M U L A T I N G   T H E   N E T
 ******************************************************************************/


void SimulateNet(SNet* Net, llc::view<llc::f3_t> argInput)
{
  llc::astatic<llc::f3_t, M> resultOutput = {};

  SetInput(Net, argInput, true);
  PropagateNet(Net);
  GetOutput(Net, resultOutput, true);
}


/******************************************************************************
                                    M A I N
 ******************************************************************************/




SNet Network = {};
void Snapshot(llc::SNN & run) { llc::nnWeights(run, Network.InstarLayer->Weight); }
llc::s2_t Cursor = 0;
llc::err_t Begin() {
 InitializeRandoms(); if_fail_fe(GenerateNetwork(&Network)); if_fail_fe(InitializeApplication(&Network));
 // The original demonstration computes the weights directly (zero training epochs).
 for(llc::s2_t i = 0; i < C; ++i)
  for(llc::s2_t j = 0; j < N; ++j) Network.InstarLayer->Weight[i][j] = Input[i][j];
 for(llc::s2_t i = 0; i < M; ++i)
  for(llc::s2_t j = 0; j < C; ++j) Network.OutstarLayer->Weight[i][j] = Output[j][i];
 Network.Winners = 2; rtrn 0;
}
llc::err_t Step(llc::SNN & run) {
 SimulateNet(&Network, Cursor < NUM_DATA ? Input[Cursor] : Input_[Cursor - NUM_DATA]);
 run.Width = X; run.Count = N;
 run.Metric = atan2(Network.OutstarLayer->Output[0], Network.OutstarLayer->Output[1]) / PI * 180;
 if(run.Metric < 0) run.Metric += 360;
 for(llc::u2_t i = 0; i < N; ++i) run.Values[i] = Network.InputLayer->Output[i];
 rtrn ++Cursor == 2 * NUM_DATA ? 1 : 0;
}

};
} // namespace
llc::err_t llc::nnCreateCpn(SNN & run) { rtrn nnCreate<SState>(run); }
