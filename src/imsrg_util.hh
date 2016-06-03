#ifndef imsrg_util_hh
#define imsrg_util_hh 1

#include "ModelSpace.hh"
#include "Operator.hh"
#include "HartreeFock.hh"
#include "IMSRGSolver.hh"
#include <gsl/gsl_math.h>
#include <gsl/gsl_sf_laguerre.h>
#include <gsl/gsl_sf_gamma.h>
#include <vector>

#define HBARC 197.3269718 // hc in MeV * fm
#define M_NUCLEON 938.9185 // average nucleon mass in MeV

namespace imsrg_util
{
 Operator OperatorFromString(ModelSpace& modelspace, string str);
 map<index_t,double> GetSecondOrderOccupations(Operator& H, int emax);

 Operator NumberOp(ModelSpace& modelspace, int n, int l, int j2, int tz2);
 Operator NumberOpAlln(ModelSpace& modelspace, int l, int j2, int tz2);
 Operator PSquaredOp(ModelSpace& modelspace);
 Operator RSquaredOp(ModelSpace& modelspace);
 Operator E0Op(ModelSpace& modelspace);
 Operator ElectricMultipoleOp(ModelSpace& modelspace, int L);
 Operator MagneticMultipoleOp(ModelSpace& modelspace, int L);
 Operator MagneticMultipoleOp_pn(ModelSpace& modelspace, int L, string pn);
 Operator Trel_Op(ModelSpace& modelspace);
 Operator TCM_Op(ModelSpace& modelspace);
 Operator HCM_Op(ModelSpace& modelspace);

 Operator R2CM_Op(ModelSpace& modelspace);
 Operator Rp2_corrected_Op(ModelSpace& modelspace, int A, int Z);
 Operator Rn2_corrected_Op(ModelSpace& modelspace, int A, int Z);
 Operator Rm2_corrected_Op(ModelSpace& modelspace, int A, int Z);
 Operator R2_p1_Op(ModelSpace& modelspace);
 Operator R2_1body_Op(ModelSpace& modelspace, string option);
 Operator R2_p2_Op(ModelSpace& modelspace);
 Operator R2_2body_Op(ModelSpace& modelspace, string option);
 Operator ProtonDensityAtR(ModelSpace& modelspace, double R);
 Operator NeutronDensityAtR(ModelSpace& modelspace, double R);
 Operator RpSpinOrbitCorrection(ModelSpace& modelspace);
 Operator FourierBesselCoeff(ModelSpace& modelspace, int nu, double R, vector<index_t> index_list);

 Operator Isospin2_Op(ModelSpace& modelspace);
 Operator AllowedFermi_Op(ModelSpace& modelspace);
 Operator AllowedGamowTeller_Op(ModelSpace& modelspace);
 Operator Sigma_Op(ModelSpace& modelspace);
 Operator Sigma_Op_pn(ModelSpace& modelspace, string pn);
 Operator RadialOverlap(ModelSpace& modelspace);
 Operator LdotS_Op(ModelSpace& modelspace);

 Operator Single_Ref_1B_Density_Matrix(ModelSpace& modelspace); // This doesn't work
 double Get_Charge_Density(Operator& DM, double r);  // This doesn't work


 double Calculate_p1p2(ModelSpace& modelspace, Ket & bra, Ket & ket, int J);
 void Calculate_p1p2_all(Operator& OpIn);
 double Calculate_r1r2(ModelSpace& modelspace, Ket & bra, Ket & ket, int J);
 double HO_density(int n, int l, double hw, double r);
 double HO_Radial_psi(int n, int l, double hw, double r);
 double RadialIntegral(int na, int la, int nb, int lb, int L);
 vector<double> GetOccupationsHF(HartreeFock& hf);
 vector<double> GetOccupations(HartreeFock& hf, IMSRGSolver& imsrgsolver);
 vector<double> GetDensity(vector<double>& occ, vector<double>& R, vector<int>& orbits, ModelSpace& modelspace);

 void Embed1BodyIn2Body(Operator& op1, int A);
 double GetEmbeddedTBME(Operator& op1, index_t i, index_t j, index_t k, index_t l, int Jbra,int Jket, int Lambda);

 void CommutatorTest(Operator& X, Operator& Y);
 void Reduce(Operator&);
 void UnReduce(Operator&);


// Templated functions need to be defined in the header file (or else explicitly declared in the .cc file).
 template <typename T>
 T VectorUnion(T& v1)
 {
   return v1;
 }
 
 template <typename T, typename... Args>
 T VectorUnion(T& v1, T& v2, Args... args)
 {
   T vec(v1.size()+v2.size());
   copy(v1.begin(),v1.end(),vec.begin());
   copy(v2.begin(),v2.end(),vec.begin()+v1.size());
   return VectorUnion(vec, args...);
 }

}




#endif
