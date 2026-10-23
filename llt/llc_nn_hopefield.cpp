/******************************************************************************

                      ==============
        Network:      Hopfield Model
                      ==============

        Application:  Autoassociative Memory
                      Associative Recall of Images

        Author:       Karsten Kutza
        Date:         30.1.96

        Reference:    J.J. Hopfield
                      Neural Networks and Physical Systems
                      with Emergent Collective Computational Abilities
                      Proceedings of the National Academy of Sciences, 79,
                      pp. 2554-2558, 1982

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


stct SNet {                     /* A SNet:                                */
        llc::s2_t           Units;         /* - number of units in this net         */
        llc::apod<llc::s2_t> Output = {};        /* - output of ith unit                  */
        llc::apod<llc::s2_t> Threshold = {};     /* - threshold of ith unit               */
        llc::img<llc::s2_t> Weight = {};        /* - connection weights to ith unit      */
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


#define NUM_DATA      5
#define X             10
#define Y             10

#define N             (X * Y)

llc::astatic<llc::sc_t[Y][X+1], NUM_DATA> Pattern =  {{ { "O O O O O ",
                                                     " O O O O O",
                                                     "O O O O O ",
                                                     " O O O O O",
                                                     "O O O O O ",
                                                     " O O O O O",
                                                     "O O O O O ",
                                                     " O O O O O",
                                                     "O O O O O ",
                                                     " O O O O O"  },

                                                   { "OO  OO  OO",
                                                     "OO  OO  OO",
                                                     "  OO  OO  ",
                                                     "  OO  OO  ",
                                                     "OO  OO  OO",
                                                     "OO  OO  OO",
                                                     "  OO  OO  ",
                                                     "  OO  OO  ",
                                                     "OO  OO  OO",
                                                     "OO  OO  OO"  },

                                                   { "OOOOO     ",
                                                     "OOOOO     ",
                                                     "OOOOO     ",
                                                     "OOOOO     ",
                                                     "OOOOO     ",
                                                     "     OOOOO",
                                                     "     OOOOO",
                                                     "     OOOOO",
                                                     "     OOOOO",
                                                     "     OOOOO"  },

                                                   { "O  O  O  O",
                                                     " O  O  O  ",
                                                     "  O  O  O ",
                                                     "O  O  O  O",
                                                     " O  O  O  ",
                                                     "  O  O  O ",
                                                     "O  O  O  O",
                                                     " O  O  O  ",
                                                     "  O  O  O ",
                                                     "O  O  O  O"  },

                                                   { "OOOOOOOOOO",
                                                     "O        O",
                                                     "O OOOOOO O",
                                                     "O O    O O",
                                                     "O O OO O O",
                                                     "O O OO O O",
                                                     "O O    O O",
                                                     "O OOOOOO O",
                                                     "O        O",
                                                     "OOOOOOOOOO"  } }};

llc::astatic<llc::sc_t[Y][X+1], NUM_DATA> Pattern_ =  {{ { "          ",
                                                     "          ",
                                                     "          ",
                                                     "          ",
                                                     "          ",
                                                     " O O O O O",
                                                     "O O O O O ",
                                                     " O O O O O",
                                                     "O O O O O ",
                                                     " O O O O O"  },

                                                   { "OOO O    O",
                                                     " O  OOO OO",
                                                     "  O O OO O",
                                                     " OOO   O  ",
                                                     "OO  O  OOO",
                                                     " O OOO   O",
                                                     "O OO  O  O",
                                                     "   O OOO  ",
                                                     "OO OOO  O ",
                                                     " O  O  OOO"  },

                                                   { "OOOOO     ",
                                                     "O   O OOO ",
                                                     "O   O OOO ",
                                                     "O   O OOO ",
                                                     "OOOOO     ",
                                                     "     OOOOO",
                                                     " OOO O   O",
                                                     " OOO O   O",
                                                     " OOO O   O",
                                                     "     OOOOO"  },

                                                   { "O  OOOO  O",
                                                     "OO  OOOO  ",
                                                     "OOO  OOOO ",
                                                     "OOOO  OOOO",
                                                     " OOOO  OOO",
                                                     "  OOOO  OO",
                                                     "O  OOOO  O",
                                                     "OO  OOOO  ",
                                                     "OOO  OOOO ",
                                                     "OOOO  OOOO"  },

                                                   { "OOOOOOOOOO",
                                                     "O        O",
                                                     "O        O",
                                                     "O        O",
                                                     "O   OO   O",
                                                     "O   OO   O",
                                                     "O        O",
                                                     "O        O",
                                                     "O        O",
                                                     "OOOOOOOOOO"  } }};

llc::astatic<llc::s2_t[N], NUM_DATA> Input = {};
llc::astatic<llc::s2_t[N], NUM_DATA> Input_ = {};

FILE * f = {};
llc::u2_t RandomState = 4711;
llc::s2_t RandomNext() { RandomState = RandomState * 214013U + 2531011U; rtrn (RandomState >> 16) & 32767; }
~SState() { if(f) fclose(f); }


