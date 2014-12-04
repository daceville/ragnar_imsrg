
#include "imsrg_util.hh"
#include "AngMom.hh"

using namespace AngMom;

namespace imsrg_util
{

 Operator NumberOp(ModelSpace& modelspace, int n, int l, int j2, int tz2)
 {
   Operator NumOp = Operator(&modelspace);
   int indx = modelspace.Index1(n,l,j2,tz2);
   NumOp.ZeroBody = 0;
   NumOp.EraseOneBody();
   NumOp.EraseTwoBody();
   NumOp.OneBody(indx,indx) = 1;
   return NumOp;
 }


 double HO_density(int n, int l, double hw, double r)
 {
    double v = M_NUCLEON * hw / (HBARC*HBARC);
    double Norm = pow(v/2.,1.5+l) * M_SQRT2/M_SQRTPI * pow(2,n+2*l+3) * gsl_sf_fact(n) / gsl_sf_doublefact(2*n + 2*l + 1);
    double L = gsl_sf_laguerre_n(n, l+0.5, v*r*r);
    double rho = Norm * pow(r,2*l) * exp(-v * r*r) * L * L;
    return rho;

 }

 // Just do the HF transformation
 vector<double> GetOccupations(HartreeFock& hf)
 {
    ModelSpace* modelspace = hf.Hbare.modelspace;
    int norb = modelspace->GetNumberOrbits();
    vector<double> occupation(norb);

    for (int i=0; i<norb; ++i)
    {
      Orbit * oi = modelspace->GetOrbit(i);
      // Get the number operator for orbit i
      Operator N_bare = NumberOp(*modelspace,oi->n,oi->l,oi->j2,oi->tz2);
      // Transform it to the normal-ordered HF basis
      Operator N_NO = hf.TransformToHFBasis(N_bare).DoNormalOrdering();
      occupation[i] = N_NO.ZeroBody;
      cout << oi->n << " " << oi->l << " " << oi->j2 << "/2 " << occupation[i] << endl;
    }
    return occupation;
 }

 // Do the full IMSRG transformation
 vector<double> GetOccupations(HartreeFock& hf, IMSRGSolver& imsrgsolver)
 {
    ModelSpace* modelspace = imsrgsolver.modelspace;
    int norb = modelspace->GetNumberOrbits();
    vector<double> occupation(norb,0);

    for (int i=0; i<norb; ++i)
    {
      Orbit * oi = modelspace->GetOrbit(i);
      // Get the number operator for orbit i
      Operator N_bare = NumberOp(*modelspace,oi->n,oi->l,oi->j2,oi->tz2);
      // Transform it to the normal-ordered HF basis
      Operator N_NO = hf.TransformToHFBasis(N_bare).DoNormalOrdering();
      // Transform to the imsrg evolved basis
      Operator N_final = imsrgsolver.Transform(N_NO);
      occupation[i] = N_final.ZeroBody;
    }
    return occupation;
 }

 vector<double> GetDensity( vector<double>& occupation, vector<double>& R, vector<int>& orbits, ModelSpace& modelspace )
 {
     int nr_steps = R.size();
     double hw = modelspace.GetHbarOmega();
     vector<double> dens(nr_steps,0);
     for (int& i : orbits)
     {
       Orbit * oi = modelspace.GetOrbit(i);
       for (int ir=0;ir<nr_steps;++ir)
       {
          dens[ir] += HO_density(oi->n,oi->l,hw,R[ir]) * occupation[i];
       }
     }
     return dens;
 }


// Center of mass kinetic energy, with the hw/A factor
 Operator TCM_Op(ModelSpace& modelspace)
 {
   Operator TcmOp = Operator(&modelspace);

   int norb = modelspace.GetNumberOrbits();
   for (int i=0; i<norb; ++i)
   {
      Orbit * oi = modelspace.GetOrbit(i);
      for (int j=i; j<norb; ++j)
      {
         Orbit * oj = modelspace.GetOrbit(j);
         if (oi->l != oj->l or oi->j2 != oj->j2 or oi->tz2 != oj->tz2) continue;
         double tij = 0;
         if (oi->n == oj->n) tij = 0.5*(2*oi->n+oi->l + 1.5);
         else if (oi->n == oj->n-1) tij = 0.5*sqrt(oj->n*(oj->n+oj->l + 0.5));
         TcmOp.OneBody(i,j) = tij;
         TcmOp.OneBody(j,i) = tij;
      }
   }

   int nchan = modelspace.GetNumberTwoBodyChannels();
   #pragma omp parallel for schedule(dynamic,5) 
   for (int ch=0; ch<nchan; ++ch)
   {
      TwoBodyChannel& tbc = modelspace.GetTwoBodyChannel(ch);
      int nkets = tbc.GetNumberKets();
      for (int ibra=0;ibra<nkets;++ibra)
      {
         Ket * bra = tbc.GetKet(ibra);
         for (int iket=ibra;iket<nkets;++iket)
         {
            Ket * ket = tbc.GetKet(iket);
            double mat_el = Calculate_p1p2(modelspace,bra,ket,tbc.J);
            #pragma omp critical
            {
              TcmOp.TwoBody[ch](ibra,iket) = mat_el;
              TcmOp.TwoBody[ch](iket,ibra) = mat_el;
            }
         }
      }
   }
//   return TcmOp;
   return TcmOp * modelspace.GetHbarOmega() / modelspace.GetTargetMass();
 }



