#include "ModelSpace.hh"
#include "ReadWrite.hh"
#include "Operator.hh"
#include "HartreeFock.hh"
#include "IMSRGSolver.hh"
#include "imsrg_util.hh"
#include "AngMom.hh"
#include <string>

#include <boost/python/module.hpp>
#include <boost/python/def.hpp>
#include <boost/python.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>


using namespace boost::python;

  Orbit MSGetOrbit(ModelSpace& self, int i){ return self.GetOrbit(i);};
  TwoBodyChannel MSGetTwoBodyChannel(ModelSpace& self, int ch){return self.GetTwoBodyChannel(ch);};

  double TB_GetTBME_J(TwoBodyME& self,int j_bra, int j_ket, int a, int b, int c, int d){return self.GetTBME_J(j_bra,j_ket,a,b,c,d);};
  double TB_GetTBME_J_norm(TwoBodyME& self,int j_bra, int j_ket, int a, int b, int c, int d){return self.GetTBME_J_norm(j_bra,j_ket,a,b,c,d);};

  int TBCGetLocalIndex(TwoBodyChannel& self, int p, int q){ return self.GetLocalIndex( p, q);};

  void ArmaMatPrint( arma::mat& self){ self.print();};

BOOST_PYTHON_MODULE(pyIMSRG)
{

   class_<vector<string> > ("vector_string")
      .def (vector_indexing_suite< vector<string> >())
   ;

   class_<Orbit>("Orbit",init<>())
      .def_readwrite("n", &Orbit::n)
      .def_readwrite("l", &Orbit::l)
      .def_readwrite("j2", &Orbit::j2)
      .def_readwrite("tz2", &Orbit::tz2)
      .def_readwrite("occ", &Orbit::occ)
      .def_readwrite("cvq", &Orbit::cvq)
      .def_readwrite("index", &Orbit::index)
   ;

   class_<TwoBodyChannel>("TwoBodyChannel",init<>())
      .def("GetNumberKets",&TwoBodyChannel::GetNumberKets)
      .def("GetLocalIndex",&TBCGetLocalIndex)
      .def("GetKetIndex",&TwoBodyChannel::GetKetIndex)
   ;

   class_<ModelSpace>("ModelSpace",init<>())
      .def(init<const ModelSpace&>())
      .def(init< int, const std::string&>())
      .def(init< int, const std::string&, const std::string&>())
      .def(init< int,vector<string>,vector<string> >())
      .def(init< int,vector<string>,vector<string>,vector<string> >())
      .def("SetHbarOmega", &ModelSpace::SetHbarOmega)
      .def("SetTargetMass", &ModelSpace::SetTargetMass)
      .def("SetE3max", &ModelSpace::SetE3max)
      .def("GetHbarOmega", &ModelSpace::GetHbarOmega)
      .def("GetTargetMass", &ModelSpace::GetTargetMass)
      .def("GetNumberOrbits", &ModelSpace::GetNumberOrbits)
      .def("GetNumberKets", &ModelSpace::GetNumberKets)
      .def("GetOrbit", &MSGetOrbit)
      .def("GetTwoBodyChannelIndex", &ModelSpace::GetTwoBodyChannelIndex)
      .def("GetTwoBodyChannel", &MSGetTwoBodyChannel)
      .def("Index2String",&ModelSpace::Index2String)
   ;


   class_<Operator>("Operator",init<>())
      .def(init< ModelSpace&>())
      .def(init< ModelSpace&,int,int,int,int>())
      .def(self += Operator())
      .def(self + Operator())
      .def(self -= Operator())
      .def(self - Operator())
      .def( - self)
      .def(self *= double())
      .def(self * double())
      .def(self /= double())
      .def(self / double())
      .def(self += double())
      .def(self + double())
      .def(self -= double())
      .def(self - double())
      .def_readwrite("ZeroBody", &Operator::ZeroBody)
      .def_readwrite("OneBody", &Operator::OneBody)
      .def_readwrite("TwoBody", &Operator::TwoBody)
      .def("GetOneBody", &Operator::GetOneBody)
      .def("SetOneBody", &Operator::SetOneBody)
      .def("GetTwoBody", &Operator::GetTwoBody)
      .def("SetTwoBody", &Operator::SetTwoBody)
      .def("GetTwoBodyDimension", &Operator::GetTwoBodyDimension)
      .def("ScaleOneBody", &Operator::ScaleOneBody)
      .def("ScaleTwoBody", &Operator::ScaleTwoBody)
      .def("DoNormalOrdering", &Operator::DoNormalOrdering)
      .def("UndoNormalOrdering", &Operator::UndoNormalOrdering)
      .def("SetModelSpace", &Operator::SetModelSpace)
//      .def("CalculateKineticEnergy", &Operator::CalculateKineticEnergy)
      .def("Norm", &Operator::Norm)
      .def("OneBodyNorm", &Operator::OneBodyNorm)
      .def("TwoBodyNorm", &Operator::TwoBodyNorm)
      .def("SetHermitian", &Operator::SetHermitian)
      .def("SetAntiHermitian", &Operator::SetAntiHermitian)
      .def("SetNonHermitian", &Operator::SetNonHermitian)
      .def("Set_BCH_Transform_Threshold", &Operator::Set_BCH_Transform_Threshold)
      .def("Set_BCH_Product_Threshold", &Operator::Set_BCH_Product_Threshold)
      .def("PrintOneBody", &Operator::PrintOneBody)
      .def("PrintTwoBody", &Operator::PrintTwoBody)
      .def("GetParticleRank", &Operator::GetParticleRank)
      .def("GetE3max", &Operator::GetE3max)
      .def("SetE3max", &Operator::SetE3max)
      .def("PrintTimes", &Operator::PrintTimes)
      .def("BCH_Transform", &Operator::BCH_Transform)
//      .def("EraseThreeBody", &Operator::EraseThreeBody)
      .def("Size", &Operator::Size)
      .def("SetToCommutator", &Operator::SetToCommutator)
      .def("comm110ss", &Operator::comm110ss)
      .def("comm220ss", &Operator::comm220ss)
      .def("comm111ss", &Operator::comm111ss)
      .def("comm121ss", &Operator::comm121ss)
      .def("comm221ss", &Operator::comm221ss)
      .def("comm122ss", &Operator::comm122ss)
      .def("comm222_pp_hh_221ss", &Operator::comm222_pp_hh_221ss)
      .def("comm222_phss", &Operator::comm222_phss)
      .def("comm111st", &Operator::comm111st)
      .def("comm121st", &Operator::comm121st)
      .def("comm122st", &Operator::comm122st)
      .def("comm222_pp_hh_221st", &Operator::comm222_pp_hh_221st)
      .def("comm222_phst", &Operator::comm222_phst)
   ;

   class_<arma::mat>("ArmaMat",init<>())
      .def("Print",&ArmaMatPrint)
      .def(self *= double())
      .def(self * double())
      .def(self /= double())
      .def(self / double())
      .def(self += double())
      .def(self + double())
      .def(self -= double())
      .def(self - double())
   ;

   class_<TwoBodyME>("TwoBodyME",init<>())
      .def("GetTBME_J", TB_GetTBME_J)
      .def("GetTBME_J_norm", TB_GetTBME_J_norm)
   ;

   class_<ReadWrite>("ReadWrite",init<>())
      .def("ReadSettingsFile", &ReadWrite::ReadSettingsFile)
      .def("ReadTBME_Oslo", &ReadWrite::ReadTBME_Oslo)
      .def("ReadBareTBME_Jason", &ReadWrite::ReadBareTBME_Jason)
      .def("ReadBareTBME_Navratil", &ReadWrite::ReadBareTBME_Navratil)
      .def("ReadBareTBME_Darmstadt", &ReadWrite::ReadBareTBME_Darmstadt)
      .def("Read_Darmstadt_3body", &ReadWrite::Read_Darmstadt_3body)
      .def("Read3bodyHDF5", &ReadWrite::Read3bodyHDF5)
      .def("Write_me2j", &ReadWrite::Write_me2j)
      .def("Write_me3j", &ReadWrite::Write_me3j)
      .def("WriteTBME_Navratil", &ReadWrite::WriteTBME_Navratil)
      .def("WriteNuShellX_sps", &ReadWrite::WriteNuShellX_sps)
      .def("WriteNuShellX_int", &ReadWrite::WriteNuShellX_int)
      .def("WriteNuShellX_op", &ReadWrite::WriteNuShellX_op)
      .def("ReadNuShellX_int", &ReadWrite::ReadNuShellX_int)
      .def("WriteAntoine_int", &ReadWrite::WriteAntoine_int)
      .def("WriteAntoine_input", &ReadWrite::WriteAntoine_input)
      .def("WriteOperator", &ReadWrite::WriteOperator)
      .def("WriteOperatorHuman", &ReadWrite::WriteOperatorHuman)
      .def("ReadOperator", &ReadWrite::ReadOperator)
      .def("CompareOperators", &ReadWrite::CompareOperators)
      .def("ReadOneBody_Takayuki", &ReadWrite::ReadOneBody_Takayuki)
      .def("ReadTwoBody_Takayuki", &ReadWrite::ReadTwoBody_Takayuki)
      .def("WriteOneBody_Takayuki", &ReadWrite::WriteOneBody_Takayuki)
      .def("WriteTwoBody_Takayuki", &ReadWrite::WriteTwoBody_Takayuki)
      .def("WriteTensorOneBody", &ReadWrite::WriteTensorOneBody)
      .def("WriteTensorTwoBody", &ReadWrite::WriteTensorTwoBody)
      .def("WriteOneBody_Oslo", &ReadWrite::WriteOneBody_Oslo)
      .def("WriteTwoBody_Oslo", &ReadWrite::WriteTwoBody_Oslo)
      .def("SetCoMCorr", &ReadWrite::SetCoMCorr)
      .def("ReadTwoBodyEngel", &ReadWrite::ReadTwoBodyEngel)
      .def("ReadOperator_Nathan",&ReadWrite::ReadOperator_Nathan)
      .def("ReadTensorOperator_Nathan",&ReadWrite::ReadTensorOperator_Nathan)
      .def_readwrite("InputParameters", &ReadWrite::InputParameters)
   ;





   class_<HartreeFock>("HartreeFock",init<Operator&>())
      .def("Solve",&HartreeFock::Solve)
      .def("TransformToHFBasis",&HartreeFock::TransformToHFBasis)
      .def("GetHbare",&HartreeFock::GetHbare)
      .def("GetNormalOrderedH",&HartreeFock::GetNormalOrderedH)
      .def("GetOmega",&HartreeFock::GetOmega)
      .def("PrintSPE",&HartreeFock::PrintSPE)
      .def_readonly("EHF",&HartreeFock::EHF)
   ;

   // Define which overloaded version of IMSRGSolver::Transform I want to expose
   Operator (IMSRGSolver::*Transform_ref)(Operator&) = &IMSRGSolver::Transform;

   class_<IMSRGSolver>("IMSRGSolver",init<Operator&>())
      .def("Solve",&IMSRGSolver::Solve)
      .def("Transform",Transform_ref)
      .def("InverseTransform",&IMSRGSolver::InverseTransform)
      .def("SetFlowFile",&IMSRGSolver::SetFlowFile)
      .def("SetMethod",&IMSRGSolver::SetMethod)
      .def("SetEtaCriterion",&IMSRGSolver::SetEtaCriterion)
      .def("SetDs",&IMSRGSolver::SetDs)
      .def("SetdOmega",&IMSRGSolver::SetdOmega)
      .def("SetOmegaNormMax",&IMSRGSolver::SetOmegaNormMax)
      .def("SetSmax",&IMSRGSolver::SetSmax)
      .def("SetDsmax",&IMSRGSolver::SetDsmax)
      .def("SetHin",&IMSRGSolver::SetHin)
      .def("SetODETolerance",&IMSRGSolver::SetODETolerance)
      .def("Reset",&IMSRGSolver::Reset)
      .def("SetGenerator",&IMSRGSolver::SetGenerator)
      .def("SetDenominatorCutoff",&IMSRGSolver::SetDenominatorCutoff)
      .def("SetDenominatorDelta",&IMSRGSolver::SetDenominatorDelta)
      .def("SetDenominatorDeltaOrbit",&IMSRGSolver::SetDenominatorDeltaOrbit)
      .def("GetSystemDimension",&IMSRGSolver::GetSystemDimension)
      .def("GetOmega",&IMSRGSolver::GetOmega)
      .def("GetH_s",&IMSRGSolver::GetH_s,return_value_policy<reference_existing_object>())
      .def("SetMagnusAdaptive",&IMSRGSolver::SetMagnusAdaptive)
      .def_readwrite("Eta", &IMSRGSolver::Eta)
   ;



   class_<IMSRGProfiler>("IMSRGProfiler",init<>())
       .def("PrintTimes",&IMSRGProfiler::PrintTimes)
       .def("PrintCounters",&IMSRGProfiler::PrintCounters)
       .def("PrintAll",&IMSRGProfiler::PrintAll)
       .def("PrintMemory",&IMSRGProfiler::PrintMemory)
   ;





   def("TCM_Op",           imsrg_util::TCM_Op);
   def("Trel_Op",           imsrg_util::Trel_Op);
   def("R2CM_Op",          imsrg_util::R2CM_Op);
   def("HCM_Op",           imsrg_util::HCM_Op);
   def("NumberOp",         imsrg_util::NumberOp);
   def("RSquaredOp",       imsrg_util::RSquaredOp);
   def("RpSpinOrbitCorrection", imsrg_util::RpSpinOrbitCorrection);
   def("E0Op",             imsrg_util::E0Op);
   def("AllowedFermi_Op",             imsrg_util::AllowedFermi_Op);
   def("AllowedGamowTeller_Op",             imsrg_util::AllowedGamowTeller_Op);
   def("ElectricMultipoleOp",             imsrg_util::ElectricMultipoleOp);
   def("MagneticMultipoleOp",             imsrg_util::MagneticMultipoleOp);
   def("Sigma_Op", imsrg_util::Sigma_Op);
   def("Isospin2_Op",      imsrg_util::Isospin2_Op);
   def("LdotS_Op",         imsrg_util::LdotS_Op);
   def("HO_density",       imsrg_util::HO_density);
   def("GetOccupationsHF", imsrg_util::GetOccupationsHF);
   def("GetOccupations",   imsrg_util::GetOccupations);
   def("GetDensity",       imsrg_util::GetDensity);
   def("CommutatorTest",   imsrg_util::CommutatorTest);
   def("Calculate_p1p2_all",   imsrg_util::Calculate_p1p2_all);
   def("Single_Ref_1B_Density_Matrix", imsrg_util::Single_Ref_1B_Density_Matrix);
   def("Get_Charge_Density", imsrg_util::Get_Charge_Density);
   def("Embed1BodyIn2Body",  imsrg_util::Embed1BodyIn2Body);

   def("CG",AngMom::CG);
   def("ThreeJ",AngMom::ThreeJ);
   def("SixJ",AngMom::SixJ);
   def("NineJ",AngMom::NineJ);
   def("NormNineJ",AngMom::NormNineJ);
   def("Moshinsky",AngMom::Moshinsky);



}
