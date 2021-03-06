
#ifndef TwoBodyME_h
#define TwoBodyME_h 1

#include <memory>
#include <fstream>
#include "ModelSpace.hh"
class TwoBodyME_ph;

/// The two-body piece of the operator, stored in a vector of maps of of armadillo matrices.
/// The index of the vector indicates the J-coupled two-body channel of the ket state, while the
/// map key is the two-body channel of the bra state. This is done to allow for tensor operators
/// which connect different two-body channels without having to store all possible combinations.
/// In the case of a scalar operator, there is only one map key for the bra state, corresponding
/// to that of the ket state.
/// The normalized J-coupled TBME's are stored in the matrices. However, when the TBME's are
/// accessed by GetTBME(), they are returned as
/// \f$ \tilde{\Gamma}_{ijkl} \equiv \sqrt{(1+\delta_{ij})(1+\delta_{kl})} \Gamma_{ijkl} \f$
/// because the flow equations are derived in terms of \f$ \tilde{\Gamma} \f$.
/// For efficiency, only matrix elements with \f$ i\leq j \f$ and \f$ k\leq l \f$
/// are stored.
/// When performing sums that can be cast as matrix multiplication, we have something
/// of the form
/// \f[
/// \tilde{Z}_{ijkl} \sim \frac{1}{2} \sum_{ab}\tilde{X}_{ijab} \tilde{Y}_{abkl}
/// \f]
/// which may be rewritten as a restricted sum, or matrix multiplication
/// \f[
/// Z_{ijkl} \sim \sum_{a\leq b} X_{ijab} Y_{abkl} = \left( X\cdot Y \right)_{ijkl}
/// \f]
class TwoBodyME
{
 public:
  ModelSpace*  modelspace;
  map<array<int,2>,arma::mat> MatEl;
  int nChannels;
  bool hermitian;
  bool antihermitian;
  int rank_J;
  int rank_T;
  int parity;

  ~TwoBodyME();
  TwoBodyME();
  TwoBodyME(ModelSpace*);
  TwoBodyME(TwoBodyME_ph&); // Transform a ph operator to pp.
  TwoBodyME(ModelSpace* ms, int rankJ, int rankT, int parity);

  TwoBodyME& operator*=(const double);
  TwoBodyME& operator+=(const TwoBodyME&);
  TwoBodyME& operator-=(const TwoBodyME&);

//  void Copy(const TwoBodyME&);
  void Allocate();
  bool IsHermitian(){return hermitian;};
  bool IsAntiHermitian(){return antihermitian;};
  bool IsNonHermitian(){return not (hermitian or antihermitian);};
  void SetHermitian();
  void SetAntiHermitian();
  void SetNonHermitian();

  arma::mat& GetMatrix(int chbra, int chket){return MatEl.at({chbra,chket});};
  arma::mat& GetMatrix(int ch){return GetMatrix(ch,ch);};
  arma::mat& GetMatrix(array<int,2> a){return GetMatrix(a[0],a[1]);};
  const arma::mat& GetMatrix(int chbra, int chket)const {return  MatEl.at({chbra,chket});};
  const arma::mat& GetMatrix(int ch)const {return  GetMatrix(ch,ch);};

