
#include "Operator.hh"
#include "AngMom.hh"
#include "IMSRGProfiler.hh"
#include <cmath>
#include <iostream>
#include <iomanip>
#include <deque>

#ifndef SQRT2
  #define SQRT2 1.4142135623730950488
#endif

using namespace std;

//===================================================================================
//===================================================================================
//  START IMPLEMENTATION OF OPERATOR METHODS
//===================================================================================
//===================================================================================

//double  Operator::bch_transform_threshold = 1e-6;
double  Operator::bch_transform_threshold = 1e-9;
double  Operator::bch_product_threshold = 1e-4;
bool Operator::tensor_transform_first_pass = true; // Flag to check if we've calculated a commutator yet
bool Operator::use_brueckner_bch = false;

Operator& Operator::TempOp(size_t n)
{
  static deque<Operator> TempArray;
  if (n >= TempArray.size()) TempArray.resize(n+1,*this);
  return TempArray[n];
}

//vector<arma::mat>& Operator::TempMatVec(size_t n)
//{
//  static deque<vector<arma::mat>> TempMatVecArray;
//  if (n>= TempMatVecArray.size()) TempMatVecArray.resize(max(n,(size_t)5));
//  return TempMatVecArray[n];
//}

//////////////////// DESTRUCTOR //////////////////////////////////////////
Operator::~Operator()
{
  profiler.counter["N_Operators"] --;
}

/////////////////// CONSTRUCTORS /////////////////////////////////////////
Operator::Operator()
 :   modelspace(NULL), 
    rank_J(0), rank_T(0), parity(0), particle_rank(2),
    hermitian(true), antihermitian(false), nChannels(0)
{
  profiler.counter["N_Operators"] ++;
}


// Create a zero-valued operator in a given model space
Operator::Operator(ModelSpace& ms, int Jrank, int Trank, int p, int part_rank) : 
    modelspace(&ms), ZeroBody(0), OneBody(ms.GetNumberOrbits(), ms.GetNumberOrbits(),arma::fill::zeros),
    TwoBody(&ms,Jrank,Trank,p),  ThreeBody(&ms),
    rank_J(Jrank), rank_T(Trank), parity(p), particle_rank(part_rank),
    E3max(ms.GetE3max()),
    hermitian(true), antihermitian(false),  
    nChannels(ms.GetNumberTwoBodyChannels()) 
{
  cout << "About to SetUpOneBodyChannels();" << endl;
  SetUpOneBodyChannels();
  cout << "About to Allocate() for prank="<<particle_rank<<endl;
  if (particle_rank >=3) ThreeBody.Allocate();
  cout << "Finished allocating, counting, then moving on" << endl;
  profiler.counter["N_Operators"] ++;
}

Operator::Operator(ModelSpace& ms) :
    modelspace(&ms), ZeroBody(0), OneBody(ms.GetNumberOrbits(), ms.GetNumberOrbits(),arma::fill::zeros),
    TwoBody(&ms),  ThreeBody(&ms),
    rank_J(0), rank_T(0), parity(0), particle_rank(2),
    E3max(ms.GetE3max()),
    hermitian(true), antihermitian(false),  
    nChannels(ms.GetNumberTwoBodyChannels())
{
  SetUpOneBodyChannels();
  profiler.counter["N_Operators"] ++;
}

Operator::Operator(const Operator& op)
: modelspace(op.modelspace),  ZeroBody(op.ZeroBody),
  OneBody(op.OneBody), TwoBody(op.TwoBody) ,ThreeBody(op.ThreeBody),
  rank_J(op.rank_J), rank_T(op.rank_T), parity(op.parity), particle_rank(op.particle_rank),
  E2max(op.E2max), E3max(op.E3max), 
  hermitian(op.hermitian), antihermitian(op.antihermitian),
  nChannels(op.nChannels), OneBodyChannels(op.OneBodyChannels)
{
  profiler.counter["N_Operators"] ++;
}

Operator::Operator(Operator&& op)
: modelspace(op.modelspace), ZeroBody(op.ZeroBody),
  OneBody(move(op.OneBody)), TwoBody(move(op.TwoBody)) , ThreeBody(move(op.ThreeBody)),
  rank_J(op.rank_J), rank_T(op.rank_T), parity(op.parity), particle_rank(op.particle_rank),
  E2max(op.E2max), E3max(op.E3max), 
  hermitian(op.hermitian), antihermitian(op.antihermitian),
  nChannels(op.nChannels), OneBodyChannels(op.OneBodyChannels)
{
  profiler.counter["N_Operators"] ++;
}



/////////////// OVERLOADED OPERATORS =,+,-,*,etc ////////////////////

Operator& Operator::operator=(const Operator& rhs) = default;
Operator& Operator::operator=(Operator&& rhs) = default;

// multiply operator by a scalar
Operator& Operator::operator*=(const double rhs)
{
   ZeroBody *= rhs;
   OneBody *= rhs;
   TwoBody *= rhs;
   return *this;
}

Operator Operator::operator*(const double rhs) const
{
   Operator opout = Operator(*this);
   opout *= rhs;
   return opout;
}

// Add non-member operator so we can multiply an operator
// by a scalar from the lhs, i.e. s*O = O*s
Operator operator*(const double lhs, const Operator& rhs)
{
   return rhs * lhs;
}
Operator operator*(const double lhs, const Operator&& rhs)
{
   return rhs * lhs;
}


// divide operator by a scalar
Operator& Operator::operator/=(const double rhs)
{
   return *this *=(1.0/rhs);
}

Operator Operator::operator/(const double rhs) const
{
   Operator opout = Operator(*this);
   opout *= (1.0/rhs);
   return opout;
}

// Add operators
Operator& Operator::operator+=(const Operator& rhs)
{
   ZeroBody += rhs.ZeroBody;
   OneBody  += rhs.OneBody;
   if (rhs.GetParticleRank() > 1)
     TwoBody  += rhs.TwoBody;
   return *this;
}

Operator Operator::operator+(const Operator& rhs) const
{
   if (GetParticleRank() >= rhs.GetParticleRank())
     return ( Operator(*this) += rhs );
   else
     return ( Operator(rhs) += *this );
}

Operator& Operator::operator+=(const double& rhs)
{
   ZeroBody += rhs;
   return *this;
}

Operator Operator::operator+(const double& rhs) const
{
   return ( Operator(*this) += rhs );
}

// Subtract operators
Operator& Operator::operator-=(const Operator& rhs)
{
   ZeroBody -= rhs.ZeroBody;
   OneBody -= rhs.OneBody;
   if (rhs.GetParticleRank() > 1)
     TwoBody -= rhs.TwoBody;
   return *this;
}

Operator Operator::operator-(const Operator& rhs) const
{
   return ( Operator(*this) -= rhs );
}

Operator& Operator::operator-=(const double& rhs)
{
   ZeroBody -= rhs;
   return *this;
}

Operator Operator::operator-(const double& rhs) const
{
   return ( Operator(*this) -= rhs );
}

// Negation operator
Operator Operator::operator-() const
{
   return (*this)*-1.0;
}



void Operator::SetUpOneBodyChannels()
{
  for ( int i=0; i<modelspace->GetNumberOrbits(); ++i )
  {
    Orbit& oi = modelspace->GetOrbit(i);
    int lmin = max( oi.l - rank_J + (rank_J+parity)%2, (oi.l+parity)%2);
    int lmax = min( oi.l + rank_J, modelspace->GetEmax() );
    for (int l=lmin; l<=lmax; l+=2)
    {
      int j2min = max(max(oi.j2 - 2*rank_J, 2*l-1),1);
      int j2max = min(oi.j2 + 2*rank_J, 2*l+1);
      for (int j2=j2min; j2<=j2max; j2+=2)
      {
        int tz2min = max( oi.tz2 - 2*rank_T, -1);
        int tz2max = min( oi.tz2 + 2*rank_T, 1);
        for (int tz2=tz2min; tz2<=tz2max; tz2+=2)
        {
          OneBodyChannels[ {l, j2, tz2} ].push_back(i);
        }
      }
    }
  }
  for (auto& it: OneBodyChannels)  it.second.shrink_to_fit();
}


size_t Operator::Size()
{
   return sizeof(ZeroBody) + OneBody.size()*sizeof(double) + TwoBody.size() + ThreeBody.size();
}


void Operator::SetOneBody(int i, int j, double val)
{
 OneBody(i,j) = val;
 if ( IsNonHermitian() ) return;
 int flip_phase = IsHermitian() ? 1 : -1;
 if (rank_J > 0)
   flip_phase *= modelspace->phase( (modelspace->GetOrbit(i).j2 - modelspace->GetOrbit(j).j2) / 2 );
 OneBody(j,i) = flip_phase * val;

}

void Operator::SetTwoBody(int J1, int p1, int T1, int J2, int p2, int T2, int i, int j, int k, int l, double v)
{
  TwoBody.SetTBME( J1,  p1,  T1,  J2,  p2,  T2,  i,  j,  k,  l, v);
}

double Operator::GetTwoBody(int ch_bra, int ch_ket, int ibra, int iket)
{
  if ( ch_bra <= ch_ket or IsNonHermitian() )
  {
    return TwoBody.GetMatrix(ch_bra, ch_ket)(ibra,iket);
  }
  else
  {
    TwoBodyChannel& tbc_bra = modelspace->GetTwoBodyChannel(ch_bra);
    TwoBodyChannel& tbc_ket = modelspace->GetTwoBodyChannel(ch_ket);
    int flipphase = modelspace->phase( tbc_bra.J - tbc_ket.J) * ( IsHermitian() ? 1 : -1 ) ;
    return  flipphase * TwoBody.GetMatrix(ch_ket,ch_bra)(iket,ibra);
  }
}


void Operator::WriteBinary(ofstream& ofs)
{
  double tstart = omp_get_wtime();
  ofs.write((char*)&rank_J,sizeof(rank_J));
  ofs.write((char*)&rank_T,sizeof(rank_T));
  ofs.write((char*)&parity,sizeof(parity));
  ofs.write((char*)&particle_rank,sizeof(particle_rank));
  ofs.write((char*)&E2max,sizeof(E2max));
  ofs.write((char*)&E3max,sizeof(E3max));
  ofs.write((char*)&hermitian,sizeof(hermitian));
  ofs.write((char*)&antihermitian,sizeof(antihermitian));
  ofs.write((char*)&nChannels,sizeof(nChannels));
  ofs.write((char*)&ZeroBody,sizeof(ZeroBody));
  ofs.write((char*)OneBody.memptr(),OneBody.size()*sizeof(double));
  if (particle_rank > 1)
    TwoBody.WriteBinary(ofs);
  if (particle_rank > 2)
    ThreeBody.WriteBinary(ofs);
  profiler.timer["Write Binary Op"] += omp_get_wtime() - tstart;
}


void Operator::ReadBinary(ifstream& ifs)
{
  double tstart = omp_get_wtime();
  ifs.read((char*)&rank_J,sizeof(rank_J));
  ifs.read((char*)&rank_T,sizeof(rank_T));
  ifs.read((char*)&parity,sizeof(parity));
  ifs.read((char*)&particle_rank,sizeof(particle_rank));
  ifs.read((char*)&E2max,sizeof(E2max));
  ifs.read((char*)&E3max,sizeof(E3max));
  ifs.read((char*)&hermitian,sizeof(hermitian));
  ifs.read((char*)&antihermitian,sizeof(antihermitian));
  ifs.read((char*)&nChannels,sizeof(nChannels));
  SetUpOneBodyChannels();
  ifs.read((char*)&ZeroBody,sizeof(ZeroBody));
  ifs.read((char*)OneBody.memptr(),OneBody.size()*sizeof(double));
  if (particle_rank > 1)
    TwoBody.ReadBinary(ifs);
  if (particle_rank > 2)
    ThreeBody.ReadBinary(ifs);
  profiler.timer["Read Binary Op"] += omp_get_wtime() - tstart;
}





////////////////// MAIN INTERFACE METHODS //////////////////////////

Operator Operator::DoNormalOrdering()
{
   if (particle_rank==3)
      return DoNormalOrdering3();
   else
      return DoNormalOrdering2();
}

//*************************************************************
///  Normal ordering of a 2body operator
///  set up for scalar or tensor operators, but
///  the tensor part hasn't been tested
//*************************************************************
Operator Operator::DoNormalOrdering2()
{
   Operator opNO(*this);

   if (opNO.rank_J==0 and opNO.rank_T==0 and opNO.parity==0)
   {
     for (auto& it_k : modelspace->holes) // loop over hole orbits
     {
        index_t k = it_k.first;
        double occ_k = it_k.second;
        opNO.ZeroBody += (modelspace->GetOrbit(k).j2+1) * occ_k * OneBody(k,k);
     }
   }
   cout << "OneBody contribution: " << opNO.ZeroBody << endl;

   index_t norbits = modelspace->GetNumberOrbits();

   for ( auto& itmat : TwoBody.MatEl )
   {
      int ch_bra = itmat.first[0];
      int ch_ket = itmat.first[1];
      auto& matrix = itmat.second;
      
      TwoBodyChannel &tbc_bra = modelspace->GetTwoBodyChannel(ch_bra);
      TwoBodyChannel &tbc_ket = modelspace->GetTwoBodyChannel(ch_ket);
      int J_bra = tbc_bra.J;
      int J_ket = tbc_ket.J;

      // Zero body part
      if (opNO.rank_J==0 and opNO.rank_T==0 and opNO.parity==0)
      {
        arma::vec diagonals = matrix.diag();
        auto hh = tbc_ket.GetKetIndex_hh();
        auto hocc = tbc_ket.Ket_occ_hh;
//        opNO.ZeroBody +=  (hocc.t() * diagonals.elem(hh)) * (2*J_ket+1);
//        opNO.ZeroBody += arma::sum( diagonals.elem(hh) ) * (2*J_ket+1);
        opNO.ZeroBody += arma::sum( hocc % diagonals.elem(hh) ) * (2*J_ket+1);
      }

      // One body part
      for (index_t a=0;a<norbits;++a)
      {
         Orbit &oa = modelspace->GetOrbit(a);
         double ja = oa.j2/2.0;
         index_t bstart = IsNonHermitian() ? 0 : a; // If it's neither hermitian or anti, we need to do the full sum
         for ( auto& b : opNO.OneBodyChannels.at({oa.l,oa.j2,oa.tz2}) ) 
         {
            if (b < bstart) continue;
            Orbit &ob = modelspace->GetOrbit(b);
            double jb = ob.j2/2.0;
            for (auto& it_h : modelspace->holes)  // C++11 syntax
            {
              index_t h = it_h.first;
              double occ_h = it_h.second;
              if (opNO.rank_J==0)
              {
                 opNO.OneBody(a,b) += (2*J_ket+1.0)/(2*ja+1) * occ_h * TwoBody.GetTBME(ch_bra,ch_ket,a,h,b,h);
              }
              else
              {
                 Orbit &oh = modelspace->GetOrbit(h);
                 double jh = oh.j2/2.0;
                 opNO.OneBody(a,b) += sqrt((2*J_bra+1.0)*(2*J_ket+1.0)) * occ_h *modelspace->phase(ja+jh+J_ket+opNO.rank_J)
                                             * modelspace->GetSixJ(J_bra,J_ket,opNO.rank_J,jb,ja,jh) * TwoBody.GetTBME(ch_bra,ch_ket,a,h,b,h);
              }
           }
         }
      }
   } // loop over channels

   if (hermitian) opNO.Symmetrize();
   if (antihermitian) opNO.AntiSymmetrize();

   return opNO;
}



//*******************************************************************************
///   Normal ordering of a three body operator. Start by generating the normal ordered
///   two body piece, then use DoNormalOrdering2() to get the rest. (Note that there
///   are some numerical factors).
///   The normal ordered two body piece is 
///   \f[ \Gamma^J_{ijkl} = V^J_{ijkl} + \sum_a n_a  \sum_K \frac{2K+1}{2J+1} V^{(3)JJK}_{ijakla} \f]
///   Right now, this is only set up for scalar operators, but I don't anticipate
///   handling 3body tensor operators in the near future.
//*******************************************************************************
Operator Operator::DoNormalOrdering3()
{
   Operator opNO3 = Operator(*modelspace);
//   #pragma omp parallel for
   for ( auto& itmat : opNO3.TwoBody.MatEl )
   {
      int ch = itmat.first[0]; // assume ch_bra = ch_ket for 3body...
      TwoBodyChannel& tbc = modelspace->GetTwoBodyChannel(ch);
      arma::mat& Gamma = (arma::mat&) itmat.second;
      for (int ibra=0; ibra<tbc.GetNumberKets(); ++ibra)
      {
         Ket & bra = tbc.GetKet(ibra);
         int i = bra.p;
         int j = bra.q;
         Orbit & oi = modelspace->GetOrbit(i);
         Orbit & oj = modelspace->GetOrbit(j);
         for (int iket=ibra; iket<tbc.GetNumberKets(); ++iket)
         {
            Ket & ket = tbc.GetKet(iket);
            int k = ket.p;
            int l = ket.q;
            Orbit & ok = modelspace->GetOrbit(k);
            Orbit & ol = modelspace->GetOrbit(l);
            for (auto& it_a : modelspace->holes)
            {
               index_t a = it_a.first;
               double occ_a = it_a.second;
               Orbit & oa = modelspace->GetOrbit(a);
               if ( (2*(oi.n+oj.n+oa.n)+oi.l+oj.l+oa.l)>E3max) continue;
               if ( (2*(ok.n+ol.n+oa.n)+ok.l+ol.l+oa.l)>E3max) continue;
               int kmin2 = abs(2*tbc.J-oa.j2);
               int kmax2 = 2*tbc.J+oa.j2;
               for (int K2=kmin2; K2<=kmax2; K2+=2)
               {
                  Gamma(ibra,iket) += (K2+1) * occ_a * ThreeBody.GetME_pn(tbc.J,tbc.J,K2,i,j,a,k,l,a); // This is unnormalized, but it should be normalized!!!!
               }
            }
            Gamma(ibra,iket) /= (2*tbc.J+1)* sqrt((1+bra.delta_pq())*(1+ket.delta_pq()));
         }
      }
   }
   opNO3.Symmetrize();
   Operator opNO2 = opNO3.DoNormalOrdering2();
   opNO2.ScaleZeroBody(1./3.);
   opNO2.ScaleOneBody(1./2.);

   // Also normal order the 1 and 2 body pieces
   opNO2 += DoNormalOrdering2();
   return opNO2;

}



