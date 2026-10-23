/******************************************************************************

                      ==========================================
        Network:      Boltzmann Machine with Simulated Annealing
                      ==========================================

        Application:  Optimization
                      Traveling Salesman Problem

        Author:       Karsten Kutza
        Date:         21.2.96

        Reference:    D.H. Ackley, G.E. Hinton, T.J. Sejnowski
                      A Learning Algorithm for Boltzmann Machines
                      Cognitive Science, 9, pp. 147-169, 1985

 ******************************************************************************/




#include "llc_nn_internal.h"
#include <cmath>

namespace {
stct SState {
/******************************************************************************
                            D E C L A R A T I O N S
 ******************************************************************************/






#define PI            (2*asin(1))
#define sqr(x)        ((x)*(x))


stct SNet {                     /* A SNet:                                */
        llc::s2_t           Units;         /* - number of units in this net         */
        llc::apod<llc::s2_t> Output = {};        /* - output of ith unit                  */
        llc::apod<llc::s2_t> On = {};            /* - counting on states in equilibrium   */
        llc::apod<llc::s2_t> Off = {};           /* - counting off states in equilibrium  */
        llc::apod<llc::f3_t> Threshold = {};     /* - threshold of ith unit               */
        llc::img<llc::f3_t> Weight = {};        /* - connection weights to ith unit      */
        llc::f3_t          Temperature;   /* - actual temperature                  */
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


#define NUM_CITIES    10
#define N             (NUM_CITIES * NUM_CITIES)

llc::f3_t                  Gamma;
llc::astatic<llc::f3_t[NUM_CITIES], NUM_CITIES> Distance = {};

FILE * f = {};
llc::u2_t RandomState = 4711;
llc::s2_t RandomNext() { RandomState = RandomState * 214013U + 2531011U; rtrn (RandomState >> 16) & 32767; }
~SState() { if(f) fclose(f); }


llc::err_t InitializeApplication(SNet* /*Net*/)
{
  llc::s2_t  n1,n2;
  llc::f3_t x1,x2,y1,y2;
  llc::f3_t Alpha1, Alpha2;

  Gamma = 7;
  for (n1=0; n1<NUM_CITIES; n1++) {
    for (n2=0; n2<NUM_CITIES; n2++) {
      Alpha1 = ((llc::f3_t) n1 / NUM_CITIES) * 2 * PI;
      Alpha2 = ((llc::f3_t) n2 / NUM_CITIES) * 2 * PI;
      x1 = cos(Alpha1);
      y1 = sin(Alpha1);
      x2 = cos(Alpha2);
      y2 = sin(Alpha2);
      Distance[n1][n2] = sqrt(sqr(x1-x2) + sqr(y1-y2));
    }
  }
  if_true_fe(fopen_s(&f, "BOLTZMAN.txt", "w"));
  fprintf(f, "Temperature    Valid    Length    Tour\n\n");
  rtrn 0;
}


void CalculateWeights(SNet* Net)
{
  llc::s2_t  n1,n2,n3,n4;
  llc::s2_t  i,j;
  llc::s2_t  Pred_n3, Succ_n3;
  llc::f3_t Weight;

  for (n1=0; n1<NUM_CITIES; n1++) {
    for (n2=0; n2<NUM_CITIES; n2++) {
      i = n1*NUM_CITIES+n2;
      for (n3=0; n3<NUM_CITIES; n3++) {
        for (n4=0; n4<NUM_CITIES; n4++) {
          j = n3*NUM_CITIES+n4;
          Weight = 0;
          if (i!=j) {
            Pred_n3 = (n3==0 ? NUM_CITIES-1 : n3-1);
            Succ_n3 = (n3==NUM_CITIES-1 ? 0 : n3+1);
            if ((n1==n3) || (n2==n4))
              Weight = -Gamma;
            else if ((n1 == Pred_n3) || (n1 == Succ_n3))
              Weight = -Distance[n2][n4];
          }
          Net->Weight[i][j] = Weight;
        }
      }
      Net->Threshold[i] = -Gamma/2;
    }
  }
}


llc::s2_t ValidTour(SNet* Net)
{
  llc::s2_t n1,n2;
  llc::s2_t Cities, Stops;

  for (n1=0; n1<NUM_CITIES; n1++) {
    Cities = 0;
    Stops  = 0;
    for (n2=0; n2<NUM_CITIES; n2++) {
      if (Net->Output[n1*NUM_CITIES+n2]) {
        if (++Cities > 1) 
          return false;
      }
      if (Net->Output[n2*NUM_CITIES+n1]) {
        if (++Stops > 1) 
          return false;
      }
    }
    if ((Cities != 1) || (Stops != 1))
      return false;
  }
  return true;
}


llc::f3_t LengthOfTour(SNet* Net)
{
  llc::s2_t  n1,n2,n3;
  llc::f3_t Length;

  Length = 0;
  for (n1=0; n1<NUM_CITIES; n1++) {
    for (n2=0; n2<NUM_CITIES; n2++) {
      if (Net->Output[((n1) % NUM_CITIES)*NUM_CITIES+n2])
        break;
    }
    for (n3=0; n3<NUM_CITIES; n3++) {
      if (Net->Output[((n1+1) % NUM_CITIES)*NUM_CITIES+n3])
        break;
    }
    Length += Distance[n2][n3];
  }
  return Length;
}


void WriteTour(SNet* Net)
{
  llc::s2_t  n1,n2;
  llc::s2_t First;

  if (ValidTour(Net))
    fprintf(f, "%11.6f      yes    %6.3f    ", Net->Temperature, LengthOfTour(Net));
  else
    fprintf(f, "%11.6f       no              ", Net->Temperature);

  for (n1=0; n1<NUM_CITIES; n1++) {
    First = true;
    fprintf(f, "[");
    for (n2=0; n2<NUM_CITIES; n2++) {
      if (Net->Output[n1*NUM_CITIES+n2]) {
        if (First) {
          First = false;
          fprintf(f, "%d", n2);
        }
        else {
          fprintf(f, ", %d", n2);
        }
      }
    }
    fprintf(f, "]");
    if (n1 != NUM_CITIES-1) {
      fprintf(f, " -> ");
    }
  }
  fprintf(f, "\n");
}





/******************************************************************************
                          I N I T I A L I Z A T I O N
 ******************************************************************************/


llc::err_t GenerateNetwork(SNet* Net)
{

  Net->Units     = N;
  if_fail_fe(Net->Output.resize(N, 0));
  if_fail_fe(Net->On.resize(N, 0));
  if_fail_fe(Net->Off.resize(N, 0));
  if_fail_fe(Net->Threshold.resize(N, 0));
  if_fail_fe(Net->Weight.resize(llc::n2u2_t{(llc::u2_t)(N), (llc::u2_t)(N)}, 0));
  rtrn 0;
}


void SetRandom(SNet* Net)
{
  llc::s2_t i;
   
  for (i=0; i<Net->Units; i++) {
    Net->Output[i] = RandomEqualBOOL();
  }
}


/******************************************************************************
                     P R O P A G A T I N G   S I G N A L S
 ******************************************************************************/


void PropagateUnit(SNet* Net, llc::s2_t i)
{
  llc::s2_t  j;
  llc::f3_t Sum, Probability;

  Sum = 0;
  for (j=0; j<Net->Units; j++) {
    Sum += Net->Weight[i][j] * Net->Output[j];
  }
  Sum -= Net->Threshold[i];
  Probability = 1 / (1 + exp(-Sum / Net->Temperature));
  if (RandomEqualREAL(0, 1) <= Probability)
    Net->Output[i] = true;
  else
    Net->Output[i] = false;
}


/******************************************************************************
                      S I M U L A T I N G   T H E   N E T
 ******************************************************************************/








/******************************************************************************
                                    M A I N
 ******************************************************************************/




SNet Network = {};
void Snapshot(llc::SNN & run) { llc::nnWeights(run, Network.Weight); }
llc::u2_t Cursor = 0;
llc::err_t Begin() {
 InitializeRandoms(); if_fail_fe(GenerateNetwork(&Network)); if_fail_fe(InitializeApplication(&Network));
 CalculateWeights(&Network); SetRandom(&Network); Network.Temperature = 100; rtrn 0;
}
llc::err_t Step(llc::SNN & run) {
 const auto i = RandomEqualINT(0, Network.Units - 1);
 PropagateUnit(&Network, i);
 if(Cursor >= 1000 * N) { if(Network.Output[i]) ++Network.On[i]; else ++Network.Off[i]; }
 bool complete = false;
 if(++Cursor == 1100 * N) {
  for(llc::u2_t j = 0; j < N; ++j) Network.Output[j] = Network.On[j] > Network.Off[j];
  WriteTour(&Network); complete = ValidTour(&Network);
  Network.Temperature *= 0.99;
  for(llc::u2_t j = 0; j < N; ++j) Network.On[j] = Network.Off[j] = 0;
  Cursor = 0;
 }
 run.Width = NUM_CITIES; run.Count = N; run.Metric = complete ? LengthOfTour(&Network) : Network.Temperature;
 for(llc::u2_t j = 0; j < N; ++j) run.Values[j] = Network.Output[j];
 rtrn complete ? 1 : 0;
}

};
} // namespace
llc::err_t llc::nnCreateBoltzman(SNN & run) { rtrn nnCreate<SState>(run); }
