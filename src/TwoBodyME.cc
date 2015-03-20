
#include "TwoBodyME.hh"
#ifndef SQRT2
  #define SQRT2 1.4142135623730950488
#endif


TwoBodyME::TwoBodyME()
: hermitian(true),antihermitian(false)
{}

TwoBodyME::TwoBodyME(ModelSpace* ms)
: modelspace(ms), nChannels(ms->GetNumberTwoBodyChannels()),
 rank_J(0), rank_T(0), parity(0), hermitian(true), antihermitian(false)
{}

TwoBodyME::TwoBodyME(ModelSpace* ms, int rJ, int rT, int p)
: modelspace(ms), nChannels(ms->GetNumberTwoBodyChannels()),
 rank_J(rJ), rank_T(rT), parity(p), hermitian(true), antihermitian(false),
 TwoBodyTensorChannels(ms->GetNumberTwoBodyChannels())
{}



 TwoBodyME& TwoBodyME::operator=(const TwoBodyME& rhs)
 {
   Copy(rhs);
   return *this;
 }

 TwoBodyME& TwoBodyME::operator*=(const double rhs)
 {
   for ( auto& itmat : MatEl )
   {
      itmat.second *= rhs;
   }

   return *this;
 }

 TwoBodyME& TwoBodyME::operator+=(const TwoBodyME& rhs)
 {
   for ( auto& itmat : MatEl )
   {
      int ch_bra = itmat.first[0];
      int ch_ket = itmat.first[1];
      itmat.second += rhs.GetMatrix(ch_bra,ch_ket);
   }

 }

 TwoBodyME& TwoBodyME::operator-=(const TwoBodyME& rhs)
 {
   for ( auto& itmat : MatEl )
   {
      int ch_bra = itmat.first[0];
      int ch_ket = itmat.first[1];
      itmat.second -= rhs.GetMatrix(ch_bra,ch_ket);
   }

 }


void TwoBodyME::Copy(const TwoBodyME& rhs)
{
   modelspace = rhs.modelspace;
   MatEl      = rhs.MatEl;
   nChannels  = rhs.nChannels;
   hermitian  = rhs.hermitian;
   antihermitian  = rhs.antihermitian;
   rank_J     = rhs.rank_J;
   rank_T     = rhs.rank_T;
   parity     = rhs.parity;
   TwoBodyTensorChannels = rhs.TwoBodyTensorChannels;

}

void TwoBodyME::AllocateTwoBody()
{
//  MatEl.resize(nChannels);
//  TwoBodyTensorChannels.resize(nChannels);
  for (int ch_bra=0; ch_bra<nChannels;++ch_bra)
  {
     TwoBodyChannel& tbc_bra = modelspace->GetTwoBodyChannel(ch_bra);
     for (int ch_ket=ch_bra; ch_ket<nChannels;++ch_ket)
     {
        TwoBodyChannel& tbc_ket = modelspace->GetTwoBodyChannel(ch_ket);
        if ( abs(tbc_bra.J-tbc_ket.J)>rank_J ) continue;
        if ( (tbc_bra.J+tbc_ket.J)<rank_J ) continue;
        if ( abs(tbc_bra.Tz-tbc_ket.Tz)>rank_T ) continue;
        if ( (tbc_bra.parity + tbc_ket.parity + parity)%2>0 ) continue;
        
//        TwoBodyTensorChannels[ch_bra].push_back(ch_ket);
//        MatEl[ch_bra][ch_ket] =  arma::mat(tbc_bra.GetNumberKets(), tbc_ket.GetNumberKets(), arma::fill::zeros);
        MatEl[{ch_bra,ch_ket}] =  arma::mat(tbc_bra.GetNumberKets(), tbc_ket.GetNumberKets(), arma::fill::zeros);
        
     }
  }
}



/// This returns the matrix element times a factor \f$ \sqrt{(1+\delta_{ij})(1+\delta_{kl})} \f$
double TwoBodyME::GetTBME(int ch_bra, int ch_ket, int a, int b, int c, int d) const
{
   TwoBodyChannel& tbc_bra =  modelspace->GetTwoBodyChannel(ch_bra);
   TwoBodyChannel& tbc_ket =  modelspace->GetTwoBodyChannel(ch_ket);
   int bra_ind = tbc_bra.GetLocalIndex(min(a,b),max(a,b));
   int ket_ind = tbc_ket.GetLocalIndex(min(c,d),max(c,d));
   if (bra_ind < 0 or ket_ind < 0 or bra_ind > tbc_bra.GetNumberKets() or ket_ind > tbc_ket.GetNumberKets() )
     return 0;
   Ket & bra = tbc_bra.GetKet(bra_ind);
   Ket & ket = tbc_ket.GetKet(ket_ind);

   double phase = 1;
   if (a>b) phase *= bra.Phase(tbc_bra.J);
   if (c>d) phase *= ket.Phase(tbc_ket.J);
   if (a==b) phase *= SQRT2;
   if (c==d) phase *= SQRT2;
   return phase * GetMatrix(ch_bra,ch_ket)(bra_ind, ket_ind);
}