/// Convert to a basis normal ordered wrt the vacuum.
/// This doesn't handle 3-body terms. In that case,
/// the 2-body piece is unchanged.
Operator Operator::UndoNormalOrdering()
{
   Operator opNO = *this;
//   cout << "Undoing Normal ordering. Initial ZeroBody = " << opNO.ZeroBody << endl;

   if (opNO.GetJRank()==0 and opNO.GetTRank()==0 and opNO.GetParity()==0)
   {
     for (auto& it_k : modelspace->holes) // loop over hole orbits
     {
        index_t k = it_k.first;
        double occ_k = it_k.second;
        opNO.ZeroBody -= (modelspace->GetOrbit(k).j2+1) * occ_k * OneBody(k,k);
     }
   }

   index_t norbits = modelspace->GetNumberOrbits();

   for ( auto& itmat : TwoBody.MatEl )
   {
      int ch_bra = itmat.first[0];
      int ch_ket = itmat.first[1];
      auto& matrix = itmat.second;
      
      TwoBodyChannel &tbc_bra = modelspace->GetTwoBodyChannel(ch_bra);
      TwoBodyChannel &tbc_ket = modelspace->GetTwoBodyChannel(ch_ket);
      int J_bra = tbc_bra.J;
      int J_ket = tbc_ket.J;

      // Zero body part
      if (opNO.GetJRank()==0 and opNO.GetTRank()==0 and opNO.GetParity()==0)
      {
        arma::vec diagonals = matrix.diag();
        auto hh = tbc_ket.GetKetIndex_hh();
        auto hocc = tbc_ket.Ket_occ_hh;
        opNO.ZeroBody +=  arma::sum( hocc % diagonals.elem(hh) ) * (2*J_ket+1);
//        opNO.ZeroBody +=  hocc.t() * diagonals.elem(hh) * (2*J_ket+1);
      }

      // One body part
      for (index_t a=0;a<norbits;++a)
      {
         Orbit &oa = modelspace->GetOrbit(a);
         double ja = oa.j2/2.0;
         index_t bstart = IsNonHermitian() ? 0 : a; // If it's neither hermitian or anti, we need to do the full sum
         for ( auto& b : opNO.OneBodyChannels.at({oa.l,oa.j2,oa.tz2}) ) // OneBodyChannels should be moved to the operator, to accommodate tensors
         {
            if (b < bstart) continue;
            Orbit &ob = modelspace->GetOrbit(b);
            double jb = ob.j2/2.0;
            for (auto& it_h : modelspace->holes)  // C++11 syntax
            {
              index_t h = it_h.first;
              double occ_h = it_h.second;
              if (opNO.rank_J==0)
              {
                 opNO.OneBody(a,b) -= (2*J_ket+1.0)/(2*ja+1) * occ_h * TwoBody.GetTBME(ch_bra,ch_ket,a,h,b,h);
              }
              else
              {
                 Orbit &oh = modelspace->GetOrbit(h);
                 double jh = oh.j2/2.0;
                 opNO.OneBody(a,b) -= sqrt((2*J_bra+1.0)*(2*J_ket+1.0)) * occ_h * modelspace->phase(ja+jh+J_ket+opNO.rank_J)
                                             * modelspace->GetSixJ(J_bra,J_ket,opNO.rank_J,jb,ja,jh) * TwoBody.GetTBME(ch_bra,ch_ket,a,h,b,h);
              }
            }

//            for (auto& h : modelspace->holes)  // C++11 syntax
//            {
//               opNO.OneBody(a,b) -= (2*J_ket+1.0)/(oa.j2+1)  * TwoBody.GetTBME(ch_bra,ch_ket,a,h,b,h);
//            }
         }
      }
   } // loop over channels

   if (hermitian) opNO.Symmetrize();
   if (antihermitian) opNO.AntiSymmetrize();

//   cout << "Zero-body piece is now " << opNO.ZeroBody << endl;
   return opNO;

}

//********************************************
/// Truncate an operator to a smaller emax
/// A corresponding ModelSpace object must be
/// created at the appropriate scope. That's why
/// the new operator is passed as a 
//********************************************
Operator Operator::Truncate(ModelSpace& ms_new)
{
  Operator OpNew(ms_new, rank_J, rank_T, parity, particle_rank);
  
  int new_emax = ms_new.GetEmax();
  if ( new_emax > modelspace->GetEmax() )
  {
    cout << "Error: Cannot truncate an operator with emax = " << modelspace->GetEmax() << " to one with emax = " << new_emax << endl;
    return OpNew;
  }
//  OpNew.rank_J=rank_J; 
//  OpNew.rank_T=rank_T; 
//  OpNew.parity=parity; 
//  OpNew.particle_rank = particle_rank; 
  OpNew.ZeroBody = ZeroBody;
  OpNew.hermitian = hermitian;
  OpNew.antihermitian = antihermitian;
  int norb = ms_new.GetNumberOrbits();
  OpNew.OneBody = OneBody.submat(0,0,norb-1,norb-1);
  for (auto& itmat : OpNew.TwoBody.MatEl )
  {
    int ch = itmat.first[0];
    TwoBodyChannel& tbc_new = ms_new.GetTwoBodyChannel(ch);
    int chold = modelspace->GetTwoBodyChannelIndex(tbc_new.J,tbc_new.parity,tbc_new.Tz);
    TwoBodyChannel& tbc = modelspace->GetTwoBodyChannel(chold);
    auto& Mat_new = itmat.second;
    auto& Mat = TwoBody.GetMatrix(chold,chold);
    int nkets = tbc_new.GetNumberKets();
    arma::uvec ibra_old(nkets);
    for (int ibra=0;ibra<nkets;++ibra)
    {
      ibra_old(ibra) = tbc.GetLocalIndex(tbc_new.GetKetIndex(ibra));
    }
    Mat_new = Mat.submat(ibra_old,ibra_old);
  }
  return OpNew;
}



ModelSpace* Operator::GetModelSpace()
{
   return modelspace;
}


void Operator::Erase()
{
  EraseZeroBody();
  EraseOneBody();
  TwoBody.Erase();
  if (particle_rank >=3)
    ThreeBody.Erase();
}

void Operator::EraseOneBody()
{
   OneBody.zeros();
}

void Operator::EraseTwoBody()
{
 TwoBody.Erase();
}

void Operator::EraseThreeBody()
{
  ThreeBody = ThreeBodyME();
}

void Operator::SetHermitian()
{
  hermitian = true;
  antihermitian = false;
  TwoBody.SetHermitian();
}

void Operator::SetAntiHermitian()
{
  hermitian = false;
  antihermitian = true;
  TwoBody.SetAntiHermitian();
}

void Operator::SetNonHermitian()
{
  hermitian = false;
  antihermitian = false;
  TwoBody.SetNonHermitian();
}

void Operator::MakeReduced()
{
  if (rank_J>0)
  {
    cout << "Trying to reduce an operator with J rank = " << rank_J << ". Not good!!!" << endl;
    return;
  }
  for ( int a=0;a<modelspace->GetNumberOrbits();++a )
  {
    Orbit& oa = modelspace->GetOrbit(a);
    for (int b : OneBodyChannels.at({oa.l, oa.j2, oa.tz2}) )
    {
      if (b<a) continue;
      OneBody(a,b) *= sqrt(oa.j2+1);
      OneBody(b,a) = OneBody(a,b);
    }
  }
  for ( auto& itmat : TwoBody.MatEl )
  {
    TwoBodyChannel& tbc = modelspace->GetTwoBodyChannel(itmat.first[0]);
    itmat.second *= sqrt(2*tbc.J+1);
  }
}

void Operator::MakeNotReduced()
{
  if (rank_J>0)
  {
    cout << "Trying to un-reduce an operator with J rank = " << rank_J << ". Not good!!!" << endl;
    return;
  }
  for ( int a=0;a<modelspace->GetNumberOrbits();++a )
  {
    Orbit& oa = modelspace->GetOrbit(a);
    for (int b : OneBodyChannels.at({oa.l, oa.j2, oa.tz2}) )
    {
      if (b<a) continue;
      OneBody(a,b) /= sqrt(oa.j2+1);
      OneBody(b,a) = OneBody(a,b);
    }
  }
  for ( auto& itmat : TwoBody.MatEl )
  {
    TwoBodyChannel& tbc = modelspace->GetTwoBodyChannel(itmat.first[0]);
    itmat.second /= sqrt(2*tbc.J+1);
  }
}


void Operator::ScaleZeroBody(double x)
{
   ZeroBody *= x;
}

void Operator::ScaleOneBody(double x)
{
   OneBody *= x;
}

void Operator::ScaleTwoBody(double x)
{
   TwoBody.Scale(x);
}

void Operator::Eye()
{
   ZeroBody = 1;
   OneBody.eye();
   TwoBody.Eye();
}


/// Calculate the second-order perturbation theory correction to the energy
/// \f[
/// E^{(2)} = \sum_{ia} (2 j_a +1) \frac{|f_{ia}|^2}{f_{aa}-f_{ii}}
/// +  \sum_{\substack{i\leq j // a\leq b}}\sum_{J} (2J+1)\frac{|\Gamma_{ijab}^{J}|^2}{f_{aa}+f_{bb}-f_{ii}-f_{jj}}
/// \f]
///
double Operator::GetMP2_Energy()
{
   double t_start = omp_get_wtime();
   double Emp2 = 0;
   int nparticles = modelspace->particles.size();
   #pragma omp parallel for reduction(+:Emp2)
   for ( int ii=0;ii<nparticles;++ii)
   {
     index_t i = modelspace->particles[ii];
     double ei = OneBody(i,i);
     Orbit& oi = modelspace->GetOrbit(i);
//     for (index_t a : modelspace->holes)
     for (auto& it_a : modelspace->holes)
     {
       index_t a = it_a.first;
       double occ_a = it_a.second;
       double ea = OneBody(a,a);
       Orbit& oa = modelspace->GetOrbit(a);
       Emp2 += (oa.j2+1) * occ_a * OneBody(i,a)*OneBody(i,a)/(OneBody(a,a)-OneBody(i,i));
       for (index_t j : modelspace->particles)
       {
         if (j<i) continue;
         double ej = OneBody(j,j);
         Orbit& oj = modelspace->GetOrbit(j);
//         for ( index_t b: modelspace->holes)
         for ( auto it_b: modelspace->holes)
         {
           index_t b = it_b.first;
           double occ_b = it_b.second;
           if (b<a) continue;
           double eb = OneBody(b,b);
           Orbit& ob = modelspace->GetOrbit(b);
           double denom = ea+eb-ei-ej;
           int Jmin = max(abs(oi.j2-oj.j2),abs(oa.j2-ob.j2))/2;
           int Jmax = min(oi.j2+oj.j2,oa.j2+ob.j2)/2;
           for (int J=Jmin; J<=Jmax; ++J)
           {
             double tbme = TwoBody.GetTBME_J_norm(J,a,b,i,j);
             Emp2 += (2*J+1)* occ_a * occ_b * tbme*tbme/denom; // no factor 1/4 because of the restricted sum
           }
         }
       }
     }
   }
   profiler.timer["GetMP2_Energy"] += omp_get_wtime() - t_start;
   return Emp2;
}


//*************************************************************
/// Calculate the third order perturbation correction to energy
/// \f[
/// \begin{align}
/// \frac{1}{8}\sum_{abijpq}\sum_J (2J+1)\frac{\Gamma_{abij}^J\Gamma_{ijpq}^J\Gamma_{pqab}^J}{(f_a+f_b-f_p-f_q)(f_a+f_b-f_i-f_j)}
/// +\sum_{abcijk}\sum_J (2J+1) \frac{\Gamma_{abij}^J\Gamma_{cjkb}^J\Gamma_{ikac}^J}{(f_a+f_b-f_i-f_j)(f_a+f_c-f_i-f_k)}
/// \end{align}
/// \f]
///
//*************************************************************
double Operator::GetMP3_Energy()
{
   // So far, the pp and hh parts seem to work. No such luck for the ph.
   double t_start = omp_get_wtime();
   double Emp3 = 0;
   // This can certainly be optimized, but I'll wait until this is the bottleneck.
   int nch = modelspace->GetNumberTwoBodyChannels();

/*
   // construct Pandya-transformed ph interaction
   deque<arma::mat> V_bar_hp (InitializePandya( nChannels, "normal"));
   deque<arma::mat> V_bar_ph (InitializePandya( nChannels, "normal"));
   DoPandyaTransformation(V_bar_hp, V_bar_ph ,"normal");
   // Allocate operator for product of ph interactions
   deque<arma::mat> X_bar (nChannels );
   int n_nonzero = modelspace->SortedTwoBodyChannels_CC.size();
   for (int ich=0; ich<n_nonzero; ++ich)
   {
      int ch_cc = modelspace->SortedTwoBodyChannels_CC[ich];
      TwoBodyChannel& tbc_cc = modelspace->GetTwoBodyChannel_CC(ch_cc);
      int nKets_cc = tbc_cc.GetNumberKets();
      arma::uvec& kets_ph = tbc_cc.GetKetIndex_ph();
      


   for (int ich=0;ich<nch;++ich)
   {
     auto& Xbarmat = X_bar[ich];
     for ()
     {
       
     }
   }
*/

/*
   int n_nonzero = modelspace->SortedTwoBodyChannels_CC.size();
   for (int ich=0; ich<n_nonzero; ++ich)
   {
      int ch_cc = modelspace->SortedTwoBodyChannels_CC[ich];
      TwoBodyChannel& tbc_cc = modelspace->GetTwoBodyChannel_CC(ch_cc);
      int nKets_cc = tbc_cc.GetNumberKets();
      arma::uvec& kets_ph = tbc_cc.GetKetIndex_ph();
      int nph_kets = kets_ph.n_rows;
      if (orientation=="normal")
         X[ch_cc] = arma::mat(nph_kets,   2*nKets_cc, arma::fill::zeros);
*/

   #pragma omp parallel for reduction(+:Emp3)
   for (int ich=0;ich<nch;++ich)
   {
     TwoBodyChannel& tbc = modelspace->GetTwoBodyChannel(ich);
     auto& Mat = TwoBody.GetMatrix(ich,ich);
     int J = tbc.J;
     for (auto iket_ab : tbc.GetKetIndex_hh() )
     {
       Ket& ket_ab = tbc.GetKet(iket_ab);
       index_t a = ket_ab.p;
       index_t b = ket_ab.q;

       for (auto iket_ij : tbc.GetKetIndex_pp() )
       {
         Ket& ket_ij = tbc.GetKet(iket_ij);
         index_t i = ket_ij.p;
         index_t j = ket_ij.q;
         double Delta_abij = OneBody(a,a) + OneBody(b,b) - OneBody(i,i) - OneBody(j,j);


       // hh term
         for (auto iket_cd : tbc.GetKetIndex_hh() )
         {
           Ket& ket_cd = tbc.GetKet(iket_cd);
           index_t c = ket_cd.p;
           index_t d = ket_cd.q;
           double Delta_cdij = OneBody(c,c) + OneBody(d,d) - OneBody(i,i) - OneBody(j,j);
           Emp3 += (2*J+1)*Mat(iket_ab,iket_ij) * Mat(iket_ij,iket_cd) * Mat(iket_cd,iket_ab) / (Delta_abij * Delta_cdij);
         }

       // pp term
         for (auto iket_kl : tbc.GetKetIndex_pp() )
         {
           Ket& ket_kl = tbc.GetKet(iket_kl);
           index_t k = ket_kl.p;
           index_t l = ket_kl.q;
           double Delta_abkl = OneBody(a,a) + OneBody(b,b) - OneBody(k,k) - OneBody(l,l);
           Emp3 += (2*J+1)*Mat(iket_ab,iket_ij)*Mat(iket_ij,iket_kl)*Mat(iket_kl,iket_ab) / (Delta_abij * Delta_abkl);
         }

       } // for ij
     } // for ab
   } // for ich
   cout << "done with pp and hh. E(3) = " << Emp3 << endl;

/*
   index_t nholes = modelspace->holes.size();
   index_t norbits = modelspace->GetNumberOrbits();

//   #pragma omp parallel for reduction(+:Emp3)
   for (index_t aa=0;aa<nholes;aa++)
   {
    auto a = modelspace->holes[aa];
    double ja = 0.5*modelspace->GetOrbit(a).j2;
    for (auto b : modelspace->holes)
    {
      double jb = 0.5*modelspace->GetOrbit(b).j2;
      for (auto i : modelspace->particles)
      {
       double ji = 0.5*modelspace->GetOrbit(i).j2;
       for(auto j : modelspace->particles)
       {
         double jj = 0.5*modelspace->GetOrbit(j).j2;
         // Now the ph term. Yuck. 
         double Delta_abij = OneBody(a,a) + OneBody(b,b) - OneBody(i,i) - OneBody(j,j);
         int J1min = max(abs(ja-jb),abs(ji-jj));
         int J1max = min(ja+jb,ji+jj);
         for (auto c : modelspace->holes )
         {
           double jc = 0.5*modelspace->GetOrbit(c).j2;
           for (auto k : modelspace->particles )
           {
             double jk = 0.5*modelspace->GetOrbit(k).j2;
             double Delta_cbik = OneBody(c,c) + OneBody(b,b) - OneBody(i,i) - OneBody(k,k);
             int J2min = max(abs(jc-ji),abs(ja-jk));
             int J2max = min(jc+ji,ja+jk);
             int J3min = max(abs(jc-jb),abs(jk-jj))/2;
             int J3max = min(jc+jb,jk+jj)/2;
             for (int J1=J1min;J1<=J1max;++J1)
             {
               double tbme_abij = (2*J1+1)*TwoBody.GetTBME_J(J1,a,b,i,j);
               for (int J2=J2min;J2<=J2max;++J2)
               {
                 double tbme_cjak = (2*J2+1)*TwoBody.GetTBME_J(J2,c,j,a,k);
                 for (int J3=J3min;J3<=J3max;++J3)
                 {
                   double tbme_ikcb = (2*J3+1)*TwoBody.GetTBME_J(J3,i,k,c,b);
                   Emp3 -=  modelspace->GetNineJ(ji,jj,J1,jk,J2,ja,J3,jc,jb) * tbme_abij * tbme_cjak * tbme_ikcb / (Delta_abij * Delta_cbik);
                 } // for J3
               } // for J2
             } // for J1
           } // for k
         } // for c
       } // for j
      } // for i
     } // for b
   } // for a

*/

   profiler.timer["GetMP3_Energy"] += omp_get_wtime() - t_start;
   return Emp3;
}


