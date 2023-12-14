#ifndef TauAnalysis_ClassicSVfit_PolVecAlgoThreeProng0Pi0_h
#define TauAnalysis_ClassicSVfit_PolVecAlgoThreeProng0Pi0_h

#include "TauAnalysis/ClassicSVfit/interface/BoostToHelicityFrame.h"                 // BoostToHelicityFrame
#include "TauAnalysis/ClassicSVfit/interface/MeasuredTauLepton.h"                    // MeasuredTauLepton
#include "TauAnalysis/ClassicSVfit/interface/svFitAuxFunctions.h"                    // Vector

#include "TauAnalysis/PolarimetricVectorTau2a1/interface/PolarimetricVectorTau2a1.h" // PolarimetricVectorTau2a1 

namespace classic_svFit
{
  class PolVecAlgoThreeProng0Pi0
  {
   public:
    PolVecAlgoThreeProng0Pi0();
    ~PolVecAlgoThreeProng0Pi0();

    Vector
    operator()(const MeasuredTauLepton& measuredTauLepton, int tau,
               const BoostToHelicityFrame& boostToHelicityFrame) const;

   private:
    PolarimetricVectorTau2a1 a1pol_;
  };
}

#endif