void TwoBodyME::SetTBME(int ch_bra, int ch_ket, int a, int b, int c, int d, double tbme)
{
   TwoBodyChannel& tbc_bra =  modelspace->GetTwoBodyChannel(ch_bra);
   TwoBodyChannel& tbc_ket =  modelspace->GetTwoBodyChannel(ch_ket);
   int bra_ind = tbc_bra.GetLocalIndex(min(a,b),max(a,b));
   int ket_ind = tbc_ket.GetLocalIndex(min(c,d),max(c,d));
   double phase = 1;
   if (a>b) phase *= tbc_bra.GetKet(bra_ind).Phase(tbc_bra.J);
   if (c>d) phase *= tbc_ket.GetKet(ket_ind).Phase(tbc_ket.J);
   GetMatrix(ch_bra,ch_ket)(bra_ind,ket_ind) = phase * tbme;
   if (hermitian) GetMatrix(ch_bra,ch_ket)(ket_ind,bra_ind) = phase * tbme;
   if (antihermitian) GetMatrix(ch_bra,ch_ket)(ket_ind,bra_ind) = - phase * tbme;
}


void TwoBodyME::AddToTBME(int ch_bra, int ch_ket, int a, int b, int c, int d, double tbme)
{
   TwoBodyChannel& tbc_bra =  modelspace->GetTwoBodyChannel(ch_bra);
   TwoBodyChannel& tbc_ket =  modelspace->GetTwoBodyChannel(ch_ket);
   int bra_ind = tbc_bra.GetLocalIndex(min(a,b),max(a,b));
   int ket_ind = tbc_ket.GetLocalIndex(min(c,d),max(c,d));
   double phase = 1;
   if (a>b) phase *= tbc_bra.GetKet(bra_ind).Phase(tbc_bra.J);
   if (c>d) phase *= tbc_ket.GetKet(ket_ind).Phase(tbc_ket.J);
   GetMatrix(ch_bra,ch_ket)(bra_ind,ket_ind) += phase * tbme;
   if (hermitian) GetMatrix(ch_bra,ch_ket)(ket_ind,bra_ind) += phase * tbme;
   if (antihermitian) GetMatrix(ch_bra,ch_ket)(ket_ind,bra_ind) -=  phase * tbme;
}

double TwoBodyME::GetTBME(int ch_bra, int ch_ket, Ket &bra, Ket &ket) const
{
   return GetTBME(ch_bra,ch_ket,bra.p,bra.q,ket.p,ket.q);
}
void TwoBodyME::SetTBME(int ch_bra, int ch_ket, Ket& bra, Ket& ket, double tbme)
{
   SetTBME(ch_bra, ch_ket, bra.p,bra.q,ket.p,ket.q,tbme);
}
void TwoBodyME::AddToTBME(int ch_bra, int ch_ket, Ket& bra, Ket& ket, double tbme)
{
   AddToTBME(ch_bra, ch_ket, bra.p,bra.q,ket.p,ket.q, tbme);
}

double TwoBodyME::GetTBME(int ch_bra, int ch_ket, int ibra, int iket) const
{
   return GetMatrix(ch_bra,ch_ket)(ibra,iket);
}

void TwoBodyME::SetTBME(int ch_bra, int ch_ket, int iket, int ibra, double tbme)
{
   GetMatrix(ch_bra,ch_ket)(ibra,iket) = tbme;
   if (IsHermitian())
      GetMatrix(ch_bra,ch_ket)(iket,ibra) = tbme;
   else if(IsAntiHermitian())
      GetMatrix(ch_bra,ch_ket)(iket,ibra) = -tbme;
}
void TwoBodyME::AddToTBME(int ch_bra, int ch_ket, int iket, int ibra, double tbme)
{
   GetMatrix(ch_bra,ch_ket)(ibra,iket) += tbme;
   if (IsHermitian())
      GetMatrix(ch_bra,ch_ket)(iket,ibra) += tbme;
   else if(IsAntiHermitian())
      GetMatrix(ch_bra,ch_ket)(iket,ibra) -= tbme;
}