/*
double Operator::GetMP3_Energy()
{
   double t_start = omp_get_wtime();
   double Emp3 = 0;
   // This can certainly be optimized, but I'll wait until this is the bottleneck.
   index_t nholes = modelspace->holes.size();
   index_t norbits = modelspace->GetNumberOrbits();
   #pragma omp parallel for reduction(+:Emp3)
   for (index_t a=0;a<nholes;++a)
   {
     Orbit& oa = modelspace->GetOrbit(a);
     for (index_t b : modelspace->holes)
     {
       if (a>b) continue;
       Orbit& ob = modelspace->GetOrbit(b);
       for (index_t i : modelspace->particles)
       {
         Orbit& oi = modelspace->GetOrbit(i);
         for (index_t j : modelspace->particles)
         {
           if (i>j) continue;
           Orbit& oj = modelspace->GetOrbit(j);
           {
             double Delta_abij  = OneBody(a,a)+OneBody(b,b)-OneBody(i,i)-OneBody(j,j);
             for (index_t p=0;p<norbits;++p)
             {
               Orbit& op = modelspace->GetOrbit(p);
               for (index_t q=p;q<norbits;++q)
               {
                 Orbit& oq = modelspace->GetOrbit(q);
                 if (p==a and q==b) continue; 
                 if (p==i and q==j) continue; 
                 double Delta_abpq = OneBody(a,a)+OneBody(b,b)-OneBody(p,p)-OneBody(q,q);
                 double Delta_apiq = OneBody(a,a)+OneBody(p,p)-OneBody(i,i)-OneBody(q,q);
                 int Jmin =  max(abs(op.j2-oq.j2),max(abs(oi.j2-oj.j2),abs(oa.j2-ob.j2)))/2;
                 int Jmax =  min(op.j2+oq.j2,min(oi.j2+oj.j2,oa.j2+ob.j2))/2;
                 for (int J=Jmin;J<=Jmax;++J)
                 {
                   double tbme1 = TwoBody.GetTBME_J_norm(J,a,b,i,j);
                   if (q<nholes or p>=nholes)
                   {
                     double tbme2 = TwoBody.GetTBME_J_norm(J,i,j,p,q);
                     double tbme3 = TwoBody.GetTBME_J_norm(J,p,q,a,b);
                     Emp3 += 2*(2*J+1)*tbme1*tbme2*tbme3/(Delta_abij*Delta_abpq);
                   }
                   else
                   {
                     double tbme4 = TwoBody.GetTBME_J_norm(J,p,j,q,b);
                     double tbme5 = TwoBody.GetTBME_J_norm(J,i,q,a,p);
                     Emp3 += 16*(2*J+1)*tbme1*tbme4*tbme5/(Delta_abij*Delta_apiq);
                   }
                 }
               }
             }
           }
         }
       }
     }
   }

   profiler.timer["GetMP3_Energy"] += omp_get_wtime() - t_start;
   return Emp3;
}
*/

//*************************************************************
/// Evaluate first order perturbative correction to the operator's
/// ground-state expectation value. A HF basis is assumed.
/// \f[
///  \mathcal{O}^{(1)} = 2\sum_{abij} \frac{H_{abij}\mathcal{O}_{ijab}}{\Delta_{abij}}
/// \f]
///
//*************************************************************
double Operator::MP1_Eval(Operator& H)
{
  auto& Op = *this;
  double opval = 0;
  int nch = modelspace->GetNumberTwoBodyChannels();
  for (int ich=0;ich<nch;++ich)
  {
    TwoBodyChannel& tbc = modelspace->GetTwoBodyChannel(ich);
    int J = tbc.J;
    auto& Hmat = H.TwoBody.GetMatrix(ich,ich);
    auto& Opmat = Op.TwoBody.GetMatrix(ich,ich);
    for (auto ibra : tbc.GetKetIndex_hh() )
    {
      Ket& bra = tbc.GetKet(ibra);
      for (auto iket : tbc.GetKetIndex_pp() )
      {
        Ket& ket = tbc.GetKet(iket);
        double Delta_abij = H.OneBody(bra.p,bra.p) + H.OneBody(bra.q,bra.q) -H.OneBody(ket.p,ket.p) - H.OneBody(ket.q,ket.q);
        opval += 2*(2*J+1)*Hmat(ibra,iket) * Opmat(iket,ibra) / Delta_abij;
      }
    }
  }
  return opval;
}


//***********************************************
/// Calculates the kinetic energy operator in the 
/// harmonic oscillator basis.
/// \f[ t_{ab} = \frac{1}{2}\hbar\omega
/// \delta_{\ell_a \ell_b} \delta_{j_aj_b} \delta_{t_{za}t_{zb}}
/// \left\{
/// \begin{array}{ll}
/// 2n_a + \ell_a + \frac{3}{2} &: n_a=n_b\\
/// \sqrt{n_{a}(n_{a}+\ell_a + \frac{1}{2})} &: n_a=n_b+1\\
/// \end{array} \right. \f]
//***********************************************
[[deprecated]] void Operator::CalculateKineticEnergy()
{
   OneBody.zeros();
   int norbits = modelspace->GetNumberOrbits();
   double hw = modelspace->GetHbarOmega();
   for (int a=0;a<norbits;++a)
   {
      Orbit & oa = modelspace->GetOrbit(a);
      OneBody(a,a) = 0.5 * hw * (2*oa.n + oa.l +3./2); 
      for (int b=a+1;b<norbits;++b)  // make this better once OneBodyChannel is implemented
      {
         Orbit & ob = modelspace->GetOrbit(b);
         if (oa.l == ob.l and oa.j2 == ob.j2 and oa.tz2 == ob.tz2)
         {
            if (oa.n == ob.n+1)
               OneBody(a,b) = 0.5 * hw * sqrt( (oa.n)*(oa.n + oa.l +1./2));
            else if (oa.n == ob.n-1)
               OneBody(a,b) = 0.5 * hw * sqrt( (ob.n)*(ob.n + ob.l +1./2));
            OneBody(b,a) = OneBody(a,b);
         }
      }
   }
}




//*****************************************************************************************
/// X.BCH_Transform(Y) returns \f$ Z = e^{Y} X e^{-Y} \f$.
/// We use the [Baker-Campbell-Hausdorff formula](http://en.wikipedia.org/wiki/Baker-Campbell-Hausdorff_formula)
/// \f[ Z = X + [Y,X] + \frac{1}{2!}[Y,[Y,X]] + \frac{1}{3!}[Y,[Y,[Y,X]]] + \ldots \f]
/// with all commutators truncated at the two-body level.
Operator Operator::BCH_Transform( const Operator &Omega)
{
   if (use_brueckner_bch) return Brueckner_BCH_Transform( Omega );
   else return Standard_BCH_Transform( Omega );
}

/// X.BCH_Transform(Y) returns \f$ Z = e^{Y} X e^{-Y} \f$.
/// We use the [Baker-Campbell-Hausdorff formula](http://en.wikipedia.org/wiki/Baker-Campbell-Hausdorff_formula)
/// \f[ Z = X + [Y,X] + \frac{1}{2!}[Y,[Y,X]] + \frac{1}{3!}[Y,[Y,[Y,X]]] + \ldots \f]
/// with all commutators truncated at the two-body level.
Operator Operator::Standard_BCH_Transform( const Operator &Omega)
{
   double t_start = omp_get_wtime();
   int max_iter = 40;
   int warn_iter = 12;
   double nx = Norm();
   double ny = Omega.Norm();
   Operator OpOut = *this;
   if (nx>bch_transform_threshold)
   {
//     Operator& OpNested = TempOp(0);
//     Operator& OpNested = *this;
     Operator OpNested = *this;
//     OpNested = *this;
//     tmp1 = *this;
     double epsilon = nx * exp(-2*ny) * bch_transform_threshold / (2*ny);
     for (int i=1; i<=max_iter; ++i)
     {
//        Operator& tmp1 = TempOp(1);
//        tmp1.SetToCommutator(Omega,OpNested);
//        cout << "Assign tmp1" << endl;
        Operator tmp1 = Commutator(Omega,OpNested);
//        cout << "tmp1 /=i" << endl;
//        if (this->rank_J==0)
        {
         tmp1 /= i;
        }
//        cout << "OpNested = tmp1" << endl;
        OpNested = tmp1;
//        cout << "OpOut += OpNested" << endl;
        OpOut += OpNested;
  
        if (this->rank_J > 0)
        {
            cout << "Tensor BCH, i=" << i << "  Norm = " << OpNested.OneBodyNorm() << " "  << OpNested.TwoBodyNorm() << " " << OpNested.Norm() << endl;
//            if (i>=10 and OpNested.Norm() < 1e-6 ) break;
        }
        if (OpNested.Norm() < 1e-6 )  break;
//        if (OpNested.Norm() < 1e-6 and this->rank_J==0)  break;
//        if (OpNested.Norm() < epsilon *(i+1))  break;
        if (i == warn_iter)  cout << "Warning: BCH_Transform not converged after " << warn_iter << " nested commutators" << endl;
        else if (i == max_iter)   cout << "Warning: BCH_Transform didn't coverge after "<< max_iter << " nested commutators" << endl;
     }
   }
   profiler.timer["BCH_Transform"] += omp_get_wtime() - t_start;
   return OpOut;
}
/// Variation of the BCH transformation procedure
/// requested by a one Mr. T.D. Morris
/// \f[ e^{\Omega_1 + \Omega_2} X e^{-\Omega_1 - \Omega_2}
///    \rightarrow 
///  e^{\Omega_2} e^{\Omega_1}  X e^{-\Omega_1} e^{-\Omega_2} \f]
Operator Operator::Brueckner_BCH_Transform( const Operator &Omega)
{
   Operator Omega1 = Omega;
   Operator Omega2 = Omega;
   Omega1.SetParticleRank(1);
   Omega2.EraseOneBody();
   Operator OpOut = this->Standard_BCH_Transform(Omega1);
   OpOut = OpOut.Standard_BCH_Transform(Omega2);
   return OpOut;
}


//*****************************************************************************************
// Baker-Campbell-Hausdorff formula
//  returns Z, where
//  exp(Z) = exp(X) * exp(Y).
//  Z = X + Y + 1/2[X, Y]
//     + 1/12 [X,[X,Y]] + 1/12 [Y,[Y,X]]
//     - 1/24 [Y,[X,[X,Y]]]
//     - 1/720 [Y,[Y,[Y,[Y,X]]]] - 1/720 [X,[X,[X,[X,Y]]]]
//     + ...
//*****************************************************************************************
/// X.BCH_Product(Y) returns \f$Z\f$ such that \f$ e^{Z} = e^{X}e^{Y}\f$
/// by employing the [Baker-Campbell-Hausdorff formula](http://en.wikipedia.org/wiki/Baker-Campbell-Hausdorff_formula)
/// \f[ Z = X + Y + \frac{1}{2}[X,Y] + \frac{1}{12}([X,[X,Y]]+[Y,[Y,X]]) + \ldots \f]
//*****************************************************************************************
Operator Operator::BCH_Product(  Operator &Y)
{
   double tstart = omp_get_wtime();
   Operator& X = *this;
   double nx = X.Norm();
   double ny = Y.Norm();
//   if (nx < 1e-8) return Y;
//   if (ny < 1e-8) return X;
   vector<double> bernoulli = {1.0, -0.5, 1./6, 0.0, -1./30, 0.0 , 1./42, 0, -1./30};
   vector<double> factorial = {1.0,  1.0,  2.0, 6.0,    24., 120., 720., 5040., 40320.};


   Operator Z = X + Y;
   Operator Nested = Y;
   Nested.SetToCommutator(Y,X);
   double nxy = Nested.Norm();
   // We assume X is small, but just in case, we check if we should include the [X,[X,Y]] term.
   if ( nxy*nx > 1e-6)
   {
     Z += (1./12) * Commutator(Nested,X);
//     cout << "Operator::BCH_Product -- Included X^2 term. " << nx << " " << ny << " " << nxy << endl;
   }
   
   int k = 1;
   while( Nested.Norm() > 1e-6 and k<9)
   {
     if (k<2 or k%2==0)
        Z += (bernoulli[k]/factorial[k]) * Nested;
     Nested = Commutator(Y,Nested);
     k++;
   }

   profiler.timer["BCH_Product"] += omp_get_wtime() - tstart;
   return Z;
}

/*
Operator Operator::BCH_Product(  Operator &Y)
{
   double tstart = omp_get_wtime();
   Operator& X = *this;
   double nx = X.Norm();
   double ny = Y.Norm();
   if (nx < 1e-7) return Y;
   if (ny < 1e-7) return X;

   Operator Z;
   Z.SetToCommutator(X,Y);
   double nxy = Z.Norm();
   Z *= 0.5;

   if (nxy > (nx+ny)*bch_product_threshold )
   {
     Operator& tmp = TempOp(0);
     tmp.SetToCommutator(Z,Y-Z);
     tmp /= 6;
     Z += tmp;
//     Z += (1./6) * Commutator(Z,Y-X);
   }
   Z += X;
   Z += Y;
   profiler.timer["BCH_Product"] += omp_get_wtime() - tstart;
   return Z;
}
*/

/// Obtain the Frobenius norm of the operator, which here is 
/// defined as 
/// \f[ \|X\| = \sqrt{\|X_{(1)}\|^2 +\|X_{(2)}\|^2 } \f]
/// and
/// \f[ \|X_{(1)}\|^2 = \sum\limits_{ij} X_{ij}^2 \f]
double Operator::Norm() const
{
   double n1 = OneBodyNorm();
   double n2 = TwoBody.Norm();
   return sqrt(n1*n1+n2*n2);
}

double Operator::OneBodyNorm() const
{
   return arma::norm(OneBody,"fro");
}



double Operator::TwoBodyNorm() const
{
  return TwoBody.Norm();
}

void Operator::Symmetrize()
{
   if (rank_J==0)
     OneBody = arma::symmatu(OneBody);
   else
   {
     int norb = modelspace->GetNumberOrbits();
     for (int i=0;i<norb; ++i)
     {
       Orbit& oi = modelspace->GetOrbit(i);
       for ( int j : OneBodyChannels.at({oi.l,oi.j2,oi.tz2}) )
       {
         if (j<= i) continue;
         Orbit& oj = modelspace->GetOrbit(j);
         OneBody(j,i) = modelspace->phase((oi.j2-oj.j2)/2) * OneBody(i,j);
       }
     }
   }
   TwoBody.Symmetrize();
}

void Operator::AntiSymmetrize()
{
   if (rank_J==0)
   {
     OneBody = arma::trimatu(OneBody) - arma::trimatu(OneBody).t();
   }
   else
   {
     int norb = modelspace->GetNumberOrbits();
     for (int i=0;i<norb; ++i)
     {
       Orbit& oi = modelspace->GetOrbit(i);
       for ( int j : OneBodyChannels.at({oi.l,oi.j2,oi.tz2}) )
       {
         if (j<= i) continue;
         if (rank_J==0)
          OneBody(j,i) = -OneBody(i,j);
         else
         {
           Orbit& oj = modelspace->GetOrbit(j);
           OneBody(j,i) = -modelspace->phase((oi.j2-oj.j2)/2) * OneBody(i,j);
         }
       }
     }
   }
   TwoBody.AntiSymmetrize();
}

//Operator Operator::Commutator( Operator& opright)
/// Returns \f$ Z = [X,Y] \f$
/// @relates Operator
Operator Commutator( const Operator& X, const Operator& Y)
{
//  if (X.GetJRank()+Y.GetJRank()>0) cout << "in Commutator" << endl;
  int jrank = max(X.rank_J,Y.rank_J);
  int trank = max(X.rank_T,Y.rank_T);
  int parity = (X.parity+Y.parity)%2;
  int particlerank = max(X.particle_rank,Y.particle_rank);
  Operator Z(*(X.modelspace),jrank,trank,parity,particlerank);
//  if (X.GetJRank()+Y.GetJRank()>0) cout << "calling SetToCommutator" << endl;
  Z.SetToCommutator(X,Y);
  return Z;
}

void Operator::SetToCommutator( const Operator& X, const Operator& Y)
{
   profiler.counter["N_Commutators"] += 1;
   double t_start = omp_get_wtime();
   Operator& Z = *this;
   int xrank = X.rank_J + X.rank_T + X.parity;
   int yrank = Y.rank_J + Y.rank_T + Y.parity;
   if (xrank==0)
   {
      if (yrank==0)
      {
//         return X.CommutatorScalarScalar(Y); // [S,S]
//         return CommutatorScalarScalar(X,Y); // [S,S]
         Z.CommutatorScalarScalar(X,Y); // [S,S]
      }
      else
      {
//         return X.CommutatorScalarTensor(Y); // [S,T]
//         return CommutatorScalarTensor(X,Y); // [S,T]
         Z.CommutatorScalarTensor(X,Y); // [S,T]
      }
   }
   else if(yrank==0)
   {
//      return (-1)*Y.CommutatorScalarTensor(X); // [T,S]
//      return (-1)*CommutatorScalarTensor(Y,X); // [T,S]
      Z.CommutatorScalarTensor(Y,X); // [T,S]
      Z *= -1;
   }
   else
   {
      cout << "In Tensor-Tensor because X.rank_J = " << X.rank_J << "  and Y.rank_J = " << Y.rank_J << endl;
      cout << " Tensor-Tensor commutator not yet implemented." << endl;
//      return X;
   }
   profiler.timer["Commutator"] += omp_get_wtime() - t_start;
}