 //TwoBody setter/getters
  double GetTBME(int ch_bra, int ch_ket, int a, int b, int c, int d) const;
  double GetTBME_norm(int ch_bra, int ch_ket, int a, int b, int c, int d) const;
  void   SetTBME(int ch_bra, int ch_ket, int a, int b, int c, int d, double tbme);
  void   AddToTBME(int ch_bra, int ch_ket, int a, int b, int c, int d, double tbme);
  double GetTBME(int ch_bra, int ch_ket, Ket &bra, Ket &ket) const;
  void   SetTBME(int ch_bra, int ch_ket, Ket &bra, Ket& ket, double tbme);
  void   AddToTBME(int ch_bra, int ch_ket, Ket &bra, Ket& ket, double tbme);
  double GetTBME_norm(int ch_bra, int ch_ket, int ibra, int iket) const;
  void   SetTBME(int ch_bra, int ch_ket, int ibra, int iket, double tbme);
  void   AddToTBME(int ch_bra, int ch_ket, int ibra, int iket, double tbme);
  double GetTBME(int j_bra, int p_bra, int t_bra, int j_ket, int p_ket, int t_ket, Ket& bra, Ket& ket) const;
  void   SetTBME(int j_bra, int p_bra, int t_bra, int j_ket, int p_ket, int t_ket, Ket& bra, Ket& ket, double tbme);
  void   AddToTBME(int j_bra, int p_bra, int t_bra, int j_ket, int p_ket, int t_ket, Ket& bra, Ket& ket, double tbme);
  double GetTBME(int j_bra, int p_bra, int t_bra, int j_ket, int p_ket, int t_ket, int a, int b, int c, int d) const;
  void   SetTBME(int j_bra, int p_bra, int t_bra, int j_ket, int p_ket, int t_ket, int a, int b, int c, int d, double tbme);
  void   AddToTBME(int j_bra, int p_bra, int t_bra, int j_ket, int p_ket, int t_ket, int a, int b, int c, int d, double tbme);
  double GetTBME_J(int j_bra, int j_ket, int a, int b, int c, int d) const;
  void   SetTBME_J(int j_bra, int j_ket, int a, int b, int c, int d, double tbme);
  void   AddToTBME_J(int j_bra, int j_ket, int a, int b, int c, int d, double tbme);
  double GetTBME_J_norm(int j_bra, int j_ket, int a, int b, int c, int d) const;

  // Scalar setters/getters for backwards compatibility
  double GetTBME(int ch, int a, int b, int c, int d) const;
  double GetTBME_norm(int ch, int a, int b, int c, int d) const;
  void   SetTBME(int ch, int a, int b, int c, int d, double tbme);
  void   AddToTBME(int ch, int a, int b, int c, int d, double tbme);
  double GetTBME(int ch, Ket &bra, Ket &ket) const;
  double GetTBME_norm(int ch, Ket &bra, Ket &ket) const;
  void   SetTBME(int ch, Ket &bra, Ket& ket, double tbme);
  void   AddToTBME(int ch, Ket &bra, Ket& ket, double tbme);
  double GetTBME_norm(int ch, int ibra, int iket) const;
  void   SetTBME(int ch, int ibra, int iket, double tbme);
  void   AddToTBME(int ch, int ibra, int iket, double tbme);
  double GetTBME(int j, int p, int t, Ket& bra, Ket& ket) const;
  void   SetTBME(int j, int p, int t, Ket& bra, Ket& ket, double tbme);
  void   AddToTBME(int j, int p, int t, Ket& bra, Ket& ket, double tbme);
  double GetTBME(int j, int p, int t, int a, int b, int c, int d) const;
  double GetTBME_norm(int j, int p, int t, int a, int b, int c, int d) const;
  void   SetTBME(int j, int p, int t, int a, int b, int c, int d, double tbme);
  void   AddToTBME(int j, int p, int t, int a, int b, int c, int d, double tbme);
  double GetTBME_J(int j, int a, int b, int c, int d) const;
  void   SetTBME_J(int j, int a, int b, int c, int d, double tbme);
  void   AddToTBME_J(int j, int a, int b, int c, int d, double tbme);

  double GetTBME_J_norm(int j, int a, int b, int c, int d) const;


  void Set_pn_TBME_from_iso(int j, int T, int tz, int a, int b, int c, int d, double tbme);
  double Get_iso_TBME_from_pn(int j, int T, int tz, int a, int b, int c, int d);

  double GetTBMEmonopole(int a, int b, int c, int d) const;
  double GetTBMEmonopole_norm(int a, int b, int c, int d) const;
  double GetTBMEmonopole(Ket & bra, Ket & ket) const;

  void Erase();
  void Scale(double);
  double Norm() const;
  void Symmetrize();
  void AntiSymmetrize();
  void Eye();
  void PrintMatrix(int chbra,int chket) const { MatEl.at({chbra,chket}).print();};
  int Dimension();
  int size();

  void WriteBinary(ofstream&);
  void ReadBinary(ifstream&);


};




#endif