double TwoBodyME::GetTBME(int j_bra, int p_bra, int t_bra, int j_ket, int p_ket, int t_ket, int a, int b, int c, int d) const
{
   int ch_bra = modelspace->GetTwoBodyChannelIndex(j_bra,p_bra,t_bra);
   int ch_ket = modelspace->GetTwoBodyChannelIndex(j_ket,p_ket,t_ket);
   return GetTBME(ch_bra,ch_ket,a,b,c,d);
}
void TwoBodyME::SetTBME(int j_bra, int p_bra, int t_bra, int j_ket, int p_ket, int t_ket, int a, int b, int c, int d, double tbme)
{
   int ch_bra = modelspace->GetTwoBodyChannelIndex(j_bra,p_bra,t_bra);
   int ch_ket = modelspace->GetTwoBodyChannelIndex(j_ket,p_ket,t_ket);
   SetTBME(ch_bra,ch_ket,a,b,c,d,tbme);
}
void TwoBodyME::AddToTBME(int j_bra, int p_bra, int t_bra, int j_ket, int p_ket, int t_ket, int a, int b, int c, int d, double tbme)
{
   int ch_bra = modelspace->GetTwoBodyChannelIndex(j_bra,p_bra,t_bra);
   int ch_ket = modelspace->GetTwoBodyChannelIndex(j_ket,p_ket,t_ket);
   AddToTBME(ch_bra,ch_ket,a,b,c,d,tbme);
}
double TwoBodyME::GetTBME(int j_bra, int p_bra, int t_bra, int j_ket, int p_ket, int t_ket, Ket& bra, Ket& ket) const
{
   int ch_bra = modelspace->GetTwoBodyChannelIndex(j_bra,p_bra,t_bra);
   int ch_ket = modelspace->GetTwoBodyChannelIndex(j_ket,p_ket,t_ket);
   return GetTBME(ch_bra,ch_ket,bra,ket);
}
void TwoBodyME::SetTBME(int j_bra, int p_bra, int t_bra, int j_ket, int p_ket, int t_ket, Ket& bra, Ket& ket, double tbme)
{
   int ch_bra = modelspace->GetTwoBodyChannelIndex(j_bra,p_bra,t_bra);
   int ch_ket = modelspace->GetTwoBodyChannelIndex(j_ket,p_ket,t_ket);
   SetTBME(ch_bra,ch_ket,bra,ket,tbme);
}
void TwoBodyME::AddToTBME(int j_bra, int p_bra, int t_bra, int j_ket, int p_ket, int t_ket, Ket& bra, Ket& ket, double tbme)
{
   int ch_bra = modelspace->GetTwoBodyChannelIndex(j_bra,p_bra,t_bra);
   int ch_ket = modelspace->GetTwoBodyChannelIndex(j_ket,p_ket,t_ket);
   AddToTBME(ch_bra,ch_ket,bra,ket,tbme);
}



// for backwards compatibility...
double TwoBodyME::GetTBME(int ch, int a, int b, int c, int d) const
{
   return GetTBME(ch,ch,a,b,c,d);
}

void TwoBodyME::SetTBME(int ch, int a, int b, int c, int d, double tbme)
{
   return SetTBME(ch,ch,a,b,c,d,tbme);
}

double TwoBodyME::GetTBME(int ch, Ket &bra, Ket &ket) const
{
   return GetTBME(ch,ch,bra.p,bra.q,ket.p,ket.q);
}

void TwoBodyME::SetTBME(int ch, Ket& bra, Ket& ket, double tbme)
{
   SetTBME(ch,ch,bra.p,bra.q,ket.p,ket.q,tbme);
}

void TwoBodyME::AddToTBME(int ch, Ket& bra, Ket& ket, double tbme)
{
   AddToTBME(ch,ch,bra.p,bra.q,ket.p,ket.q,tbme);
}


double TwoBodyME::GetTBME(int ch, int ibra, int iket) const
{
   return GetTBME(ch,ch,ibra,iket);
}

void TwoBodyME::SetTBME(int ch, int ibra, int iket, double tbme)
{
   SetTBME(ch,ch,ibra,iket,tbme);
}

void TwoBodyME::AddToTBME(int ch, int ibra, int iket, double tbme)
{
   AddToTBME(ch,ch,ibra,iket,tbme);
}



double TwoBodyME::GetTBME(int j, int p, int t, int a, int b, int c, int d) const
{
   int ch = modelspace->GetTwoBodyChannelIndex(j,p,t);
   return GetTBME(ch,ch,a,b,c,d);
}

