//////////////////////////////////////////////////////////////////////////
///  imsrg++ : Interface for performing standard IMSRG calculations.   ///
///            Usage is imsrg++  option1=value1 option2=value2 ...     ///
///            To get a list of options, type imsrg++ help             ///
///                                                                    ///
///                                             - Ragnar Stroberg 2016 ///
//////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <omp.h>
#include "IMSRG.hh"
#include "Parameters.hh"

using namespace imsrg_util;

int main(int argc, char** argv)
{
  // Default parameters, and everything passed by command line args.
  Parameters parameters(argc,argv);
  if (parameters.help_mode) return 0;

  string inputtbme = parameters.s("2bme");
  string input3bme = parameters.s("3bme");
  string reference = parameters.s("reference");
  string valence_space = parameters.s("valence_space");
  string basis = parameters.s("basis");
  string method = parameters.s("method");
  string flowfile = parameters.s("flowfile");
  string intfile = parameters.s("intfile");
  string core_generator = parameters.s("core_generator");
  string valence_generator = parameters.s("valence_generator");
  string fmt2 = parameters.s("fmt2");
  string denominator_delta_orbit = parameters.s("denominator_delta_orbit");
  string LECs = parameters.s("LECs");
  string scratch = parameters.s("scratch");
  string use_brueckner_bch = parameters.s("use_brueckner_bch");
  string valence_file_format = parameters.s("valence_file_format");
  string occ_file = parameters.s("occ_file");

  int eMax = parameters.i("emax");
  int E3max = parameters.i("e3max");
  int lmax3 = parameters.i("lmax3");
  int targetMass = parameters.i("A");
  int nsteps = parameters.i("nsteps");
  int file2e1max = parameters.i("file2e1max");
  int file2e2max = parameters.i("file2e2max");
  int file2lmax = parameters.i("file2lmax");
  int file3e1max = parameters.i("file3e1max");
  int file3e2max = parameters.i("file3e2max");
  int file3e3max = parameters.i("file3e3max");

  double hw = parameters.d("hw");
  double smax = parameters.d("smax");
  double ode_tolerance = parameters.d("ode_tolerance");
  double dsmax = parameters.d("dsmax");
  double ds_0 = parameters.d("ds_0");
  double domega = parameters.d("domega");
  double omega_norm_max = parameters.d("omega_norm_max"); 
  double denominator_delta = parameters.d("denominator_delta");
  double BetaCM = parameters.d("BetaCM");

  vector<string> opnames = parameters.v("Operators");

  vector<Operator> ops;



  // test 2bme file
  ifstream test(inputtbme);
  if( not test.good() )
  {
    cout << "trouble reading " << inputtbme << " exiting. " << endl;
    return 1;
  }
  test.close();
  // test 3bme file
  if (input3bme != "none")
  {
    test.open(input3bme);
    if( not test.good() )
    {
      cout << "trouble reading " << input3bme << " exiting. " << endl;
      return 1;
    }
    test.close();
  }



  ReadWrite rw;
  rw.SetLECs_preset(LECs);
  rw.SetScratchDir(scratch);
  ModelSpace modelspace = reference=="default" ? ModelSpace(eMax,valence_space) : ModelSpace(eMax,reference,valence_space);
  if (occ_file != "none" and occ_file != "" )
  {
    modelspace.Init_occ_from_file(eMax,valence_space,occ_file);
  }

  if (nsteps < 0)
    nsteps = modelspace.valence.size()>0 ? 2 : 1;


  modelspace.SetHbarOmega(hw);
  if (targetMass>0)
     modelspace.SetTargetMass(targetMass);
  modelspace.SetE3max(E3max);
  if (lmax3>0)
     modelspace.SetLmax3(lmax3);
  
  cout << "Making the operator..." << endl;
  int particle_rank = input3bme=="none" ? 2 : 3;
  Operator Hbare = Operator(modelspace,0,0,0,particle_rank);
  Hbare.SetHermitian();

  if (use_brueckner_bch == "true" or use_brueckner_bch == "True")
  {
    Hbare.SetUseBruecknerBCH(true);
    cout << "Using Brueckner flavor of BCH" << endl;
  }

  cout << "Reading interactions..." << endl;

  #pragma omp parallel sections 
  {
    #pragma omp section
    {
    if (fmt2 == "me2j")
      rw.ReadBareTBME_Darmstadt(inputtbme, Hbare,file2e1max,file2e2max,file2lmax);
    else if (fmt2 == "navratil" or fmt2 == "Navratil")
      rw.ReadBareTBME_Navratil(inputtbme, Hbare);
    else if (fmt2 == "oslo" )
      rw.ReadTBME_Oslo(inputtbme, Hbare);
    else if (fmt2 == "oakridge" )
      rw.ReadTBME_OakRidge(inputtbme, Hbare);
     cout << "done reading 2N" << endl;
    }
  
    #pragma omp section
    if (Hbare.particle_rank >=3)
    {
      rw.Read_Darmstadt_3body(input3bme, Hbare, file3e1max,file3e2max,file3e3max);
      cout << "done reading 3N" << endl;
    }  
  }

  Hbare += Trel_Op(modelspace);
  if (abs(BetaCM)>1e-3)
  {
    Hbare += BetaCM * HCM_Op(modelspace);
  }

  HartreeFock hf(Hbare);
  hf.Solve();
  cout << "EHF = " << hf.EHF << endl;
  
  if (basis == "HF" and method !="HF")
    Hbare = hf.GetNormalOrderedH();
  else if (basis == "oscillator")
    Hbare = Hbare.DoNormalOrdering();

  Hbare -= BetaCM * 1.5*hw;
  cout << "Hbare 0b = " << Hbare.ZeroBody << endl;

  if (method != "HF")
  {
    cout << "Perturbative estimates of gs energy:" << endl;
    double EMP2 = Hbare.GetMP2_Energy();
    cout << "EMP2 = " << EMP2 << endl; 
    double EMP3 = Hbare.GetMP3_Energy();
    cout << "EMP3 = " << EMP3 << endl; 
    cout << "To 3rd order, E = " << Hbare.ZeroBody+EMP2+EMP3 << endl;
  }

  // Calculate all the desired operators
  for (auto& opname : opnames)
  {
    ops.emplace_back( imsrg_util::OperatorFromString(modelspace,opname) );
  }

  for (auto& op : ops)
  {
     if (basis == "HF") op = hf.TransformToHFBasis(op);
     op = op.DoNormalOrdering();
     if (method == "MP3")
     {
       double dop = op.MP1_Eval( Hbare );
       cout << "Operator 1st order correction  " << dop << "  ->  " << op.ZeroBody + dop << endl;
     }
  }
  auto itR2p = find(opnames.begin(),opnames.end(),"Rp2");
  if (itR2p != opnames.end())
  {
    Operator& Rp2 = ops[itR2p-opnames.begin()];
    int Z = modelspace.GetTargetZ();
    int A = modelspace.GetTargetMass();
    cout << " HF point proton radius = " << sqrt( Rp2.ZeroBody ) << endl; 
    cout << " HF charge radius = " << sqrt( Rp2.ZeroBody + r2p + r2n*(A-Z)/Z + DF) << endl; 
  }
  for (index_t i=0;i<ops.size();++i)
  {
    cout << opnames[i] << " = " << ops[i].ZeroBody << endl;
  }
  
  if ( method == "HF" or method == "MP3")
  {
    Hbare.PrintTimes();
    return 0;
  }

  IMSRGSolver imsrgsolver(Hbare);
  imsrgsolver.SetReadWrite(rw);
  
  if (method == "NSmagnus") // "No split" magnus
  {
    omega_norm_max=500;
    method = "magnus";
  }

  imsrgsolver.SetMethod(method);
  imsrgsolver.SetHin(Hbare);
  imsrgsolver.SetSmax(smax);
  imsrgsolver.SetFlowFile(flowfile);
  imsrgsolver.SetDs(ds_0);
  imsrgsolver.SetDsmax(dsmax);
  imsrgsolver.SetDenominatorDelta(denominator_delta);
  imsrgsolver.SetdOmega(domega);
  imsrgsolver.SetOmegaNormMax(omega_norm_max);
  imsrgsolver.SetODETolerance(ode_tolerance);
  if (denominator_delta_orbit != "none")
    imsrgsolver.SetDenominatorDeltaOrbit(denominator_delta_orbit);

  if (nsteps > 1) // two-step decoupling, do core first
  {
    imsrgsolver.SetGenerator(core_generator);
    imsrgsolver.Solve();
    if (method == "magnus") smax *= 2;
  }

  imsrgsolver.SetGenerator(valence_generator);
  imsrgsolver.SetSmax(smax);
  imsrgsolver.Solve();


  // Transform all the operators
  if (method == "magnus")
  {
    if (ops.size()>0) cout << "transforming operators" << endl;
    for (size_t i=0;i<ops.size();++i)
    {
      cout << opnames[i] << " " << flush;
      ops[i] = imsrgsolver.Transform(ops[i]);
      cout << " (" << ops[i].ZeroBody << " ) " << endl; 
    }
    cout << endl;
    // increase smax in case we need to do additional steps
    smax *= 1.5;
    imsrgsolver.SetSmax(smax);
  }


  // If we're doing targeted normal ordering 
  // we now re-normal order wrt to the core
  // and do any remaining flow.
  ModelSpace ms2(modelspace);
  bool renormal_order = false;
  if (modelspace.valence.size() > 0 )
  {
    renormal_order = modelspace.holes.size() != modelspace.core.size();
    if (not renormal_order)
    {
      for (auto c : modelspace.core)
      {
         if ( (find( modelspace.holes.begin(), modelspace.holes.end(), c) == modelspace.holes.end()) or (abs(1-modelspace.holes[c])>1e-6))
         {
           renormal_order = true;
           break;
         }
      }
    }
  }
  if ( renormal_order )
  {

    Hbare = imsrgsolver.GetH_s();

    int nOmega = imsrgsolver.GetOmegaSize() + imsrgsolver.GetNOmegaWritten();
    cout << "Undoing NO wrt A=" << modelspace.GetAref() << " Z=" << modelspace.GetZref() << endl;
    Hbare = Hbare.UndoNormalOrdering();

    ms2.SetReference(ms2.core); // chage the reference determinant
    Hbare.SetModelSpace(ms2);

    cout << "Doing NO wrt A=" << ms2.GetAref() << " Z=" << ms2.GetZref() << "  norbits = " << ms2.GetNumberOrbits() << endl;
    Hbare = Hbare.DoNormalOrdering();

    imsrgsolver.SetHin(Hbare);
    imsrgsolver.SetEtaCriterion(1e-4);
    imsrgsolver.Solve();
    // Change operators to the new basis, then apply the rest of the transformation
    cout << "Final transformation on the operators..." << endl;
    for (auto& op : ops)
    {
      double ZeroBody_before = op.ZeroBody;
      op = op.UndoNormalOrdering();
      double ZeroBody_undo = op.ZeroBody;
      op.SetModelSpace(ms2);
      op = op.DoNormalOrdering();
      double ZeroBody_mid = op.ZeroBody;
      // transform using the remaining omegas
      op = imsrgsolver.Transform_Partial(op,nOmega);
      cout << ZeroBody_before << "   =>   " << ZeroBody_undo << "   =>   " << ZeroBody_mid<< "   =>   " << op.ZeroBody << endl;
    }
  }


  // Write the output

  // If we're doing a shell model interaction, write the
  // interaction files to disk.
  if (modelspace.valence.size() > 0)
  {
    if (valence_file_format == "antoine") // this is still being tested...
    {
      rw.WriteAntoine_int(imsrgsolver.GetH_s(),intfile+".ant");
      rw.WriteAntoine_input(imsrgsolver.GetH_s(),intfile+".inp",modelspace.GetAref(),modelspace.GetZref());
    }
    rw.WriteNuShellX_int(imsrgsolver.GetH_s(),intfile+".int");
    rw.WriteNuShellX_sps(imsrgsolver.GetH_s(),intfile+".sp");

    if (method == "magnus")
    {
       for (index_t i=0;i<ops.size();++i)
       {
          if ((ops[i].GetJRank()+ops[i].GetTRank()+ops[i].GetParity())<1)
          {
            rw.WriteNuShellX_op(ops[i],intfile+opnames[i]+".int");
          }
          else
          {
            rw.WriteTensorOneBody(intfile+opnames[i]+"_1b.op",ops[i],opnames[i]);
            rw.WriteTensorTwoBody(intfile+opnames[i]+"_2b.op",ops[i],opnames[i]);
          }
       }
    }
  }
  else // single ref. just print the zero body pieces out. (maybe check if its magnus?)
  {
    cout << "Core Energy = " << setprecision(6) << imsrgsolver.GetH_s().ZeroBody << endl;
    for (index_t i=0;i<ops.size();++i)
    {
      Operator& op = ops[i];
      cout << opnames[i] << " = " << ops[i].ZeroBody << endl;
      if ( opnames[i] == "Rp2" )
      {
         int Z = modelspace.GetTargetZ();
         int A = modelspace.GetTargetMass();
         cout << " IMSRG point proton radius = " << sqrt( op.ZeroBody ) << endl; 
         cout << " IMSRG charge radius = " << sqrt( op.ZeroBody + r2p + r2n*(A-Z)/Z + DF) << endl; 
      }
    }
  }


  Hbare.PrintTimes();
 
  return 0;
}