/// Commutator where \f$ X \f$ and \f$Y\f$ are scalar operators.
/// Should be called through Commutator()
//Operator Operator::CommutatorScalarScalar( Operator& opright) 
//Operator CommutatorScalarScalar( const Operator& X, const Operator& Y) 
void Operator::CommutatorScalarScalar( const Operator& X, const Operator& Y) 
{
   double t_css = omp_get_wtime();
   Operator& Z = *this;
   Z = X.GetParticleRank()>Y.GetParticleRank() ? X : Y;
   Z.EraseZeroBody();
   Z.EraseOneBody();
   Z.EraseTwoBody();

//   if ( (IsHermitian() and opright.IsHermitian()) or (IsAntiHermitian() and opright.IsAntiHermitian()) ) out.SetAntiHermitian();
//   else if ( (IsHermitian() and opright.IsAntiHermitian()) or (IsAntiHermitian() and opright.IsHermitian()) ) out.SetHermitian();
//   else out.SetNonHermitian();
   if ( (X.IsHermitian() and Y.IsHermitian()) or (X.IsAntiHermitian() and Y.IsAntiHermitian()) ) Z.SetAntiHermitian();
   else if ( (X.IsHermitian() and Y.IsAntiHermitian()) or (X.IsAntiHermitian() and Y.IsHermitian()) ) Z.SetHermitian();
   else Z.SetNonHermitian();

   if ( not Z.IsAntiHermitian() )
   {
      Z.comm110ss(X, Y);
      Z.comm220ss(X, Y) ;
   }

   double t_start = omp_get_wtime();
   Z.comm111ss(X, Y);
   profiler.timer["comm111ss"] += omp_get_wtime() - t_start;

    t_start = omp_get_wtime();
   Z.comm121ss(X,Y);
   profiler.timer["comm121ss"] += omp_get_wtime() - t_start;

    t_start = omp_get_wtime();
   Z.comm122ss(X,Y); 
   profiler.timer["comm122ss"] += omp_get_wtime() - t_start;

   if (X.particle_rank>1 and Y.particle_rank>1)
   {
    t_start = omp_get_wtime();
    Z.comm222_pp_hh_221ss(X, Y);
    profiler.timer["comm222_pp_hh_221ss"] += omp_get_wtime() - t_start;
     
    t_start = omp_get_wtime();
    Z.comm222_phss(X, Y);
    profiler.timer["comm222_phss"] += omp_get_wtime() - t_start;
   }


   if ( Z.IsHermitian() )
      Z.Symmetrize();
   else if (Z.IsAntiHermitian() )
      Z.AntiSymmetrize();

   profiler.timer["CommutatorScalarScalar"] += omp_get_wtime() - t_css;

//   return Z;
}


/// Commutator \f$[X,Y]\f$ where \f$ X \f$ is a scalar operator and \f$Y\f$ is a tensor operator.
/// Should be called through Commutator()
//Operator Operator::CommutatorScalarTensor( Operator& opright) 
//Operator CommutatorScalarTensor( const Operator& X, const Operator& Y) 
void Operator::CommutatorScalarTensor( const Operator& X, const Operator& Y) 
{
   double t_start = omp_get_wtime();
   Operator& Z = *this;
   Z = Y; // This ensures the commutator has the same tensor rank as Y
   Z.EraseZeroBody();
   Z.EraseOneBody();
   Z.EraseTwoBody();

   if ( (X.IsHermitian() and Y.IsHermitian()) or (X.IsAntiHermitian() and Y.IsAntiHermitian()) ) Z.SetAntiHermitian();
   else if ( (X.IsHermitian() and Y.IsAntiHermitian()) or (X.IsAntiHermitian() and Y.IsHermitian()) ) Z.SetHermitian();
   else Z.SetNonHermitian();

//   cout << "comm111st" << endl;
   Z.comm111st(X, Y);
//   cout << "comm121st" << endl;
   Z.comm121st(X, Y);

//   cout << "comm122st" << endl;
   Z.comm122st(X, Y);
//   cout << "comm222_pp_hh_st" << endl;
   Z.comm222_pp_hh_221st(X, Y);
//   cout << "comm222_phst" << endl;
   Z.comm222_phst(X, Y);

//   cout << "symmetrize" << endl;
   if ( Z.IsHermitian() )
      Z.Symmetrize();
   else if (Z.IsAntiHermitian() )
      Z.AntiSymmetrize();
//   cout << "done." << endl;
   profiler.timer["CommutatorScalarTensor"] += omp_get_wtime() - t_start;
//   return Z;
}



///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
// Below is the implementation of the commutators in the various channels
///////////////////////////////////////////////////////////////////////////////////////////

//*****************************************************************************************
//                ____Y    __         ____X
//          X ___(_)             Y___(_) 
//
//  [X1,Y1](0) = Sum_ab (2j_a+1) x_ab y_ba  (n_a-n_b) 
//             = Sum_a  (2j_a+1)  (xy-yx)_aa n_a
//
// -- AGREES WITH NATHAN'S RESULTS
/// \f[
///  [X_{1)},Y_{(1)}]_{(0)} = \sum_{a} n_a (2j_a+1) \left(X_{(1)}Y_{(1)}-Y_{(1)}X_{(1)}\right)_{aa}
/// \f]
void Operator::comm110ss( const Operator& X, const Operator& Y) 
{
  Operator& Z = *this;
  if (X.IsHermitian() and Y.IsHermitian()) return ; // I think this is the case
  if (X.IsAntiHermitian() and Y.IsAntiHermitian()) return ; // I think this is the case

   arma::mat xyyx = X.OneBody*Y.OneBody - Y.OneBody*X.OneBody;
   for ( auto& it_a : modelspace->holes) 
   {
      index_t a = it_a.first;
      double occ_a = it_a.second;
      Z.ZeroBody += (modelspace->GetOrbit(a).j2+1) * occ_a * xyyx(a,a);
//      cout << a << ": " << occ_a << "   " << xyyx(a,a) << "  " << Z.ZeroBody << endl;
   }
}


//*****************************************************************************************
//         __Y__       __X__
//        ()_ _()  -  ()_ _()
//           X           Y
//
//  [ X^(2), Y^(2) ]^(0) = 1/2 Sum_abcd  Sum_J (2J+1) x_abcd y_cdab (n_a n_b nbar_c nbar_d)
//                       = 1/2 Sum_J (2J+1) Sum_abcd x_abcd y_cdab (n_a n_b nbar_c nbar_d)  
//                       = 1/2 Sum_J (2J+1) Sum_ab  (X*P_pp*Y)_abab  P_hh
//
//  -- AGREES WITH NATHAN'S RESULTS (within < 1%)
/// \f[
/// [X_{(2)},Y_{(2)}]_{(0)} = \frac{1}{2} \sum_{J} (2J+1) \sum_{abcd} (n_a n_b \bar{n}_c \bar{n}_d) \tilde{X}_{abcd}^{J} \tilde{Y}_{cdab}^{J}
/// \f]
/// may be rewritten as
/// \f[
/// [X_{(2)},Y_{(2)}]_{(0)} = 2 \sum_{J} (2J+1) Tr(X_{hh'pp'}^{J} Y_{pp'hh'}^{J})
/// \f] where we obtain a factor of four from converting two unrestricted sums to restricted sums, i.e. \f$\sum_{ab} \rightarrow \sum_{a\leq b} \f$,
/// and using the normalized TBME.
void Operator::comm220ss( const Operator& X, const Operator& Y) 
{
   Operator& Z = *this;
   if (X.IsHermitian() and Y.IsHermitian()) return; // I think this is the case
   if (X.IsAntiHermitian() and Y.IsAntiHermitian()) return; // I think this is the case

   for (int ch=0;ch<nChannels;++ch)
   {
      TwoBodyChannel& tbc = modelspace->GetTwoBodyChannel(ch);
      auto hh = tbc.GetKetIndex_hh();
      auto pp = tbc.GetKetIndex_pp();
      auto ph = tbc.GetKetIndex_ph();
      auto ppph = arma::join_cols(pp,ph);
      if (hh.size()==0 or pp.size()==0) continue;
      auto nn = tbc.Ket_occ_hh;
      auto nbarnbar = tbc.Ket_unocc_ph; // note: this is an arma::rowvec
//      auto & X2 = X.TwoBody.GetMatrix(ch).submat(hh,pp);
//      arma::mat Y2 = Y.TwoBody.GetMatrix(ch).submat(pp,hh);
      auto & X2 = X.TwoBody.GetMatrix(ch).submat(hh,ppph);
      arma::mat Y2 = Y.TwoBody.GetMatrix(ch).submat(ppph,hh);
//      Y2.each_row() %= nbarnbar.t();
//      cout << "ch = " << ch << " " << tbc.J << " " << tbc.parity << " " << tbc.Tz 
//           << "  sizes hh,pp,ph = " << hh.size() << "," << pp.size() << "," << ph.size() << endl;
//      cout << "Y2:"  << endl << Y2 << endl;
//      cout << "nn: " << endl << nn << endl;
//      cout << "nbarnbar: " << endl << nbarnbar << endl;
//      cout << "Y2 dim = " << Y2.n_rows << "," << Y2.n_cols << "   nbarnbar.size = " << nbarnbar.size() << endl;
//      cout << "Y2.tail_rows(nbarnbar.size()):" << Y2.tail_rows(nbarnbar.size()) << endl;
      Y2.tail_rows(nbarnbar.size()).each_col() %= nbarnbar;
      // Try vectorize to avoid excessive multiplications ??
//      cout << "ch = " << ch << "X2" << endl << X2 << endl << "Y2" << endl << Y2 << endl << "  X2Y2: " << endl << arma::diagvec(X2 * Y2) << endl << "nn: " << endl << nn << endl << "ZeroBody = " << Z.ZeroBody << endl;
      Z.ZeroBody += 2 * (2*tbc.J+1) * arma::sum(arma::diagvec( X2 * Y2 ) % nn); // This could be made more efficient, but who cares?
//      Z.ZeroBody += 2 * (2*tbc.J+1) * arma::sum(arma::diagvec( X2 * Y2 ) ); // This could be made more efficient, but who cares?
   }
}

//*****************************************************************************************
//
//        |____. Y          |___.X
//        |        _        |
//  X .___|            Y.___|              [X1,Y1](1)  =  XY - YX
//        |                 |
//
// -- AGREES WITH NATHAN'S RESULTS
/// \f[
/// [X_{(1)},Y_{(1)}]_{(1)} = X_{(1)}Y_{(1)} - Y_{(1)}X_{(1)}
/// \f]
//void Operator::comm111ss( Operator & Y, Operator& Z) 
void Operator::comm111ss( const Operator & X, const Operator& Y) 
{
   Operator& Z = *this;
   Z.OneBody += X.OneBody*Y.OneBody - Y.OneBody*X.OneBody;
}

//*****************************************************************************************
//                                       |
//      i |              i |             |
//        |    ___.Y       |__X__        |
//        |___(_)    _     |   (_)__.    |  [X2,Y1](1)  =  1/(2j_i+1) sum_ab(n_a-n_b)y_ab 
//      j | X            j |        Y    |        * sum_J (2J+1) x_biaj^(J)  
//                                       |      
//---------------------------------------*        = 1/(2j+1) sum_a n_a sum_J (2J+1)
//                                                  * sum_b y_ab x_biaj - yba x_aibj
//
//                     (note: I think this should actually be)
//                                                = sum_ab (n_a nbar_b) sum_J (2J+1)/(2j_i+1)
//                                                      * y_ab xbiag - yba x_aibj
//
// -- AGREES WITH NATHAN'S RESULTS 
/// Returns \f$ [X_{(1)},Y_{(2)}] - [Y_{(1)},X_{(2)}] \f$, where
/// \f[
/// [X_{(1)},Y_{(2)}]_{ij} = \frac{1}{2j_i+1}\sum_{ab} (n_a \bar{n}_b) \sum_{J} (2J+1) (X_{ab} Y^J_{biaj} - X_{ba} Y^J_{aibj})
/// \f]
//void Operator::comm121ss( Operator& Y, Operator& Z) 
void Operator::comm121ss( const Operator& X, const Operator& Y) 
{
   Operator& Z = *this;
   index_t norbits = modelspace->GetNumberOrbits();
   #pragma omp parallel for 
   for (index_t i=0;i<norbits;++i)
   {
      Orbit &oi = modelspace->GetOrbit(i);
      index_t jmin = Z.IsNonHermitian() ? 0 : i;
      for (auto j : Z.OneBodyChannels.at({oi.l,oi.j2,oi.tz2}) ) 
      {
          if (j<jmin) continue; // only calculate upper triangle
//          for (auto& a : modelspace->holes)  // C++11 syntax
          for (auto& it_a : modelspace->holes)  // C++11 syntax
          {
             index_t a = it_a.first;
             double occ_a = it_a.second;
             Orbit &oa = modelspace->GetOrbit(a);
//             for (auto& b : modelspace->particles)
             for (index_t b=0; b<norbits; ++b)
             {
                Orbit &ob = modelspace->GetOrbit(b);
                double nanb = occ_a * (1-ob.occ);
                if (abs(nanb)<1e-6) continue;
                Z.OneBody(i,j) += (ob.j2+1) * nanb *  X.OneBody(a,b) * Y.TwoBody.GetTBMEmonopole(b,i,a,j) ;
                Z.OneBody(i,j) -= (oa.j2+1) * nanb *  X.OneBody(b,a) * Y.TwoBody.GetTBMEmonopole(a,i,b,j) ;

                // comm211 part
                Z.OneBody(i,j) -= (ob.j2+1) * nanb * Y.OneBody(a,b) * X.TwoBody.GetTBMEmonopole(b,i,a,j) ;
                Z.OneBody(i,j) += (oa.j2+1) * nanb * Y.OneBody(b,a) * X.TwoBody.GetTBMEmonopole(a,i,b,j) ;
             }
          }
      }
   }
}



//*****************************************************************************************
//
//      i |              i |            [X2,Y2](1)  =  1/(2(2j_i+1)) sum_J (2J+1) 
//        |__Y__           |__X__           * sum_abc (nbar_a*nbar_b*n_c + n_a*n_b*nbar_c)
//        |    /\          |    /\          * (x_ciab y_abcj - y_ciab xabcj)
//        |   (  )   _     |   (  )                                                                                      
//        |____\/          |____\/       = 1/(2(2j+1)) sum_J (2J+1)
//      j | X            j |  Y            *  sum_c ( Pp*X*Phh*Y*Pp - Pp*Y*Phh*X*Pp)  - (Ph*X*Ppp*Y*Ph - Ph*Y*Ppp*X*Ph)_cicj
//                                     
//
// -- AGREES WITH NATHAN'S RESULTS 
//   No factor of 1/2 because the matrix multiplication corresponds to a restricted sum (a<=b) 
// \f[
// [X_{(2)},Y_{(2)}]_{ij} = \frac{1}{2(2j_i+1)}\sum_{J}(2J+1)\sum_{c}
// \left( \mathcal{P}_{pp} (X \mathcal{P}_{hh} Y^{J} 
// - Y^{J} \mathcal{P}_{hh} X^{J}) \mathcal{P}_{pp}
//  - \mathcal{P}_{hh} (X^{J} \mathcal{P}_{pp} Y^{J} 
//  -  Y^{J} \mathcal{P}_{pp} X^{J}) \mathcal{P}_{hh} \right)_{cicj}
// \f]
/// \f[
/// [X_{(2)},Y_{(2)}]_{ij} = \frac{1}{2(2j_i+1)}\sum_{J}(2J+1)\sum_{abc} (\bar{n}_a\bar{n}_bn_c + n_an_b\bar{n}_c)
///  (X^{J}_{ciab} Y^{J}_{abcj} - Y^{J}_{ciab}X^{J}_{abcj})
/// \f]
/// This may be rewritten as
/// \f[
/// [X_{(2)},Y_{(2)}]_{ij} = \frac{1}{2j_i+1} \sum_{c} \sum_{J} (2J+1) \left( n_c \mathcal{M}^{J}_{pp,icjc} + \bar{n}_c\mathcal{M}^{J}_{hh,icjc} \right)
/// \f]
/// With the intermediate matrix \f[ \mathcal{M}^{J}_{pp} \equiv \frac{1}{2} (X^{J}\mathcal{P}_{pp} Y^{J} - Y^{J}\mathcal{P}_{pp}X^{J}) \f]
/// and likewise for \f$ \mathcal{M}^{J}_{hh} \f$
//void Operator::comm221ss( Operator& Y, Operator& Z) 
void Operator::comm221ss( const Operator& X, const Operator& Y) 
{

   Operator& Z = *this;
   int norbits = modelspace->GetNumberOrbits();
   TwoBodyME Mpp = Y.TwoBody;
   TwoBodyME Mhh = Y.TwoBody;

   int nch = modelspace->SortedTwoBodyChannels.size();
   #ifndef OPENBLAS_NOUSEOMP
   #pragma omp parallel for schedule(dynamic,1)
   #endif
   for (int ich=0; ich<nch; ++ich)
   {
      int ch = modelspace->SortedTwoBodyChannels[ich];
      TwoBodyChannel& tbc = modelspace->GetTwoBodyChannel(ch);

      auto& LHS = X.TwoBody.GetMatrix(ch,ch);
      auto& RHS = Y.TwoBody.GetMatrix(ch,ch);

      auto& Matrixpp = Mpp.GetMatrix(ch,ch);
      auto& Matrixhh = Mhh.GetMatrix(ch,ch);

      auto& kets_pp = tbc.GetKetIndex_pp();
      auto& kets_hh = tbc.GetKetIndex_hh();
      
      Matrixpp =  LHS.cols(kets_pp) * RHS.rows(kets_pp);
      Matrixhh =  LHS.cols(kets_hh) * RHS.rows(kets_hh);

      if (Z.IsHermitian())
      {
         Matrixpp +=  Matrixpp.t();
         Matrixhh +=  Matrixhh.t();
      }
      else if (Z.IsAntiHermitian()) // i.e. LHS and RHS are both hermitian or ant-hermitian
      {
         Matrixpp -=  Matrixpp.t();
         Matrixhh -=  Matrixhh.t();
      }
      else
      {
        Matrixpp -=  RHS.cols(kets_pp) * LHS.rows(kets_pp);
        Matrixhh -=  RHS.cols(kets_hh) * LHS.rows(kets_hh);
      }

      // The two body part
   } //for ch


   #pragma omp parallel for schedule(dynamic,1)
   for (int i=0;i<norbits;++i)
   {
      Orbit &oi = modelspace->GetOrbit(i);
      int jmin = Z.IsNonHermitian() ? 0 : i;
      for (int j : Z.OneBodyChannels.at({oi.l,oi.j2,oi.tz2}) )
      {
         if (j<jmin) continue;
         double cijJ = 0;
         for (int ch=0;ch<nChannels;++ch)
         {
            TwoBodyChannel& tbc = modelspace->GetTwoBodyChannel(ch);
            double Jfactor = (2*tbc.J+1.0);
            // Sum c over holes and include the nbar_a * nbar_b terms
//            for (auto& c : modelspace->holes)
            for (auto& it_c : modelspace->holes)
            {
               index_t c = it_c.first;
               double occ_c = it_c.second;
               cijJ += Jfactor * occ_c     * Mpp.GetTBME(ch,c,i,c,j);
               cijJ += Jfactor * (1-occ_c) * Mhh.GetTBME(ch,c,i,c,j);
            // Sum c over particles and include the n_a * n_b terms
            }
            for (auto& c : modelspace->particles)
            {
               cijJ += Jfactor * Mhh.GetTBME(ch,c,i,c,j);
            }
         }
         Z.OneBody(i,j) += cijJ /(oi.j2+1.0);
      } // for j
   }


}