void TwoBodyME::SetTBME(int j, int p, int t, int a, int b, int c, int d, double tbme)
{
   int ch = modelspace->GetTwoBodyChannelIndex(j,p,t);
   SetTBME(ch,ch,a,b,c,d,tbme);
}

void TwoBodyME::AddToTBME(int j, int p, int t, int a, int b, int c, int d, double tbme)
{
   int ch = modelspace->GetTwoBodyChannelIndex(j,p,t);
   AddToTBME(ch,ch,a,b,c,d,tbme);
}

double TwoBodyME::GetTBME(int j, int p, int t, Ket& bra, Ket& ket) const
{
   int ch = modelspace->GetTwoBodyChannelIndex(j,p,t);
   return GetTBME(ch,ch,bra,ket);
}

void TwoBodyME::SetTBME(int j, int p, int t, Ket& bra, Ket& ket, double tbme)
{
   int ch = modelspace->GetTwoBodyChannelIndex(j,p,t);
   SetTBME(ch,ch,bra,ket,tbme);
}

void TwoBodyME::AddToTBME(int j, int p, int t, Ket& bra, Ket& ket, double tbme)
{
   int ch = modelspace->GetTwoBodyChannelIndex(j,p,t);
   AddToTBME(ch,ch,bra,ket,tbme);
}

/// Returns an unnormalized monopole-like (angle-averaged) term
/// \f[ \bar{V}_{ijkl} = \sqrt{(1+\delta_{ij})(1+\delta_{kl})} \sum_{J}(2J+1) V_{ijkl}^J \f]
///
double TwoBodyME::GetTBMEmonopole(int a, int b, int c, int d) const
{
   double mon = 0;
   Orbit &oa = modelspace->GetOrbit(a);
   Orbit &ob = modelspace->GetOrbit(b);
   Orbit &oc = modelspace->GetOrbit(c);
   Orbit &od = modelspace->GetOrbit(d);
   int Tzab = (oa.tz2 + ob.tz2)/2;
   int parityab = (oa.l + ob.l)%2;
   int Tzcd = (oc.tz2 + od.tz2)/2;
   int paritycd = (oc.l + od.l)%2;

   if (Tzab != Tzcd or parityab != paritycd) return 0;

   int jmin = abs(oa.j2 - ob.j2)/2;
   int jmax = (oa.j2 + ob.j2)/2;
   
   for (int J=jmin;J<=jmax;++J)
   {

      mon += (2*J+1) * GetTBME(J,parityab,Tzab,a,b,c,d);
   }
   mon /= (oa.j2 +1)*(ob.j2+1);
   return mon;
}


double TwoBodyME::GetTBMEmonopole(Ket & bra, Ket & ket) const
{
   return GetTBMEmonopole(bra.p,bra.q,ket.p,ket.q);
}

void TwoBodyME::Erase()
{
  for ( auto& itmat : MatEl )
  {
     arma::mat& matrix = itmat.second;
     matrix.zeros();
  }
}


double TwoBodyME::Norm() const
{
   double nrm = 0;
   for ( auto& itmat : MatEl )
   {
      const arma::mat& matrix = itmat.second;
      double n2 = arma::norm(matrix,"fro");
      nrm += n2*n2;
   }
   return sqrt(nrm);
}


void TwoBodyME::Symmetrize()
{
  for (auto& itmat : MatEl )
  {
    arma::mat& matrix = itmat.second;
    matrix = arma::symmatu(matrix);
  }
}

void TwoBodyME::AntiSymmetrize()
{
  for (auto& itmat : MatEl )
  {
    int ch_bra = itmat.first[0];
    int ch_ket = itmat.first[1];
    int nbras = modelspace->GetTwoBodyChannel(ch_bra).GetNumberKets();
    int nkets = modelspace->GetTwoBodyChannel(ch_ket).GetNumberKets();
    arma::mat& matrix = itmat.second;
    for (int ibra=0; ibra<nbras; ++ibra)
    {
      for (int iket=ibra+1; iket<nkets; ++iket)
      {
         matrix(iket,ibra) = -matrix(ibra,iket);
      }
    }
  }


}


void TwoBodyME::Scale(double x)
{
   for ( auto& itmat : MatEl )
   {
      arma::mat& matrix = itmat.second;
      matrix *= x;
   }

}

void TwoBodyME::Eye()
{
   for ( auto& itmat : MatEl )
   {
      arma::mat& matrix = itmat.second;
      matrix.eye();
   }
}


int TwoBodyME::Dimension()
{
   int dim = 0;
   for ( auto& itmat : MatEl )
   {
      int N = itmat.second.n_cols;
      dim += N*(N+1)/2;
   }
   return dim;
}