llc::err_t InitializeApplication(SNet* /*Net*/)
{
  llc::s2_t n,i,j;

  for (n=0; n<NUM_DATA; n++) {
    for (i=0; i<Y; i++) {
      for (j=0; j<X; j++) {
        Input [n][i*X+j] = BIPOLAR(Pattern [n][i][j] == 'O');
        Input_[n][i*X+j] = BIPOLAR(Pattern_[n][i][j] == 'O');
      }
    }
  }
  if_true_fe(fopen_s(&f, "HOPFIELD.txt", "w"));
  rtrn 0;
}


void WriteNet(SNet* Net)
{
  llc::s2_t i,j;
   
  for (i=0; i<Y; i++) {
    for (j=0; j<X; j++) {
      fprintf(f, "%c", BINARY(Net->Output[i*X+j]) ? 'O' : ' ');
    }
    fprintf(f, "\n");
  }
  fprintf(f, "\n");
}





/******************************************************************************
                          I N I T I A L I Z A T I O N
 ******************************************************************************/


llc::err_t GenerateNetwork(SNet* Net)
{
  llc::s2_t i;

  Net->Units     = N;
  if_fail_fe(Net->Output.resize(N, 0));
  if_fail_fe(Net->Threshold.resize(N, 0));
  if_fail_fe(Net->Weight.resize(llc::n2u2_t{(llc::u2_t)(N), (llc::u2_t)(N)}, 0));

  for (i=0; i<N; i++) {
    Net->Threshold[i] = 0;
    
  }
  rtrn 0;
}


void CalculateWeights(SNet* Net)
{
  llc::s2_t i,j,n;
  llc::s2_t Weight;

  for (i=0; i<Net->Units; i++) {
    for (j=0; j<Net->Units; j++) {
      Weight = 0;
      if (i!=j) {
        for (n=0; n<NUM_DATA; n++) {
          Weight += Input[n][i] * Input[n][j];
        }
      }
      Net->Weight[i][j] = Weight;
    }
  }
}


void SetInput(SNet* Net, llc::view<llc::s2_t> argInput)
{
  llc::s2_t i;
   
  for (i=0; i<Net->Units; i++) {
    Net->Output[i] = argInput[i];
  }
  WriteNet(Net);
}


void GetOutput(SNet* Net, llc::view<llc::s2_t> argOutput)
{
  llc::s2_t i;
   
  for (i=0; i<Net->Units; i++) {
    argOutput[i] = Net->Output[i];
  }
  WriteNet(Net);
}


/******************************************************************************
                     P R O P A G A T I N G   S I G N A L S
 ******************************************************************************/


llc::s2_t PropagateUnit(SNet* Net, llc::s2_t i)
{
  llc::s2_t  j;
  llc::s2_t  Sum, Out = 0;
  llc::s2_t Changed;

  Changed = false;
  Sum = 0;
  for (j=0; j<Net->Units; j++) {
    Sum += Net->Weight[i][j] * Net->Output[j];
  }
  if (Sum != Net->Threshold[i]) {
    if (Sum < Net->Threshold[i]) Out = LO;
    if (Sum > Net->Threshold[i]) Out = HI;
    if (Out != Net->Output[i]) {
      Changed = true;
      Net->Output[i] = Out;
    }
  }
  return Changed;
}





/******************************************************************************
                      S I M U L A T I N G   T H E   N E T
 ******************************************************************************/





/******************************************************************************
                                    M A I N
 ******************************************************************************/




SNet Network = {};
void Snapshot(llc::SNN & run) { llc::nnWeights(run, Network.Weight); }
llc::s2_t Cursor = 0;
llc::u2_t Unchanged = 0;
bool Started = false;
llc::astatic<llc::s2_t, N> Result = {};
llc::err_t Begin() {
 InitializeRandoms(); if_fail_fe(GenerateNetwork(&Network)); if_fail_fe(InitializeApplication(&Network)); CalculateWeights(&Network); rtrn 0;
}
llc::err_t Step(llc::SNN & run) {
 if(!Started) {
  SetInput(&Network, Cursor < NUM_DATA ? Input[Cursor] : Input_[Cursor - NUM_DATA]);
  Unchanged = 0; Started = true;
 }
 if(PropagateUnit(&Network, RandomEqualINT(0, Network.Units - 1))) Unchanged = 0; else ++Unchanged;
 run.Inspection.Width = X; run.Inspection.Count = N; run.Metric = Unchanged;
 for(llc::u2_t i = 0; i < N; ++i) run.Inspection.Values[i] = Network.Output[i];
 if(Unchanged == 10 * N) { GetOutput(&Network, Result); ++Cursor; Started = false; }
 rtrn Cursor == 2 * NUM_DATA ? 1 : 0;
}

};
} // namespace
llc::err_t llc::nnCreateHopfield(SNN & run) { rtrn nnCreate<SState>(run); }
