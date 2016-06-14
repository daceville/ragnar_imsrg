
#ifndef Generator_hh
#define Generator_hh 1

#include "ModelSpace.hh"
#include "Operator.hh"


class Generator
{
 public:

  string generator_type;
  Operator * H;
  Operator * Eta;
  ModelSpace* modelspace;
  double denominator_cutoff;
  double denominator_delta;
  int denominator_delta_index;

  Generator();
  void SetType(string g){generator_type = g;};
  string GetType(){return generator_type;};
  void Update(Operator* H, Operator* Eta);
  void AddToEta(Operator* H, Operator* Eta);
  void SetDenominatorCutoff(double c){denominator_cutoff=c;};
  void SetDenominatorDelta(double d){denominator_delta=d;};
  void SetDenominatorDeltaIndex(int i){denominator_delta_index=i;};
  void SetDenominatorDeltaOrbit(string orb);

// private:
  void ConstructGenerator_Wegner();
  void ConstructGenerator_White();
  void ConstructGenerator_Atan();
  void ConstructGenerator_ImaginaryTime();
  void ConstructGenerator_ShellModel();
  void ConstructGenerator_ShellModel_Atan();
  void ConstructGenerator_ShellModel_ImaginaryTime();
  void ConstructGenerator_HartreeFock();
  double Get1bDenominator(int i, int j);
  double Get2bDenominator(int ch, int ibra, int iket);

};

#endif