//*****************************************************************************************
//
//    |     |               |      |           [X2,Y1](2) = sum_a ( Y_ia X_ajkl + Y_ja X_iakl - Y_ak X_ijal - Y_al X_ijka )
//    |     |___.Y          |__X___|         
//    |     |         _     |      |          
//    |_____|               |      |_____.Y        
//    |  X  |               |      |            
//
// -- AGREES WITH NATHAN'S RESULTS
/// Returns \f$ [X_{(1)},Y_{(2)}]_{(2)} - [Y_{(1)},X_{(2)}]_{(2)} \f$, where
/// \f[
/// [X_{(1)},Y_{(2)}]^{J}_{ijkl} = \sum_{a} ( X_{ia}Y^{J}_{ajkl} + X_{ja}Y^{J}_{iakl} - X_{ak} Y^{J}_{ijal} - X_{al} Y^{J}_{ijka} )
/// \f]
/// here, all TBME are unnormalized, i.e. they should have a tilde.
// This is still too slow...
//void Operator::comm122ss( Operator& Y, Operator& Z ) 
void Operator::comm122ss( const Operator& X, const Operator& Y ) 
{
   Operator& Z = *this;
   auto& X1 = X.OneBody;
   auto& Y1 = Y.OneBody;

   int n_nonzero = modelspace->SortedTwoBodyChannels.size();
   #pragma omp parallel for schedule(dynamic,1)
   for (int ich=0; ich<n_nonzero; ++ich)
   {
      int ch = modelspace->SortedTwoBodyChannels[ich];
      TwoBodyChannel& tbc = modelspace->GetTwoBodyChannel(ch);
      auto& X2 = X.TwoBody.GetMatrix(ch,ch);
      auto& Y2 = Y.TwoBody.GetMatrix(ch,ch);
      auto& Z2 = Z.TwoBody.GetMatrix(ch,ch);


      int npq = tbc.GetNumberKets();
      for (int indx_ij = 0;indx_ij<npq; ++indx_ij)
      {
         Ket & bra = tbc.GetKet(indx_ij);
         int i = bra.p;
         int j = bra.q;
         double pre_ij = i==j ? SQRT2 : 1;
         Orbit& oi = modelspace->GetOrbit(i);
         Orbit& oj = modelspace->GetOrbit(j);
         arma::Row<double> X2_ij = X2.row(indx_ij); // trying this to better use the cache. not sure if it helps.
         arma::Row<double> Y2_ij = Y2.row(indx_ij);
         int klmin = Z.IsNonHermitian() ? 0 : indx_ij;
         for (int indx_kl=klmin;indx_kl<npq; ++indx_kl)
         {
            Ket & ket = tbc.GetKet(indx_kl);
            int k = ket.p;
            int l = ket.q;
            double pre_kl = k==l ? SQRT2 : 1;
            Orbit& ok = modelspace->GetOrbit(k);
            Orbit& ol = modelspace->GetOrbit(l);
            arma::vec X2_kl = X2.unsafe_col(indx_kl);
            arma::vec Y2_kl = Y2.unsafe_col(indx_kl);

            double cijkl = 0;


            for (int a : modelspace->OneBodyChannels.at({oi.l,oi.j2,oi.tz2}) )
            {
                 int indx_aj = tbc.GetLocalIndex(min(a,j),max(a,j));
                 if (indx_aj < 0) continue;
                 double pre_aj = a>j ? tbc.GetKet(indx_aj).Phase(tbc.J) : (a==j ? SQRT2 : 1);
                 cijkl += pre_kl * pre_aj  * ( X1(i,a) * Y2(indx_aj,indx_kl) - Y1(i,a) * X2_kl(indx_aj) );
            }

            for (int a : modelspace->OneBodyChannels.at({oj.l,oj.j2,oj.tz2}) )
            {
                 int indx_ia = tbc.GetLocalIndex(min(i,a),max(i,a));
                 if (indx_ia < 0) continue;
                 double pre_ia = i>a ? tbc.GetKet(indx_ia).Phase(tbc.J) : (i==a ? SQRT2 : 1);
                 cijkl += pre_kl * pre_ia * ( X1(j,a) * Y2(indx_ia,indx_kl)  - Y1(j,a) * X2_kl(indx_ia) );
             }

            for (int a : modelspace->OneBodyChannels.at({ok.l,ok.j2,ok.tz2}) )
            {
                int indx_al = tbc.GetLocalIndex(min(a,l),max(a,l));
                if (indx_al < 0) continue;
                double pre_al = a>l ? tbc.GetKet(indx_al).Phase(tbc.J) : (a==l ? SQRT2 : 1);
                cijkl -= pre_ij * pre_al * ( X1(a,k) * Y2(indx_ij,indx_al) - Y1(a,k) * X2_ij(indx_al) );
            }

            for (int a : modelspace->OneBodyChannels.at({ol.l,ol.j2,ol.tz2}) )
            {
               int indx_ka = tbc.GetLocalIndex(min(k,a),max(k,a));
               if (indx_ka < 0) continue;
               double pre_ka = k>a ? tbc.GetKet(indx_ka).Phase(tbc.J) : (k==a ? SQRT2 : 1);
               cijkl -= pre_ij * pre_ka * ( X1(a,l) * Y2(indx_ij,indx_ka) - Y1(a,l) * X2_ij(indx_ka) );
            }

            double norm = bra.delta_pq()==ket.delta_pq() ? 1+bra.delta_pq() : SQRT2;
            Z2(indx_ij,indx_kl) += cijkl / norm;
         }
      }
   }

}





//*****************************************************************************************
//
//  |     |      |     |   
//  |__Y__|      |__x__|   [X2,Y2](2)_pp(hh) = 1/2 sum_ab (X_ijab Y_abkl - Y_ijab X_abkl)(1 - n_a - n_b)
//  |     |  _   |     |                = 1/2 [ X*(P_pp-P_hh)*Y - Y*(P_pp-P_hh)*X ]
//  |__X__|      |__Y__|   
//  |     |      |     |   
//
// -- AGREES WITH NATHAN'S RESULTS
//   No factor of 1/2 because the matrix multiplication corresponds to a restricted sum (a<=b) 
/// Calculates the part of the commutator \f$ [X_{(2)},Y_{(2)}]_{(2)} \f$ which involves particle-particle
/// or hole-hole intermediate states.
/// \f[
/// [X_{(2)},Y_{(2)}]^{J}_{ijkl} = \frac{1}{2} \sum_{ab} (\bar{n}_a\bar{n}_b - n_an_b) (X^{J}_{ijab}Y^{J}_{ablk} - Y^{J}_{ijab}X^{J}_{abkl})
/// \f]
/// This may be written as
/// \f[
/// [X_{(2)},Y_{(2)}]^{J} = \mathcal{M}^{J}_{pp} - \mathcal{M}^{J}_{hh}
/// \f]
/// With the intermediate matrices
/// \f[
/// \mathcal{M}^{J}_{pp} \equiv \frac{1}{2}(X^{J} \mathcal{P}_{pp} Y^{J} - Y^{J} \mathcal{P}_{pp} X^{J})
/// \f]
/// and likewise for \f$ \mathcal{M}^{J}_{hh} \f$.
//void Operator::comm222_pp_hhss( Operator& opright, Operator& opout ) 
void Operator::comm222_pp_hhss( const Operator& X, const Operator& Y ) 
{
   Operator& Z = *this;
//   #pragma omp parallel for schedule(dynamic,5)
   for (int ch=0; ch<nChannels; ++ch)
   {
      TwoBodyChannel& tbc = modelspace->GetTwoBodyChannel(ch);
      arma::mat& LHS = (arma::mat&) X.TwoBody.GetMatrix(ch,ch);
      arma::mat& RHS = (arma::mat&) Y.TwoBody.GetMatrix(ch,ch);
      arma::mat& OUT = (arma::mat&) Z.TwoBody.GetMatrix(ch,ch);

      arma::mat Mpp = (LHS.rows(tbc.GetKetIndex_pp()) * RHS.cols(tbc.GetKetIndex_pp()));
      arma::mat Mhh = (LHS.rows(tbc.GetKetIndex_hh()) * RHS.cols(tbc.GetKetIndex_hh()));
      OUT += Mpp - Mpp.t() - Mhh + Mhh.t();
   }
}







/// Since comm222_pp_hhss() and comm221ss() both require the ruction of 
/// the intermediate matrices \f$\mathcal{M}_{pp} \f$ and \f$ \mathcal{M}_{hh} \f$, we can combine them and
/// only calculate the intermediates once.
//void Operator::comm222_pp_hh_221ss( Operator& Y, Operator& Z )  
void Operator::comm222_pp_hh_221ss( const Operator& X, const Operator& Y )  
{

//   int herm = Z.IsHermitian() ? 1 : -1;
   Operator& Z = *this;
   int norbits = modelspace->GetNumberOrbits();

   static TwoBodyME Mpp = Z.TwoBody;
   static TwoBodyME Mhh = Z.TwoBody;
   static TwoBodyME Mff = Z.TwoBody;

   double t = omp_get_wtime();
   // Don't use omp, because the matrix multiplication is already
   // parallelized by armadillo.
   int nch = modelspace->SortedTwoBodyChannels.size();
   #ifndef OPENBLAS_NOUSEOMP
   #pragma omp parallel for schedule(dynamic,1)
   #endif
   for (int ich=0; ich<nch; ++ich)
   {
      int ch = modelspace->SortedTwoBodyChannels[ich];
      TwoBodyChannel& tbc = modelspace->GetTwoBodyChannel(ch);

      auto& LHS = X.TwoBody.GetMatrix(ch,ch);
      auto& RHS = Y.TwoBody.GetMatrix(ch,ch);
      auto& OUT = Z.TwoBody.GetMatrix(ch,ch);

      auto& Matrixpp = Mpp.GetMatrix(ch,ch);
      auto& Matrixhh = Mhh.GetMatrix(ch,ch);
      auto& Matrixff = Mff.GetMatrix(ch,ch);

      auto& kets_pp = tbc.GetKetIndex_pp();
      auto& kets_hh = tbc.GetKetIndex_hh();
      auto& nanb = tbc.Ket_occ_hh;
      auto& nabar_nbbar = tbc.Ket_unocc_hh;
      
      Matrixpp =  LHS.cols(kets_pp) * RHS.rows(kets_pp);
      Matrixhh =  LHS.cols(kets_hh) * arma::diagmat(nanb) *  RHS.rows(kets_hh) ;
      Matrixff =  LHS.cols(kets_hh) * arma::diagmat(nabar_nbbar) *  RHS.rows(kets_hh) ;
//      Matrixhh =  LHS.cols(kets_hh) * ( RHS.rows(kets_hh).each_col() % nanb );
//      Matrixff =  LHS.cols(kets_hh) * ( RHS.rows(kets_hh).each_col() % nabar_nbbar); // 

      if (Z.IsHermitian())
      {
         Matrixpp +=  Matrixpp.t();
         Matrixhh +=  Matrixhh.t();
         Matrixff +=  Matrixff.t();
      }
      else if (Z.IsAntiHermitian()) // i.e. LHS and RHS are both hermitian or ant-hermitian
      {
         Matrixpp -=  Matrixpp.t();
         Matrixhh -=  Matrixhh.t();
         Matrixff +=  Matrixff.t();
      }
      else
      {
        Matrixpp -=  RHS.cols(kets_pp) * LHS.rows(kets_pp);
//        Matrixhh -=  RHS.cols(kets_hh) * ( LHS.rows(kets_hh).each_col() % nanb );
//        Matrixff -=  RHS.cols(kets_hh) * ( LHS.rows(kets_hh).each_col() % nabar_nbbar );
        Matrixhh =  RHS.cols(kets_hh) * arma::diagmat(nanb) *  LHS.rows(kets_hh) ;
        Matrixff =  RHS.cols(kets_hh) * arma::diagmat(nabar_nbbar) *  LHS.rows(kets_hh) ;
      }

      // The two body part
//      OUT += Matrixpp - Matrixhh;
      OUT += Matrixpp + Matrixff - Matrixhh;
   } //for ch
   profiler.timer["pphh TwoBody bit"] += omp_get_wtime() - t;

   t = omp_get_wtime();
   // The one body part
   #pragma omp parallel for schedule(dynamic,1)
   for (int i=0;i<norbits;++i)
   {
      Orbit &oi = modelspace->GetOrbit(i);
      int jmin = Z.IsNonHermitian() ? 0 : i;
      for (int j : Z.OneBodyChannels.at({oi.l,oi.j2,oi.tz2}) )
      {
         if (j<jmin) continue;
         double cijJ = 0;
         for (int ch=0;ch<nChannels;++ch)
         {
            TwoBodyChannel& tbc = modelspace->GetTwoBodyChannel(ch);
            double Jfactor = (2*tbc.J+1.0);
            // Sum c over holes and include the nbar_a * nbar_b terms
//            for (auto& c : modelspace->holes)
            for (auto& it_c : modelspace->holes)
            {
               index_t c = it_c.first;
               double occ_c = it_c.second;
               cijJ += Jfactor * occ_c * Mpp.GetTBME(ch,c,i,c,j); 
               cijJ += Jfactor * (1-occ_c) * Mff.GetTBME(ch,c,i,c,j);
            // Sum c over particles and include the n_a * n_b terms
            }
            for (auto& c : modelspace->particles)
            {
               cijJ += Jfactor * Mhh.GetTBME(ch,c,i,c,j);
            }
         }
         Z.OneBody(i,j) += cijJ /(oi.j2+1.0);
      } // for j
   } // for i
   profiler.timer["pphh One Body bit"] += omp_get_wtime() - t;
}



//**************************************************************************
//
//  X^J_ij`kl` = - sum_J' { i j J } (2J'+1) X^J'_ilkj
//                        { k l J'}
// SCALAR VARIETY
/// The scalar Pandya transformation is defined as
/// \f[
///  \bar{X}^{J}_{i\bar{j}k\bar{l}} = - \sum_{J'} (2J'+1)
///  \left\{ \begin{array}{lll}
///  j_i  &  j_j  &  J \\
///  j_k  &  j_l  &  J' \\
///  \end{array} \right\}
///  X^{J}_{ilkj}
/// \f]
/// where the overbar indicates time-reversed orbits.
/// This function is designed for use with comm222_phss() and so it takes in
/// two arrays of matrices, one for hp terms and one for ph terms.
//void Operator::DoPandyaTransformation(TwoBodyME& TwoBody_CC_hp, TwoBodyME& TwoBody_CC_ph)
//void Operator::DoPandyaTransformation(vector<arma::mat>& TwoBody_CC_hp, vector<arma::mat>& TwoBody_CC_ph, string orientation="normal") const
//void Operator::DoPandyaTransformation(deque<arma::mat>& TwoBody_CC_hp, deque<arma::mat>& TwoBody_CC_ph, string orientation="normal") const
void Operator::DoPandyaTransformation(deque<arma::mat>& TwoBody_CC_ph, string orientation="normal") const
{
   // loop over cross-coupled channels
   int n_nonzero = modelspace->SortedTwoBodyChannels_CC.size();
   int herm = IsHermitian() ? 1 : -1;
   #pragma omp parallel for schedule(dynamic,1) if (not modelspace->SixJ_is_empty())
   for (int ich=0; ich<n_nonzero; ++ich)
   {
      int ch_cc = modelspace->SortedTwoBodyChannels_CC[ich];
      TwoBodyChannel& tbc_cc = modelspace->GetTwoBodyChannel_CC(ch_cc);
      int nKets_cc = tbc_cc.GetNumberKets();
      arma::uvec& kets_ph = tbc_cc.GetKetIndex_ph();
      int nph_kets = kets_ph.n_rows;
      int J_cc = tbc_cc.J;

      // loop over cross-coupled ph bras <ab| in this channel
      // (this is the side that gets summed over)
      for (int ibra=0; ibra<nph_kets; ++ibra)
      {
         Ket & bra_cc = tbc_cc.GetKet( kets_ph[ibra] );
         int a = bra_cc.p;
         int b = bra_cc.q;
         Orbit & oa = modelspace->GetOrbit(a);
         Orbit & ob = modelspace->GetOrbit(b);
         double ja = oa.j2*0.5;
         double jb = ob.j2*0.5;
         double na_nb_factor = oa.occ - ob.occ;

         // loop over cross-coupled kets |cd> in this channel
         // we go to 2*nKets to include |cd> and |dc>
         for (int iket_cc=0; iket_cc<nKets_cc; ++iket_cc)
         {
            Ket & ket_cc = tbc_cc.GetKet(iket_cc%nKets_cc);
            int c = iket_cc < nKets_cc ? ket_cc.p : ket_cc.q;
            int d = iket_cc < nKets_cc ? ket_cc.q : ket_cc.p;
            Orbit & oc = modelspace->GetOrbit(c);
            Orbit & od = modelspace->GetOrbit(d);
            double jc = oc.j2*0.5;
            double jd = od.j2*0.5;


            int jmin = max(abs(ja-jd),abs(jc-jb));
            int jmax = min(ja+jd,jc+jb);
            double sm = 0;
            for (int J_std=jmin; J_std<=jmax; ++J_std)
            {
               double sixj = modelspace->GetSixJ(ja,jb,J_cc,jc,jd,J_std);
               if (abs(sixj) < 1e-8) continue;
               double tbme = TwoBody.GetTBME_J(J_std,a,d,c,b);
               sm -= (2*J_std+1) * sixj * tbme ;
            }
            // Xabij = sm
            // Xbaji = hx * (-1)**(a+b+i+j) Xabij
            if (orientation=="normal")
            {
              TwoBody_CC_ph[ch_cc](ibra,iket_cc) = sm;
              TwoBody_CC_ph[ch_cc](ibra+nph_kets,iket_cc+nKets_cc) = herm* modelspace->phase(ja+jb+jc+jd) * sm;
//              TwoBody_CC_hp[ch_cc](ibra,iket_cc) = sm;
//              TwoBody_CC_ph[ch_cc](ibra,iket_cc+nKets_cc) = herm* modelspace->phase(ja+jb+jc+jd) * sm;
            }
            // Xijab = hx * Xabij
            // Xjiba = (-1)**(a+b+i+j) Xabij
            else if (orientation=="transpose")
            {

              TwoBody_CC_ph[ch_cc](iket_cc,ibra) = herm * sm * na_nb_factor;
              TwoBody_CC_ph[ch_cc](iket_cc+nKets_cc,ibra+nph_kets) =  modelspace->phase(ja+jb+jc+jd) * sm * -na_nb_factor;
//              TwoBody_CC_hp[ch_cc](iket_cc,ibra) = herm * sm * na_nb_factor;
//              TwoBody_CC_ph[ch_cc](iket_cc+nKets_cc,ibra) =  modelspace->phase(ja+jb+jc+jd) * sm * -na_nb_factor;
            }

            // Exchange (a <-> b) to account for the (n_a - n_b) term
            // Get Tz,parity and range of J for <bd || ca > coupling
            jmin = max(abs(jb-jd),abs(jc-ja));
            jmax = min(jb+jd,jc+ja);
            sm = 0;
            for (int J_std=jmin; J_std<=jmax; ++J_std)
            {
               double sixj = modelspace->GetSixJ(jb,ja,J_cc,jc,jd,J_std);
               if (abs(sixj) < 1e-8) continue;
               double tbme = TwoBody.GetTBME_J(J_std,b,d,c,a);
               sm -= (2*J_std+1) * sixj * tbme ;
            }
            // Xbaij = sm
            // Xabji = hx * (-1)**(a+b+i+j) Xbaij
            if (orientation=="normal")
            {
              TwoBody_CC_ph[ch_cc](ibra+nph_kets,iket_cc) = sm;
              TwoBody_CC_ph[ch_cc](ibra,iket_cc+nKets_cc) = herm* modelspace->phase(ja+jb+jc+jd) * sm;
//              TwoBody_CC_ph[ch_cc](ibra,iket_cc) = sm;
//              TwoBody_CC_hp[ch_cc](ibra,iket_cc+nKets_cc) = herm* modelspace->phase(ja+jb+jc+jd) * sm;
            }
            // Xijba = hx * Xbaij
            // Xjiab = (-1)**(a+b+i+j) * Xbaij
            else if (orientation=="transpose")
            {
              TwoBody_CC_ph[ch_cc](iket_cc,ibra+nph_kets) = herm * sm * -na_nb_factor;
              TwoBody_CC_ph[ch_cc](iket_cc+nKets_cc,ibra) =  modelspace->phase(ja+jb+jc+jd) * sm * na_nb_factor;
//              TwoBody_CC_ph[ch_cc](iket_cc,ibra) = herm * sm * -na_nb_factor;
//              TwoBody_CC_hp[ch_cc](iket_cc+nKets_cc,ibra) =  modelspace->phase(ja+jb+jc+jd) * sm * na_nb_factor;
            }

         }
      }
   }
}



