#ifndef HartreeFock_h
#define HartreeFock_h

#include "ModelSpace.hh"
#include "Operator.hh"
#include "IMSRGProfiler.hh"
#include <armadillo>
#include <map>
#include <deque>

class HartreeFock
{
 public:
   Operator& Hbare;         ///< Input bare Hamiltonian
   ModelSpace * modelspace; ///< Model Space of the Hamiltonian
   arma::mat C;             ///< transformation coefficients, 1st index is ho basis, 2nd = HF basis
   arma::mat rho;           ///< density matrix rho_ij
   arma::mat KE;            ///< kinetic energy
   arma::mat Vij;           ///< 1 body piece of 2 body potential
   arma::mat V3ij;          ///< 1 body piece of 3 body potential
   arma::mat F;             ///< Fock matrix
   array< array< arma::mat,2>,3> Vmon;          ///< Monopole 2-body interaction
   array< array< arma::mat,2>,3> Vmon_exch;          ///< Monopole 2-body interaction
   arma::uvec holeorbs;     ///< list of hole orbits for generating density matrix
   arma::rowvec hole_occ; /// occupations of hole orbits
   arma::vec energies;      ///< vector of single particle energies
   arma::vec prev_energies; ///< SPE's from last iteration
   double tolerance;        ///< tolerance for convergence
   double EHF;              ///< Hartree-Fock energy (Normal-ordered 0-body term)
   double e1hf;             ///< One-body contribution to EHF
   double e2hf;             ///< Two-body contribution to EHF
   double e3hf;             ///< Three-body contribution to EHF
   int iterations;          ///< iterations used in Solve()
   vector< pair<const array<int,6>,double>> Vmon3;
   IMSRGProfiler profiler;  ///< Profiler for timing, etc.
   deque<double> convergence_ediff; ///< Save last few convergence checks for diagnostics
   deque<double> convergence_EHF; ///< Save last few convergence checks for diagnostics

// Methods
   HartreeFock(Operator&  hbare); ///< Constructor
   void BuildMonopoleV();         ///< Only the monopole part of V is needed, so construct it.
   void BuildMonopoleV3();        ///< Only the monopole part of V3 is needed.
   void Diagonalize();            ///< Diagonalize the Fock matrix
   void UpdateF();                ///< Update the Fock matrix with the new transformation coefficients C
   void UpdateDensityMatrix();    ///< Update the density matrix with the new coefficients C
   bool CheckConvergence();       ///< Compare the current energies with those from the previous iteration
   void Solve();                  ///< Diagonalize and UpdateF until convergence
   void CalcEHF();                ///< Evaluate the Hartree Fock energy
   void PrintEHF();               ///< Print out the Hartree Fock energy
   void ReorderCoefficients();    ///< Reorder the coefficients in C to eliminate phases etc.
   Operator TransformToHFBasis( Operator& OpIn); ///< Transform an operator from oscillator basis to HF basis
   Operator GetNormalOrderedH();  ///< Return the Hamiltonian in the HF basis at the normal-ordered 2body level.
   Operator GetOmega();           ///< Return a generator of the Hartree Fock transformation
   Operator GetHbare(){return Hbare;}; ///< Getter function for Hbare
   void PrintSPE(); ///< Print out the single-particle energies
//   void PrintSPE(){ F.diag().print();}; ///< Print out the single-particle energies
   void FreeVmon();               ///< Free up the memory used to store Vmon3.

};



#endif