 double Calculate_p1p2(ModelSpace& modelspace, Ket* bra, Ket* ket, int J)
 {
   Orbit * oa = modelspace.GetOrbit(bra->p);
   Orbit * ob = modelspace.GetOrbit(bra->q);
   Orbit * oc = modelspace.GetOrbit(ket->p);
   Orbit * od = modelspace.GetOrbit(ket->q);

   int na = oa->n;
   int nb = ob->n;
   int nc = oc->n;
   int nd = od->n;

   int la = oa->l;
   int lb = ob->l;
   int lc = oc->l;
   int ld = od->l;

   double ja = oa->j2/2.0;
   double jb = ob->j2/2.0;
   double jc = oc->j2/2.0;
   double jd = od->j2/2.0;

   int fab = 2*na + 2*nb + la + lb;
   int fcd = 2*nc + 2*nd + lc + ld;
   if (abs(fab-fcd)%2 >0) return 0; // p1*p2 only connects kets with delta N = 0,1
   if (abs(fab-fcd)>2) return 0; // p1*p2 only connects kets with delta N = 0,1

   double sa,sb,sc,sd;
   sa=sb=sc=sd=0.5;

   double T=0;

   // First, transform to LS coupling using 9j coefficients
   for (int Lab=abs(la-lb); Lab<= la+lb; ++Lab)
   {
     for (int Sab=0; Sab<=1; ++Sab)
     {
       if ( abs(Lab-Sab)>J or Lab+Sab<J) continue;

       double njab = NormNineJ(la,sa,ja, lb,sb,jb, Lab,Sab,J);
       if (njab == 0) continue;
       int Scd = Sab;
       int Lcd = Lab;
       double njcd = NormNineJ(lc,sc,jc, ld,sd,jd, Lcd,Scd,J);
       if (njcd == 0) continue;

       // Next, transform to rel / com coordinates with Moshinsky tranformation
       for (int N_ab=0; N_ab<=fab/2; ++N_ab)  // N_ab = CoM n for a,b
       {
         for (int Lam_ab=0; Lam_ab<= fab-2*N_ab; ++Lam_ab) // Lam_ab = CoM l for a,b
         {
           int Lam_cd = Lam_ab; // tcm and trel conserve lam and Lam, ie relative and com orbital angular momentum
           for (int lam_ab=(fab-2*N_ab-Lam_ab)%2; lam_ab<= (fab-2*N_ab-Lam_ab); lam_ab+=2) // lam_ab = relative l for a,b
           {
              // factor to account for antisymmetrization
              int asymm_factor = 1;

              if (ket->Tz!=0)
              {
                if ((lam_ab+Sab)%2>0) continue; // Pauli rule for identical particles
                asymm_factor = 2 ;
              }
              else if ( (bra->p + ket->p)%2 >0) // if we have pnnp or nppn, then pick up a phase
              {
                asymm_factor = modelspace.phase( lam_ab + Sab );
              }

              int lam_cd = lam_ab; // tcm and trel conserve lam and Lam
              int n_ab = (fab - 2*N_ab-Lam_ab-lam_ab)/2; // n_ab is determined by energy conservation

              double mosh_ab = modelspace.GetMoshinsky(N_ab,Lam_ab,n_ab,lam_ab,na,la,nb,lb,Lab);

              if (abs(mosh_ab)<1e-8) continue;

              for (int N_cd=max(0,N_ab-1); N_cd<=N_ab+1; ++N_cd) // N_cd = CoM n for c,d
              {
                int n_cd = (fcd - 2*N_cd-Lam_cd-lam_cd)/2; // n_cd is determined by energy conservation
                if (n_cd < 0) continue;
                if  (n_ab != n_cd and N_ab != N_cd) continue;

                double mosh_cd = modelspace.GetMoshinsky(N_cd,Lam_cd,n_cd,lam_cd,nc,lc,nd,ld,Lcd);
                if (abs(mosh_cd)<1e-8) continue;

                double tcm = 0;
                double trel = 0;
                if (n_ab == n_cd)
                {
                  if      (N_ab == N_cd)   tcm = 0.5*(2*N_ab+Lam_ab+1.5);
                  else if (N_ab == N_cd+1) tcm = 0.5*sqrt(N_ab*( N_ab+Lam_ab+0.5));
                  else if (N_ab == N_cd-1) tcm = 0.5*sqrt(N_cd*( N_cd+Lam_ab+0.5));
                }
                if (N_ab == N_cd)
                {
                  if      (n_ab == n_cd)   trel = 0.5*(2*n_ab+lam_ab+1.5);
                  else if (n_ab == n_cd+1) trel = 0.5*sqrt(n_ab*( n_ab+lam_ab+0.5));
                  else if (n_ab == n_cd-1) trel = 0.5*sqrt(n_cd*( n_cd+lam_cd+0.5));
                }
                double prefactor = njab * njcd * mosh_ab * mosh_cd * asymm_factor;
                T += (tcm-trel) * prefactor;

              } // N_cd
           } // lam_ab
         } // Lam_ab
       } // N_ab

     } // Sab
   } // Lab

   // normalize
   T /= sqrt((1.0+bra->delta_pq())*(1.0+ket->delta_pq()));
   return T ;

 }





}