//void Operator::InversePandyaTransformation(vector<arma::mat>& W, vector<arma::mat>& opout)
//void Operator::AddInversePandyaTransformation(vector<arma::mat>& Zbar)
void Operator::AddInversePandyaTransformation(deque<arma::mat>& Zbar)
{
    // Do the inverse Pandya transform
    // Only go parallel if we've previously calculated the SixJ's. Otherwise, it's not thread safe.
   int n_nonzeroChannels = modelspace->SortedTwoBodyChannels.size();
   #pragma omp parallel for schedule(dynamic,1) if (not modelspace->SixJ_is_empty())
   for (int ich = 0; ich < n_nonzeroChannels; ++ich)
   {
      int ch = modelspace->SortedTwoBodyChannels[ich];
      TwoBodyChannel& tbc = modelspace->GetTwoBodyChannel(ch);
      int J = tbc.J;
      int nKets = tbc.GetNumberKets();

      for (int ibra=0; ibra<nKets; ++ibra)
      {
         Ket & bra = tbc.GetKet(ibra);
         int i = bra.p;
         int j = bra.q;
         Orbit & oi = modelspace->GetOrbit(i);
         Orbit & oj = modelspace->GetOrbit(j);
         double ji = oi.j2/2.;
         double jj = oj.j2/2.;
         int ketmin = IsHermitian() ? ibra : ibra+1;
         for (int iket=ketmin; iket<nKets; ++iket)
         {
            Ket & ket = tbc.GetKet(iket);
            int k = ket.p;
            int l = ket.q;
            Orbit & ok = modelspace->GetOrbit(k);
            Orbit & ol = modelspace->GetOrbit(l);
            double jk = ok.j2/2.;
            double jl = ol.j2/2.;

            double commij = 0;
            double commji = 0;

            int parity_cc = (oi.l+ol.l)%2;
            int Tz_cc = abs(oi.tz2+ol.tz2)/2;
            int jmin = max(abs(int(ji-jl)),abs(int(jk-jj)));
            int jmax = min(int(ji+jl),int(jk+jj));
            for (int Jprime=jmin; Jprime<=jmax; ++Jprime)
            {
               double sixj = modelspace->GetSixJ(ji,jj,J,jk,jl,Jprime);
               if (abs(sixj)<1e-8) continue;
               int ch_cc = modelspace->GetTwoBodyChannelIndex(Jprime,parity_cc,Tz_cc);
               TwoBodyChannel_CC& tbc_cc = modelspace->GetTwoBodyChannel_CC(ch_cc);
               int indx_il = tbc_cc.GetLocalIndex(min(i,l),max(i,l));
               int indx_kj = tbc_cc.GetLocalIndex(min(j,k),max(j,k));
               if (i>l) indx_il += tbc_cc.GetNumberKets();
               if (k>j) indx_kj += tbc_cc.GetNumberKets();
               double me1 = Zbar[ch_cc](indx_il,indx_kj);
               commij += (2*Jprime+1) * sixj * me1;

            }

            // now loop over the cross coupled TBME's
            parity_cc = (oi.l+ok.l)%2;
            Tz_cc = abs(oi.tz2+ok.tz2)/2;
            jmin = max(abs(int(jj-jl)),abs(int(jk-ji)));
            jmax = min(int(jj+jl),int(jk+ji));
            for (int Jprime=jmin; Jprime<=jmax; ++Jprime)
            {
               double sixj = modelspace->GetSixJ(jj,ji,J,jk,jl,Jprime);
               if (abs(sixj)<1e-8) continue;
               int ch_cc = modelspace->GetTwoBodyChannelIndex(Jprime,parity_cc,Tz_cc);
               TwoBodyChannel_CC& tbc_cc = modelspace->GetTwoBodyChannel_CC(ch_cc);
               int indx_jl = tbc_cc.GetLocalIndex(min(j,l),max(j,l));
               int indx_ki = tbc_cc.GetLocalIndex(min(i,k),max(i,k));
               if (j>l) indx_jl += tbc_cc.GetNumberKets();
               if (k>i) indx_ki += tbc_cc.GetNumberKets();
               double me1 = Zbar[ch_cc](indx_jl,indx_ki);
               commji += (2*Jprime+1) *  sixj * me1;

            }

            double norm = bra.delta_pq()==ket.delta_pq() ? 1+bra.delta_pq() : SQRT2;
            TwoBody.GetMatrix(ch,ch)(ibra,iket) += (commij - modelspace->phase(ji+jj-J)*commji) / norm;
         }
      }
   }
 
}


deque<arma::mat> Operator::InitializePandya(size_t nch, string orientation="normal")
{
   deque<arma::mat> X(nch);
   int n_nonzero = modelspace->SortedTwoBodyChannels_CC.size();
   for (int ich=0; ich<n_nonzero; ++ich)
   {
      int ch_cc = modelspace->SortedTwoBodyChannels_CC[ich];
      TwoBodyChannel& tbc_cc = modelspace->GetTwoBodyChannel_CC(ch_cc);
      int nKets_cc = tbc_cc.GetNumberKets();
      arma::uvec& kets_ph = tbc_cc.GetKetIndex_ph();
      int nph_kets = kets_ph.n_rows;
      if (orientation=="normal")
         X[ch_cc] = arma::mat(2*nph_kets,   2*nKets_cc, arma::fill::zeros);
//         X[ch_cc] = arma::mat(nph_kets,   2*nKets_cc, arma::fill::zeros);
      else if (orientation=="transpose")
         X[ch_cc] = arma::mat(2*nKets_cc, 2*nph_kets,   arma::fill::zeros);
//         X[ch_cc] = arma::mat(2*nKets_cc, nph_kets,   arma::fill::zeros);
   }
   return X;
}

//*****************************************************************************************
//
//  THIS IS THE BIG UGLY ONE.     
//                                             
//   |          |      |          |           
//   |     __Y__|      |     __X__|            
//   |    /\    |      |    /\    |
//   |   (  )   |  _   |   (  )   |            
//   |____\/    |      |____\/    |            
//   |  X       |      |  Y       |            
//           
//            
// -- This appears to agree with Nathan's results
//
/// Calculates the part of \f$ [X_{(2)},Y_{(2)}]_{ijkl} \f$ which involves ph intermediate states, here indicated by \f$ Z^{J}_{ijkl} \f$
/// \f[
/// Z^{J}_{ijkl} = \sum_{ab}(n_a\bar{n}_b-\bar{n}_an_b)\sum_{J'} (2J'+1)
/// \left[
///  \left\{ \begin{array}{lll}
///  j_i  &  j_j  &  J \\
///  j_k  &  j_l  &  J' \\
///  \end{array} \right\}
/// \left( \bar{X}^{J'}_{i\bar{l}a\bar{b}}\bar{Y}^{J'}_{a\bar{b}k\bar{j}} - 
///   \bar{Y}^{J'}_{i\bar{l}a\bar{b}}\bar{X}^{J'}_{a\bar{b}k\bar{j}} \right)
///  -(-1)^{j_i+j_j-J}
///  \left\{ \begin{array}{lll}
///  j_j  &  j_i  &  J \\
///  j_k  &  j_l  &  J' \\
///  \end{array} \right\}
/// \left( \bar{X}^{J'}_{j\bar{l}a\bar{b}}\bar{Y}^{J'}_{a\bar{b}k\bar{i}} - 
///   \bar{Y}^{J'}_{j\bar{l}a\bar{b}}\bar{X}^{J'}_{a\bar{b}k\bar{i}} \right)
/// \right]
/// \f]
/// This is implemented by defining an intermediate matrix
/// \f[
/// \bar{Z}^{J}_{i\bar{l}k\bar{j}} \equiv \sum_{ab}(n_a\bar{n}_b)
/// \left[ \left( \bar{X}^{J'}_{i\bar{l}a\bar{b}}\bar{Y}^{J'}_{a\bar{b}k\bar{j}} - 
///   \bar{Y}^{J'}_{i\bar{l}a\bar{b}}\bar{X}^{J'}_{a\bar{b}k\bar{j}} \right)
/// -\left( \bar{X}^{J'}_{i\bar{l}b\bar{a}}\bar{Y}^{J'}_{b\bar{a}k\bar{j}} - 
///    \bar{Y}^{J'}_{i\bar{l}b\bar{a}}\bar{X}^{J'}_{b\bar{a}k\bar{j}} \right)\right]
/// \f]
/// The Pandya-transformed matrix elements are obtained with DoPandyaTransformation().
/// The matrices \f$ \bar{X}^{J'}_{i\bar{l}a\bar{b}}\bar{Y}^{J'}_{a\bar{b}k\bar{j}} \f$
/// and \f$ \bar{Y}^{J'}_{i\bar{l}a\bar{b}}\bar{X}^{J'}_{a\bar{b}k\bar{j}} \f$
/// are related by a Hermitian conjugation, which saves two matrix multiplications.
/// The commutator is then given by
/// \f[
/// Z^{J}_{ijkl} = \sum_{J'} (2J'+1)
/// \left[
///  \left\{ \begin{array}{lll}
///  j_i  &  j_j  &  J \\
///  j_k  &  j_l  &  J' \\
///  \end{array} \right\}
///  \bar{Z}^{J'}_{i\bar{l}k\bar{j}}
///  -(-1)^{j_i+j_j-J}
///  \left\{ \begin{array}{lll}
///  j_j  &  j_i  &  J \\
///  j_k  &  j_l  &  J' \\
///  \end{array} \right\}
///  \bar{Z}^{J'}_{j\bar{l}k\bar{i}}
///  \right]
///  \f]
///
void Operator::comm222_phss( const Operator& X, const Operator& Y ) 
{

   Operator& Z = *this;
   // Create Pandya-transformed hp and ph matrix elements
   deque<arma::mat> Y_bar_ph (InitializePandya( nChannels, "normal"));
   deque<arma::mat> Xt_bar_ph (InitializePandya( nChannels, "transpose"));

   double t_start = omp_get_wtime();
   Y.DoPandyaTransformation(Y_bar_ph, "normal" );
   X.DoPandyaTransformation(Xt_bar_ph ,"transpose");
   profiler.timer["DoPandyaTransformation"] += omp_get_wtime() - t_start;

   // Construct the intermediate matrix Z_bar
   t_start = omp_get_wtime();
   deque<arma::mat> Z_bar (nChannels );

   int nch = modelspace->SortedTwoBodyChannels_CC.size();
   #ifndef OPENBLAS_NOUSEOMP
   #pragma omp parallel for schedule(dynamic,1)
   #endif
   for (int ich=0; ich<nch; ++ich )
   {
      int ch = modelspace->SortedTwoBodyChannels_CC[ich];

      Z_bar[ch] =  (Xt_bar_ph[ch] * Y_bar_ph[ch]);
      // If Z is hermitian, then XY is anti-hermitian, and so XY - YX = XY + (XY)^T
      if ( Z.IsHermitian() )
         Z_bar[ch] += Z_bar[ch].t();
      else
         Z_bar[ch] -= Z_bar[ch].t();
   }
   profiler.timer["Build Z_bar"] += omp_get_wtime() - t_start;

   // Perform inverse Pandya transform on Z_bar to get Z
   t_start = omp_get_wtime();
   Z.AddInversePandyaTransformation(Z_bar);
   profiler.timer["InversePandyaTransformation"] += omp_get_wtime() - t_start;

}






//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
////////////   BEGIN SCALAR-TENSOR COMMUTATORS      //////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////


//*****************************************************************************************
//
//        |____. Y          |___.X
//        |        _        |
//  X .___|            Y.___|              [X1,Y1](1)  =  XY - YX
//        |                 |
//
// This is no different from the scalar-scalar version
void Operator::comm111st( const Operator & X, const Operator& Y)
{
   double tstart = omp_get_wtime();
   comm111ss(X,Y);
   profiler.timer["comm111st"] += omp_get_wtime() - tstart;
}


//*****************************************************************************************
//                                       |
//      i |              i |             |
//        |    ___.Y       |__X__        |
//        |___(_)    _     |   (_)__.    |  [X2,Y1](1)  =  1/(2j_i+1) sum_ab(n_a-n_b)y_ab 
//      j | X            j |        Y    |        * sum_J (2J+1) x_biaj^(J)  
//                                       |      
//---------------------------------------*        = 1/(2j+1) sum_a n_a sum_J (2J+1)
//                                                  * sum_b y_ab x_biaj - yba x_aibj
//
// X is scalar one-body, Y is tensor two-body
// There must be a better way to do this looping. 
//
//void Operator::comm121st( Operator& Y, Operator& Z) 
void Operator::comm121st( const Operator& X, const Operator& Y) 
{

   double tstart = omp_get_wtime();
   Operator& Z = *this;
   int norbits = modelspace->GetNumberOrbits();
   int Lambda = Z.GetJRank();
//   #pragma omp parallel for // for starters, don't do it parallel
   for (int i=0;i<norbits;++i)
   {
      Orbit &oi = modelspace->GetOrbit(i);
      double ji = oi.j2/2.0;
      for (int j : Z.OneBodyChannels.at({oi.l,oi.j2,oi.tz2}) ) 
      {
          Orbit &oj = modelspace->GetOrbit(j);
          double jj = oj.j2/2.0;
          if (j<i) continue; // only calculate upper triangle
          double& Zij = Z.OneBody(i,j);
//          for (auto& a : modelspace->holes)  // C++11 syntax
          for (auto& it_a : modelspace->holes)  // C++11 syntax
          {
             index_t a = it_a.first;
             double occ_a = it_a.second;
             Orbit &oa = modelspace->GetOrbit(a);
             double ja = oa.j2/2.0;
             for (auto& b : X.OneBodyChannels.at({oa.l,oa.j2,oa.tz2}) ) 
             {
                Orbit &ob = modelspace->GetOrbit(b);
                double nanb = occ_a * (1-ob.occ);
                  int J1min = abs(ji-ja);
                  int J1max = ji + ja;
                  for (int J1=J1min; J1<=J1max; ++J1)
                  {
                    int phasefactor = modelspace->phase(jj+ja+J1+Lambda);
                    int J2min = max(abs(Lambda - J1),abs(int(ja-jj)));
                    int J2max = min(Lambda + J1,int(ja+jj));
                    for (int J2=J2min; J2<=J2max; ++J2)
                    {
                      if ( ! ( J2>=abs(ja-jj) and J2<=ja+jj )) continue;
                      double prefactor = nanb*phasefactor * sqrt((2*J1+1)*(2*J2+1)) * modelspace->GetSixJ(J1,J2,Lambda,jj,ji,ja);
                      Zij +=  prefactor * ( X.OneBody(a,b) * Y.TwoBody.GetTBME_J(J1,J2,b,i,a,j) - X.OneBody(b,a) * Y.TwoBody.GetTBME_J(J1,J2,a,i,b,j ));
                    }
                  }
             }
             // Now, X is scalar two-body and Y is tensor one-body
             for (auto& b : Y.OneBodyChannels.at({oa.l,oa.j2,oa.tz2}) ) // is this is slow, it can probably be sped up by looping over OneBodyChannels
             {

                Orbit &ob = modelspace->GetOrbit(b);
                double jb = ob.j2/2.0;
                if (abs(ob.occ-1) < 1e-6) continue;
                double nanb = occ_a * (1-ob.occ);
                int J1min = max(abs(ji-jb),abs(jj-ja));
                int J1max = min(ji+jb,jj+ja);
                double zij = 0;
                for (int J1=J1min; J1<=J1max; ++J1)
                {
                  zij -= modelspace->phase(ji+jb+J1) * (2*J1+1) * modelspace->GetSixJ(ja,jb,Lambda,ji,jj,J1) * X.TwoBody.GetTBME_J(J1,J1,b,i,a,j);
                }

                J1min = max(abs(ji-ja),abs(jj-jb));
                J1max = min(ji+ja,jj+jb);
                for (int J1=J1min; J1<=J1max; ++J1)
                {
                  zij += modelspace->phase(ji+jb+J1) * (2*J1+1) * modelspace->GetSixJ(jb,ja,Lambda,ji,jj,J1) * X.TwoBody.GetTBME_J(J1,J1,a,i,b,j) ;
                }

                Zij += nanb * Y.OneBody(a,b) * zij;
             }
             
          }
      }
   }
   
   profiler.timer["comm121st"] += omp_get_wtime() - tstart;
}




//*****************************************************************************************
//
//    |     |               |      |           [X2,Y1](2) = sum_a ( Y_ia X_ajkl + Y_ja X_iakl - Y_ak X_ijal - Y_al X_ijka )
//    |     |___.Y          |__X___|         
//    |     |         _     |      |          
//    |_____|               |      |_____.Y        
//    |  X  |               |      |            
//
// -- AGREES WITH NATHAN'S RESULTS
// Right now, this is the slowest one...
// Agrees with previous code in the scalar-scalar limit
//void Operator::comm122st( Operator& Y, Operator& Z ) 
void Operator::comm122st( const Operator& X, const Operator& Y ) 
{
   double tstart = omp_get_wtime();
   Operator& Z = *this;
   int Lambda = Z.rank_J;

    vector< array<int,2> > channels;
    for ( auto& itmat : Z.TwoBody.MatEl ) channels.push_back( itmat.first );
    int nmat = channels.size();
   #pragma omp parallel for schedule(dynamic,1)
    for (int ii=0; ii<nmat; ++ii)
    {
     int ch_bra = channels[ii][0];
     int ch_ket = channels[ii][1];

      TwoBodyChannel& tbc_bra = modelspace->GetTwoBodyChannel(ch_bra);
      TwoBodyChannel& tbc_ket = modelspace->GetTwoBodyChannel(ch_ket);
      int J1 = tbc_bra.J;
      int J2 = tbc_ket.J;
      int nbras = tbc_bra.GetNumberKets();
      int nkets = tbc_ket.GetNumberKets();
      double hatfactor = sqrt((2*J1+1)*(2*J2+1));
      arma::mat& Z2 = Z.TwoBody.GetMatrix(ch_bra,ch_ket);

      for (int ibra = 0;ibra<nbras; ++ibra)
      {
         Ket & bra = tbc_bra.GetKet(ibra);
         int i = bra.p;
         int j = bra.q;
         Orbit& oi = modelspace->GetOrbit(i);
         Orbit& oj = modelspace->GetOrbit(j);
         double ji = oi.j2/2.0;
         double jj = oj.j2/2.0;
//         for (int iket=ibra;iket<nkets; ++iket)
         for (int iket=0;iket<nkets; ++iket)
         {
            Ket & ket = tbc_ket.GetKet(iket);
            int k = ket.p;
            int l = ket.q;
            Orbit& ok = modelspace->GetOrbit(k);
            Orbit& ol = modelspace->GetOrbit(l);
            double jk = ok.j2/2.0;
            double jl = ol.j2/2.0;

            double cijkl = 0;

            for ( int a : X.OneBodyChannels.at({oi.l,oi.j2,oi.tz2}) )
            {
              cijkl += X.OneBody(i,a) * Y.TwoBody.GetTBME(ch_bra,ch_ket,a,j,k,l);
            }
            for ( int a : X.OneBodyChannels.at({oj.l,oj.j2,oj.tz2}) )
            {
               cijkl += X.OneBody(j,a) * Y.TwoBody.GetTBME(ch_bra,ch_ket,i,a,k,l);
            }
            for ( int a : X.OneBodyChannels.at({ok.l,ok.j2,ok.tz2}) )
            {
               cijkl -= X.OneBody(a,k) * Y.TwoBody.GetTBME(ch_bra,ch_ket,i,j,a,l);
            }
            for ( int a : X.OneBodyChannels.at({ol.l,ol.j2,ol.tz2}) )
            {
               cijkl -= X.OneBody(a,l) * Y.TwoBody.GetTBME(ch_bra,ch_ket,i,j,k,a);
            }


            double prefactor = hatfactor * modelspace->phase(ji+jj+J2+Lambda) ;
            for ( int a : Y.OneBodyChannels.at({oi.l,oi.j2,oi.tz2}) )
            {
               double ja = modelspace->GetOrbit(a).j2*0.5;
               cijkl -= prefactor *  modelspace->GetSixJ(J2,J1,Lambda,ji,ja,jj) * Y.OneBody(i,a) * X.TwoBody.GetTBME(ch_ket,ch_ket,a,j,k,l) ;
            }
            for ( int a : Y.OneBodyChannels.at({oj.l,oj.j2,oj.tz2}) )
            {
               double ja = modelspace->GetOrbit(a).j2*0.5;
               prefactor = hatfactor * modelspace->phase(ji+ja-J1+Lambda) ;
               cijkl -= prefactor * modelspace->GetSixJ(J2,J1,Lambda,jj,ja,ji) * Y.OneBody(j,a) * X.TwoBody.GetTBME(ch_ket,ch_ket,i,a,k,l);
            }
            for ( int a : Y.OneBodyChannels.at({ok.l,ok.j2,ok.tz2}) )
            {
               double ja = modelspace->GetOrbit(a).j2*0.5;
               prefactor = hatfactor * modelspace->phase(ja+jl+J2+Lambda) ;
               cijkl += prefactor * modelspace->GetSixJ(J1,J2,Lambda,jk,ja,jl) * Y.OneBody(a,k) * X.TwoBody.GetTBME(ch_bra,ch_bra,i,j,a,l) ;
            }
            prefactor = hatfactor * modelspace->phase(jk+jl-J1+Lambda) ;
            for ( int a : Y.OneBodyChannels.at({ol.l,ol.j2,ol.tz2}) )
            {
               double ja = modelspace->GetOrbit(a).j2*0.5;
               cijkl += prefactor * modelspace->GetSixJ(J1,J2,Lambda,jl,ja,jk) * Y.OneBody(a,l) * X.TwoBody.GetTBME(ch_bra,ch_bra,i,j,k,a) ;
            }


            double norm = bra.delta_pq()==ket.delta_pq() ? 1+bra.delta_pq() : SQRT2;
            Z2(ibra,iket) += cijkl /norm;
         }
      }
   }
//   int chbra = modelspace->GetTwoBodyChannelIndex(0,0,-1);
//   int chket = modelspace->GetTwoBodyChannelIndex(2,0,-1);
//   int ibra = modelspace->GetTwoBodyChannel(chbra).GetLocalIndex(0,0);
//   int iket = modelspace->GetTwoBodyChannel(chket).GetLocalIndex(2,2);
   profiler.timer["comm122st"] += omp_get_wtime() - tstart;
}





// Since comm222_pp_hh and comm211 both require the construction of 
// the intermediate matrices Mpp and Mhh, we can combine them and
// only calculate the intermediates once.
// X is a scalar, Y is a tensor
//void Operator::comm222_pp_hh_221st( Operator& Y, Operator& Z )  
void Operator::comm222_pp_hh_221st( const Operator& X, const Operator& Y )  
{

   double tstart = omp_get_wtime();
   Operator& Z = *this;
   int Lambda = Z.GetJRank();

   TwoBodyME Mpp = Z.TwoBody;
   TwoBodyME Mhh = Z.TwoBody;
   TwoBodyME Mff = Z.TwoBody;

   vector<int> vch_bra;
   vector<int> vch_ket;
   vector<const arma::mat*> vmtx;
   for ( auto& itmat : Y.TwoBody.MatEl )
   {
     vch_bra.push_back(itmat.first[0]);
     vch_ket.push_back(itmat.first[1]);
     vmtx.push_back(&(itmat.second));
   }
   size_t nchan = vch_bra.size();
//   for ( auto& itmat : Y.TwoBody.MatEl )
//   #pragma omp parallel for schedule(dynamic,1)
   for (size_t i=0;i<nchan; ++i)
   {
//    int ch_bra = itmat.first[0];
//    int ch_ket = itmat.first[1];
    int ch_bra = vch_bra[i];
    int ch_ket = vch_ket[i];

    TwoBodyChannel& tbc_bra = modelspace->GetTwoBodyChannel(ch_bra);
    TwoBodyChannel& tbc_ket = modelspace->GetTwoBodyChannel(ch_ket);

    auto& LHS1 = X.TwoBody.GetMatrix(ch_bra,ch_bra);
    auto& LHS2 = X.TwoBody.GetMatrix(ch_ket,ch_ket);

//    auto& RHS  =  itmat.second;
    auto& RHS  =  *vmtx[i];
    arma::mat& OUT2 =    Z.TwoBody.GetMatrix(ch_bra,ch_ket);

    arma::mat& Matrixpp =  Mpp.GetMatrix(ch_bra,ch_ket);
    arma::mat& Matrixhh =  Mhh.GetMatrix(ch_bra,ch_ket);
    arma::mat& Matrixff =  Mff.GetMatrix(ch_bra,ch_ket);
   
    arma::uvec& bras_pp = tbc_bra.GetKetIndex_pp();
    arma::uvec& bras_hh = tbc_bra.GetKetIndex_hh();
    arma::uvec& kets_pp = tbc_ket.GetKetIndex_pp();
    arma::uvec& kets_hh = tbc_ket.GetKetIndex_hh();

    auto& nanb_bra = tbc_bra.Ket_occ_hh;
    auto& nanb_ket = tbc_ket.Ket_occ_hh;
    auto& nabarnbbar_bra = tbc_bra.Ket_unocc_hh;
    auto& nabarnbbar_ket = tbc_ket.Ket_unocc_hh;
    
    // There must be a better way than the diagmat multiplication...
    Matrixpp =  LHS1.cols(bras_pp) * RHS.rows(bras_pp) - RHS.cols(kets_pp)*LHS2.rows(kets_pp);
    Matrixhh =  LHS1.cols(bras_hh) * arma::diagmat(nanb_bra) * RHS.rows(bras_hh) - RHS.cols(kets_hh) * arma::diagmat(nanb_ket) * LHS2.rows(kets_hh);
    Matrixff =  LHS1.cols(bras_hh) * arma::diagmat(nabarnbbar_bra) * RHS.rows(bras_hh) - RHS.cols(kets_hh) * arma::diagmat(nabarnbbar_ket) * LHS2.rows(kets_hh);
 

    // Now, the two body part is easy
    OUT2 += Matrixpp + Matrixff - Matrixhh;

   }// for itmat

      // The one body part takes some additional work

   int norbits = modelspace->GetNumberOrbits();
   #pragma omp parallel for schedule(dynamic,1)
   for (int i=0;i<norbits;++i)
   {
      Orbit &oi = modelspace->GetOrbit(i);
      double ji = oi.j2/2.0;
      for (int j : Z.OneBodyChannels.at({oi.l, oi.j2, oi.tz2}) )
      {
         if (j<i) continue;
         Orbit &oj = modelspace->GetOrbit(j);
         double jj = oj.j2/2.0;
         double cijJ = 0;
         // Sum c over holes and include the nbar_a * nbar_b terms
//           for (auto& c : modelspace->holes)
           for (auto& it_c : modelspace->holes)
           {
              index_t c = it_c.first;
              Orbit &oc = modelspace->GetOrbit(c);
              double occ_c = oc.occ;
              double jc = oc.j2/2.0;
              int j1min = abs(jc-ji);
              int j1max = jc+ji;
              for (int J1=j1min; J1<=j1max; ++J1)
              {
               int j2min = max(int(abs(jc-jj)),abs(Lambda-J1));
               int j2max = min(int(jc+jj),J1+Lambda); 
               for (int J2=j2min; J2<=j2max; ++J2)
               {
                double hatfactor = sqrt( (2*J1+1)*(2*J2+1) );
                double sixj = modelspace->GetSixJ(J1, J2, Lambda, jj, ji, jc);
                cijJ += hatfactor * sixj * modelspace->phase(jj + jc + J1 + Lambda) * occ_c * Mpp.GetTBME_J(J1,J2,c,i,c,j);
                cijJ += hatfactor * sixj * modelspace->phase(jj + jc + J1 + Lambda) * (1-occ_c) * Mff.GetTBME_J(J1,J2,c,i,c,j);  // This is probably right???
               }
              }
           // Sum c over particles and include the n_a * n_b terms
           }
           for (auto& c : modelspace->particles)
           {
              Orbit &oc = modelspace->GetOrbit(c);
              double jc = oc.j2/2.0;
              int j1min = abs(jc-ji);
              int j1max = jc+ji;
              for (int J1=j1min; J1<=j1max; ++J1)
              {
               int j2min = max(int(abs(jc-jj)),abs(Lambda-J1));
               int j2max = min(int(jc+jj),J1+Lambda); 
               for (int J2=j2min; J2<=j2max; ++J2)
               {
                double hatfactor = sqrt( (2*J1+1)*(2*J2+1) );
                double sixj = modelspace->GetSixJ(J1, J2, Lambda, jj, ji, jc);
                cijJ += hatfactor * sixj * modelspace->phase(jj + jc + J1 + Lambda) * Mhh.GetTBME_J(J1,J2,c,i,c,j);
               }
              }
           }
//         #pragma omp critical
         Z.OneBody(i,j) += cijJ ;
      } // for j
    } // for i
    profiler.timer["comm222_pp_hh_221st"] += omp_get_wtime() - tstart;
}






//**************************************************************************
//
//  X^J_ij`kl` = - sum_J' { i j J } (2J'+1) X^J'_ilkj
//                        { k l J'}
// TENSOR VARIETY
/// The scalar Pandya transformation is defined as
/// \f[
///  \bar{X}^{J}_{i\bar{j}k\bar{l}} = - \sum_{J'} (2J'+1)
///  \left\{ \begin{array}{lll}
///  j_i  &  j_j  &  J \\
///  j_k  &  j_l  &  J' \\
///  \end{array} \right\}
///  X^{J}_{ilkj}
/// \f]
/// where the overbar indicates time-reversed orbits.
/// This function is designed for use with comm222_phss() and so it takes in
/// two arrays of matrices, one for hp terms and one for ph terms.
//void Operator::DoTensorPandyaTransformation(vector<arma::mat>& TwoBody_CC_hp, vector<arma::mat>& TwoBody_CC_ph)
//void Operator::DoTensorPandyaTransformation(map<array<int,2>,arma::mat>& TwoBody_CC_hp, map<array<int,2>,arma::mat>& TwoBody_CC_ph) const
void Operator::DoTensorPandyaTransformation( map<array<int,2>,arma::mat>& TwoBody_CC_ph) const
{
   int Lambda = rank_J;
   // loop over cross-coupled channels
   int nch = modelspace->SortedTwoBodyChannels_CC.size();
//   for ( int ch_bra_cc : modelspace->SortedTwoBodyChannels_CC )

   // Allocate map for matrices -- this needs to be serial.
   for ( int ch_bra_cc : modelspace->SortedTwoBodyChannels_CC )
   {
      TwoBodyChannel& tbc_bra_cc = modelspace->GetTwoBodyChannel_CC(ch_bra_cc);
      arma::uvec& bras_ph = tbc_bra_cc.GetKetIndex_ph();
      int nph_bras = bras_ph.n_rows;
      for ( int ch_ket_cc : modelspace->SortedTwoBodyChannels_CC )
      {
        TwoBodyChannel& tbc_ket_cc = modelspace->GetTwoBodyChannel_CC(ch_ket_cc);
        int nKets_cc = tbc_ket_cc.GetNumberKets();
//        TwoBody_CC_hp[{ch_bra_cc,ch_ket_cc}] = arma::mat(nph_bras,   2*nKets_cc, arma::fill::zeros);
        TwoBody_CC_ph[{ch_bra_cc,ch_ket_cc}] = arma::mat(2*nph_bras,   2*nKets_cc, arma::fill::zeros);
      }
   }

   #pragma omp parallel for schedule(dynamic,1) if (not this->tensor_transform_first_pass)
   for (int ich=0;ich<nch;++ich)
   {
      int ch_bra_cc = modelspace->SortedTwoBodyChannels_CC[ich];
      TwoBodyChannel& tbc_bra_cc = modelspace->GetTwoBodyChannel_CC(ch_bra_cc);
      int Jbra_cc = tbc_bra_cc.J;
      arma::uvec& bras_ph = tbc_bra_cc.GetKetIndex_ph();
      int nph_bras = bras_ph.n_rows;

      for ( int ch_ket_cc : modelspace->SortedTwoBodyChannels_CC )
      {
        TwoBodyChannel& tbc_ket_cc = modelspace->GetTwoBodyChannel_CC(ch_ket_cc);
        int Jket_cc = tbc_ket_cc.J;
        if ( (Jbra_cc+Jket_cc < rank_J) or abs(Jbra_cc-Jket_cc)>rank_J ) continue;
        if ( (tbc_bra_cc.parity + tbc_ket_cc.parity + parity)%2>0 ) continue;

        int nKets_cc = tbc_ket_cc.GetNumberKets();

//        arma::mat& MatCC_hp = TwoBody_CC_hp[{ch_bra_cc,ch_ket_cc}];
        arma::mat& MatCC_ph = TwoBody_CC_ph[{ch_bra_cc,ch_ket_cc}];
        // loop over ph bras <ad| in this channel
        for (int ibra=0; ibra<nph_bras; ++ibra)
        {
           Ket & bra_cc = tbc_bra_cc.GetKet( bras_ph[ibra] );
           int a = bra_cc.p;
           int b = bra_cc.q;
           Orbit & oa = modelspace->GetOrbit(a);
           Orbit & ob = modelspace->GetOrbit(b);
           double ja = oa.j2*0.5;
           double jb = ob.j2*0.5;

           // loop over kets |bc> in this channel
           // we go to 2*nKets to include |bc> and |cb>
           for (int iket_cc=0; iket_cc<2*nKets_cc; ++iket_cc)
           {
              Ket & ket_cc = tbc_ket_cc.GetKet(iket_cc%nKets_cc);
              int c = iket_cc < nKets_cc ? ket_cc.p : ket_cc.q;
              int d = iket_cc < nKets_cc ? ket_cc.q : ket_cc.p;
              Orbit & oc = modelspace->GetOrbit(c);
              Orbit & od = modelspace->GetOrbit(d);
              double jc = oc.j2*0.5;
              double jd = od.j2*0.5;


              int j1min = abs(ja-jd);
              int j1max = ja+jd;
              double sm = 0;
              for (int J1=j1min; J1<=j1max; ++J1)
              {
                int j2min = max(int(abs(jc-jb)),abs(J1-Lambda));
                int j2max = min(int(jc+jb),J1+Lambda);
                for (int J2=j2min; J2<=j2max; ++J2)
                {
                  double ninej = modelspace->GetNineJ(ja,jd,J1,jb,jc,J2,Jbra_cc,Jket_cc,Lambda);
                  if (abs(ninej) < 1e-8) continue;
                  double hatfactor = sqrt( (2*J1+1)*(2*J2+1)*(2*Jbra_cc+1)*(2*Jket_cc+1) );
                  double tbme = TwoBody.GetTBME_J(J1,J2,a,d,c,b);
                  sm -= hatfactor * modelspace->phase(jb+jd+Jket_cc+J2) * ninej * tbme ;
                }
              }
//              MatCC_hp(ibra,iket_cc) = sm;
              MatCC_ph(ibra,iket_cc) = sm;

              // Exchange (a <-> b) to account for the (n_a - n_b) term
              // Get Tz,parity and range of J for <bd || ca > coupling
              j1min = abs(jb-jd);
              j1max = jb+jd;
              sm = 0;
              for (int J1=j1min; J1<=j1max; ++J1)
              {
                int j2min = max(int(abs(jc-ja)),abs(J1-Lambda));
                int j2max = min(int(jc+ja),J1+Lambda);
                for (int J2=j2min; J2<=j2max; ++J2)
                {
                  double ninej = modelspace->GetNineJ(jb,jd,J1,ja,jc,J2,Jbra_cc,Jket_cc,Lambda);
                  if (abs(ninej) < 1e-8) continue;
                  double hatfactor = sqrt( (2*J1+1)*(2*J2+1)*(2*Jbra_cc+1)*(2*Jket_cc+1) );
                  double tbme = TwoBody.GetTBME_J(J1,J2,b,d,c,a);
                  sm -= hatfactor * modelspace->phase(ja+jd+Jket_cc+J2) * ninej * tbme ;
                }
              }
//              MatCC_ph(ibra,iket_cc) = sm;
              MatCC_ph(ibra+nph_bras,iket_cc) = sm;

           }
        }
    }
   }
}


void Operator::AddInverseTensorPandyaTransformation(map<array<int,2>,arma::mat>& Zbar)
{
    // Do the inverse Pandya transform
    // Only go parallel if we've previously calculated the SixJ's. Otherwise, it's not thread safe.
//   for (int ch=0;ch<nChannels;++ch)
//   int n_nonzeroChannels = modelspace->SortedTwoBodyChannels.size();
//   #pragma omp parallel for schedule(dynamic,1) if (not modelspace->SixJ_is_empty())
//   for (int ich = 0; ich < n_nonzeroChannels; ++ich)
   Operator& Z = *this;
   int Lambda = Z.rank_J;
   vector<map<array<int,2>,arma::mat>::iterator> iteratorlist;
   for (map<array<int,2>,arma::mat>::iterator iter= Z.TwoBody.MatEl.begin(); iter!= Z.TwoBody.MatEl.end(); ++iter) iteratorlist.push_back(iter);
   int niter = iteratorlist.size();
//   for (auto& iter : Z.TwoBody.MatEl)
   #pragma omp parallel for schedule(dynamic,1) if (not this->tensor_transform_first_pass)
//   for (auto iter=Z.TwoBody.MatEl.begin(); iter<Z.TwoBody.MatEl.end(); ++iter)
   for (int i=0; i<niter; ++i)
   {
      auto iter = iteratorlist[i];
      int ch_bra = iter->first[0];
      int ch_ket = iter->first[1];
      TwoBodyChannel& tbc_bra = modelspace->GetTwoBodyChannel(ch_bra);
      TwoBodyChannel& tbc_ket = modelspace->GetTwoBodyChannel(ch_ket);
      int J1 = tbc_bra.J;
      int J2 = tbc_ket.J;
//      cout << "-- " << ch_bra << " " << ch_ket << endl;
//      cout << "-- Jbra = " << tbc_bra.J << " T_bra = " << tbc_bra.Tz << "   Jket = " << tbc_ket.J << " T_ket = " << tbc_ket.Tz << endl;
      int nBras = tbc_bra.GetNumberKets();
      int nKets = tbc_ket.GetNumberKets();
      arma::mat& Zijkl = iter->second;

      for (int ibra=0; ibra<nBras; ++ibra)
      {
//         cout << "ibra = " << ibra << endl;
         Ket & bra = tbc_bra.GetKet(ibra);
         int i = bra.p;
         int j = bra.q;
         Orbit & oi = modelspace->GetOrbit(i);
         Orbit & oj = modelspace->GetOrbit(j);
         double ji = oi.j2/2.;
         double jj = oj.j2/2.;
//         int ketmin = hermitian ? ibra : ibra+1;
         int ketmin = 0;
         for (int iket=ketmin; iket<nKets; ++iket)
         {
//           cout << "iket = " << iket << endl;
            Ket & ket = tbc_ket.GetKet(iket);
            int k = ket.p;
            int l = ket.q;
            Orbit & ok = modelspace->GetOrbit(k);
            Orbit & ol = modelspace->GetOrbit(l);
            double jk = ok.j2/2.;
            double jl = ol.j2/2.;

            double commij = 0;
            double commji = 0;

            // Transform Z_ilkj
            int parity_bra_cc = (oi.l+ol.l)%2;
            int parity_ket_cc = (ok.l+oj.l)%2;
            int Tz_bra_cc = abs(oi.tz2+ol.tz2)/2;
            int Tz_ket_cc = abs(ok.tz2+oj.tz2)/2;
//            int Tz_bra_cc = (oi.tz2+ol.tz2)/2;
//            int Tz_ket_cc = (ok.tz2+oj.tz2)/2;
            int j3min = abs(int(ji-jl));
            int j3max = ji+jl;
//            cout << "before J3 loop, ti = " << oi.tz2 << " tj = " << oj.tz2 << " tk = " << ok.tz2 << " tl = " << ol.tz2  << endl;
            for (int J3=j3min; J3<=j3max; ++J3)
            {
              int ch_bra_cc = modelspace->GetTwoBodyChannelIndex(J3,parity_bra_cc,Tz_bra_cc);
              TwoBodyChannel_CC& tbc_bra_cc = modelspace->GetTwoBodyChannel_CC(ch_bra_cc);
              int indx_il = tbc_bra_cc.GetLocalIndex(min(i,l),max(i,l));
              if (i>l) indx_il += tbc_bra_cc.GetNumberKets();
              int j4min = max(abs(int(jk-jj)),abs(J3-Lambda));
              int j4max = min(int(jk+jj),J3+Lambda);
              for (int J4=j4min; J4<=j4max; ++J4)
              {
                 int ch_ket_cc = modelspace->GetTwoBodyChannelIndex(J4,parity_ket_cc,Tz_ket_cc);
                 TwoBodyChannel_CC& tbc_ket_cc = modelspace->GetTwoBodyChannel_CC(ch_ket_cc);
                 int indx_kj = tbc_ket_cc.GetLocalIndex(min(j,k),max(j,k));
                 if (k>j) indx_kj += tbc_ket_cc.GetNumberKets();

                  double ninej = modelspace->GetNineJ(ji,jl,J3,jj,jk,J4,J1,J2,Lambda);
                  if (abs(ninej) < 1e-8) continue;
                  double hatfactor = sqrt( (2*J1+1)*(2*J2+1)*(2*J3+1)*(2*J4+1) );
//                  cout << "before getting tbme  ch_bra_cc = " << ch_bra_cc << "  ch_ket_cc = " << ch_ket_cc << endl;
//                  cout << "J3 = " << J3 << " parity =  " << parity_bra_cc << " Tz = " << Tz_bra_cc << "   "
//                       << "J4 = " << J4 << " parity =  " << parity_ket_cc << " Tz = " << Tz_ket_cc << endl;
//                  cout << "indx_il = " << indx_il << "  indx_kj = " << indx_kj << "  size of mtx = " << Zbar[{ch_bra_cc,ch_ket_cc}].n_cols << endl;
                  double tbme = Zbar[{ch_bra_cc,ch_ket_cc}](indx_il,indx_kj);
//                  cout << "after getting tbme" << endl;
                  commij += hatfactor * modelspace->phase(jj+jl+J2+J4) * ninej * tbme ;
              }
            }
//            cout << "after J3 loop" << endl;

            // Transform Z_jlki
            parity_bra_cc = (oj.l+ol.l)%2;
            parity_ket_cc = (ok.l+oi.l)%2;
            Tz_bra_cc = abs(oj.tz2+ol.tz2)/2;
            Tz_ket_cc = abs(ok.tz2+oi.tz2)/2;
            j3min = abs(int(jj-jl));
            j3max = jj+jl;

            for (int J3=j3min; J3<=j3max; ++J3)
            {
              int ch_bra_cc = modelspace->GetTwoBodyChannelIndex(J3,parity_bra_cc,Tz_bra_cc);
              TwoBodyChannel_CC& tbc_bra_cc = modelspace->GetTwoBodyChannel_CC(ch_bra_cc);
              int indx_jl = tbc_bra_cc.GetLocalIndex(min(j,l),max(j,l));
              if (j>l) indx_jl += tbc_bra_cc.GetNumberKets();
              int j4min = max(abs(int(jk-ji)),abs(J3-Lambda));
              int j4max = min(int(jk+ji),J3+Lambda);
              for (int J4=j4min; J4<=j4max; ++J4)
              {
                 int ch_ket_cc = modelspace->GetTwoBodyChannelIndex(J4,parity_ket_cc,Tz_ket_cc);
                 TwoBodyChannel_CC& tbc_ket_cc = modelspace->GetTwoBodyChannel_CC(ch_ket_cc);
                 int indx_ki = tbc_ket_cc.GetLocalIndex(min(k,i),max(k,i));
                 if (k>i) indx_ki += tbc_ket_cc.GetNumberKets();

                  double ninej = modelspace->GetNineJ(jj,jl,J3,ji,jk,J4,J1,J2,Lambda);
                  if (abs(ninej) < 1e-8) continue;
                  double hatfactor = sqrt( (2*J1+1)*(2*J2+1)*(2*J3+1)*(2*J4+1) );
                  double tbme = Zbar[{ch_bra_cc,ch_ket_cc}](indx_jl,indx_ki);
                  commji += hatfactor * modelspace->phase(ji+jl+J2+J4) * ninej * tbme ;
              }
            }

            double norm = bra.delta_pq()==ket.delta_pq() ? 1+bra.delta_pq() : SQRT2;
            Zijkl(ibra,iket) += (commij - modelspace->phase(ji+jj-J1)*commji) / norm;
         }
      }
   }
 
   tensor_transform_first_pass = false;
}




//*****************************************************************************************
//
//  THIS IS THE BIG UGLY ONE.     
//                                             
//   |          |      |          |           
//   |     __Y__|      |     __X__|            
//   |    /\    |      |    /\    |
//   |   (  )   |  _   |   (  )   |            
//   |____\/    |      |____\/    |            
//   |  X       |      |  Y       |            
//           
//            
// -- This appears to agree with Nathan's results
//
/// Calculates the part of \f$ [X_{(2)},\mathbb{Y}^{\Lambda}_{(2)}]_{ijkl} \f$ which involves ph intermediate states, here indicated by \f$ \mathbb{Z}^{J_1J_2\Lambda}_{ijkl} \f$
/// \f[
/// \mathbb{Z}^{J_1J_2\Lambda}_{ijkl} = \sum_{abJ_3J_4}(n_a-n_b) \hat{J_1}\hat{J_2}\hat{J_3}\hat{J_4}
/// \left[
///  \left\{ \begin{array}{lll}
///  j_i  &  j_l  &  J_3 \\
///  j_j  &  j_k  &  J_4 \\
///  J_1  &  J_2  &  \Lambda \\
///  \end{array} \right\}
/// \left( \bar{X}^{J3}_{i\bar{l}a\bar{b}}\bar{\mathbb{Y}}^{J_3J_4\Lambda}_{a\bar{b}k\bar{j}} - 
///   \bar{\mathbb{Y}}^{J_3J_4\Lambda}_{i\bar{l}a\bar{b}}\bar{X}^{J_4}_{a\bar{b}k\bar{j}} \right)
///  -(-1)^{j_i+j_j-J_1}
///  \left\{ \begin{array}{lll}
///  j_j  &  j_l  &  J_3 \\
///  j_i  &  j_k  &  J_4 \\
///  J_1  &  J_2  &  \Lambda \\
///  \end{array} \right\}
/// \left( \bar{X}^{J_3}_{i\bar{l}a\bar{b}}\bar{\mathbb{Y}}^{J_3J_4\Lambda}_{a\bar{b}k\bar{j}} - 
///   \bar{\mathbb{Y}}^{J_3J_4\Lambda}_{i\bar{l}a\bar{b}}\bar{X}^{J_4}_{a\bar{b}k\bar{j}} \right)
/// \right]
/// \f]
/// This is implemented by defining an intermediate matrix
/// \f[
/// \bar{\mathbb{Z}}^{J_3J_4\Lambda}_{i\bar{l}k\bar{j}} \equiv \sum_{ab}(n_a\bar{n}_b)
/// \left[ \left( \bar{X}^{J3}_{i\bar{l}a\bar{b}}\bar{\mathbb{Y}}^{J_3J_4\Lambda}_{a\bar{b}k\bar{j}} - 
///   \bar{\mathbb{Y}}^{J_3J_4\Lambda}_{i\bar{l}a\bar{b}}\bar{X}^{J_4}_{a\bar{b}k\bar{j}} \right)
/// -\left( \bar{X}^{J_3}_{i\bar{l}b\bar{a}}\bar{\mathbb{Y}}^{J_3J_4\Lambda}_{b\bar{a}k\bar{j}} - 
///    \bar{\mathbb{Y}}^{J_3J_4\Lambda}_{i\bar{l}b\bar{a}}\bar{X}^{J_4}_{b\bar{a}k\bar{j}} \right)\right]
/// \f]
/// The Pandya-transformed matrix elements are obtained with DoTensorPandyaTransformation().
/// The matrices \f$ \bar{X}^{J_3}_{i\bar{l}a\bar{b}}\bar{\mathbb{Y}}^{J_3J_4\Lambda}_{a\bar{b}k\bar{j}} \f$
/// and \f$ \bar{\mathbb{Y}}^{J_4J_3\Lambda}_{i\bar{l}a\bar{b}}\bar{X}^{J_3}_{a\bar{b}k\bar{j}} \f$
/// are related by a Hermitian conjugation, which saves two matrix multiplications, provided we
/// take into account the phase \f$ (-1)^{J_3-J_4} \f$ from conjugating the spherical tensor.
/// The commutator is then given by
/// \f[
/// \mathbb{Z}^{J_1J_2\Lambda}_{ijkl} = \sum_{J_3J_4} \hat{J_1}\hat{J_2}\hat{J_3}\hat{J_4}
/// \left[
///  \left\{ \begin{array}{lll}
///  j_i  &  j_l  &  J_3 \\
///  j_j  &  j_k  &  J_4 \\
///  J_1  &  J_2  &  \Lambda \\
///  \end{array} \right\}
///  \bar{\mathbb{Z}}^{J_3J_4\Lambda}_{i\bar{l}k\bar{j}}
///  -(-1)^{j_i+j_j-J}
///  \left\{ \begin{array}{lll}
///  j_j  &  j_l  &  J_3 \\
///  j_i  &  j_k  &  J_4 \\
///  J_1  &  J_2  &  \Lambda \\
///  \end{array} \right\}
///  \bar{\mathbb{Z}}^{J_3J_4\Lambda}_{j\bar{l}k\bar{i}}
///  \right]
///  \f]
///
//void Operator::comm222_phst( Operator& Y, Operator& Z ) 
void Operator::comm222_phst( const Operator& X, const Operator& Y ) 
{

   Operator& Z = *this;
   // Create Pandya-transformed hp and ph matrix elements
//   deque<arma::mat> X_bar_hp = InitializePandya( nChannels, "transpose");
   deque<arma::mat> Xt_bar_ph = InitializePandya( nChannels, "transpose");

//   map<array<int,2>,arma::mat> Y_bar_hp;
   map<array<int,2>,arma::mat> Y_bar_ph;

   double t_start = omp_get_wtime();
   X.DoPandyaTransformation(Xt_bar_ph, "transpose" );
//   X.DoPandyaTransformation(X_bar_hp, X_bar_ph, "transpose" );
//   Y.DoTensorPandyaTransformation(Y_bar_hp, Y_bar_ph );
   Y.DoTensorPandyaTransformation(Y_bar_ph );
   profiler.timer["DoTensorPandyaTransformation"] += omp_get_wtime() - t_start;


   t_start = omp_get_wtime();
   // Construct the intermediate matrix Z_bar
   map<array<int,2>,arma::mat> Z_bar;
//   vector<int> ybras(Y_bar_hp.size());
//   vector<int> ykets(Y_bar_hp.size());
   vector<int> ybras(Y_bar_ph.size());
   vector<int> ykets(Y_bar_ph.size());
   int counter = 0;
//   for (auto& iter : Y_bar_hp )
   for (auto& iter : Y_bar_ph )
   {
      ybras[counter] = iter.first[0];
      ykets[counter] = iter.first[1];
      Z_bar[{iter.first[0],iter.first[1]}] = arma::mat(); // important to initialize this for parallelization
      counter++;
   }
   profiler.timer["Allocate Z_bar_tensor"] += omp_get_wtime() - t_start;
   t_start = omp_get_wtime();

   // This should definitely be checked, especially the hermitian phase business.
   #pragma omp parallel for schedule(dynamic,1)
   for(int i=0;i<counter;++i)
   {
      int ch_bra_cc = ybras[i];
      int ch_ket_cc = ykets[i];
      int Jbra = modelspace->GetTwoBodyChannel_CC(ch_bra_cc).J;
      int Jket = modelspace->GetTwoBodyChannel_CC(ch_ket_cc).J;
      int flipphase = modelspace->phase( Jbra - Jket ) * ( Z.IsHermitian() ? -1 : 1 );
      auto& XJ1 = Xt_bar_ph[ch_bra_cc];
      auto& XJ2 = Xt_bar_ph[ch_ket_cc];
      auto& YJ1J2 = Y_bar_ph[{ch_bra_cc,ch_ket_cc}]; // These two should be related...
      auto& YJ2J1 = Y_bar_ph[{ch_ket_cc,ch_bra_cc}];
      
      Z_bar[{ch_bra_cc,ch_ket_cc}] = XJ1 * YJ1J2 -flipphase*(XJ2 * YJ2J1).t();
   }
   profiler.timer["Build Z_bar_tensor"] += omp_get_wtime() - t_start;

   t_start = omp_get_wtime();
   Z.AddInverseTensorPandyaTransformation(Z_bar);
   profiler.timer["InverseTensorPandyaTransformation"] += omp_get_wtime() - t_start;

}









