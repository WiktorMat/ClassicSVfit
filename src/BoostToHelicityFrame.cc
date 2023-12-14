#include "TauAnalysis/ClassicSVfitLT/interface/BoostToHelicityFrame.h"

#include <vector> // std::vector<>

using namespace classic_svFit;

namespace
{
  LorentzVector
  getP4_rf(const LorentzVector& p4, const ROOT::Math::Boost& boost)
  {
    // CV: boost given four-vector to restframe
    reco::Candidate::LorentzVector p4_rf = boost(p4);
    return p4_rf;
  }

  Vector
  get_k(const LorentzVector& p4, const ROOT::Math::Boost& boost_ttrf)
  {
    LorentzVector p4_ttrf = getP4_rf(p4, boost_ttrf);
    Vector k = p4_ttrf.Vect().unit();
    return k;
  }

  Vector
  get_h(const LorentzVector& beamP4, const ROOT::Math::Boost& boost_ttrf)
  {
    reco::Candidate::LorentzVector beamP4_ttrf = getP4_rf(beamP4, boost_ttrf);
    Vector h = beamP4_ttrf.Vect().unit();
    return h;
  }

  Vector
  get_r(const Vector& k, const Vector& h)
  {
    double cosTheta = k.Dot(h);
    // CV: allow for small rounding errors
    if ( cosTheta < -1.01 || cosTheta > +1.01 )
    {
      std::cerr << "ERROR: cosTheta = " << cosTheta << " outside physical range !!\n";
      assert(0);
    }
    if ( cosTheta < -1. ) cosTheta = -1.;
    if ( cosTheta > +1. ) cosTheta = +1.;
    double sinTheta = std::sqrt(1. - cosTheta*cosTheta);
    Vector r = (h - k*cosTheta)*(1./sinTheta);
    return r;
  }

  Vector
  get_n(const Vector& k, const Vector& r)
  {
    // CV: The ordering of r and k in the cross product has been agreed with Luca on 06/09/2023.
    //     The definition n = r x k has been chosen for consistency with Eq. (2.5) in the paper arXiv:1508.05271,
    //     which Luca and Marco have used in their previous papers on Entanglement.
    //    (Whether one computes the vector n using n = r x k or using n = p x k makes no difference:
    //     in both cases, the vector n refers to the direction perpendicular to the scattering plane
    //     and the vectors { n, r, k } define a right-handed coordinate system)
    reco::Candidate::Vector n = r.Cross(k);
    return n;
  }

  void
  get_localCoordinateSystem(const LorentzVector& tauP4, const ROOT::Math::Boost& boost_ttrf,
                            const LorentzVector& beamP4, 
                            Vector& r, Vector& n, Vector& k)
  {
    k = get_k(tauP4, boost_ttrf);
    Vector h = get_h(beamP4, boost_ttrf);
    r = get_r(k, h);
    n = get_n(k, r);
  }

  LorentzVector
  getP4_hf(const LorentzVector& p4, const Vector& r, const Vector& n, Vector& k)
  {
    // CV: rotate given four-vector to helicity frame
    reco::Candidate::Vector p3 = p4.Vect();
    double Pr = p3.Dot(r);
    double Pn = p3.Dot(n);
    double Pk = p3.Dot(k);
    reco::Candidate::LorentzVector p4_hf(Pr, Pn, Pk, p4.energy());
    return p4_hf;
  }
}

BoostToHelicityFrame::BoostToHelicityFrame()
{
  // CV: set beam energy and mass of beam particles to LHC Run 2 values
  //    (both are used to define the beam axis and their numerical values should not really matter)
  const double beamE         = 7.e+3;    // [GeV]
  const double mBeamParticle = 0.938272; // [GeV]
  double beamPx = 0.;
  double beamPy = 0.;
  double beamPz = std::sqrt(square(beamEnergy) - square(mBeamParticle));
  beamP4_ = LorentzVector(beamPx, beamPy, beamPz, beamE);
}

BoostToHelicityFrame::~BoostToHelicityFrame()
{}

void
BoostToHelicityFrame::setFittedTauLeptons(const FittedTauLepton& fittedTauLepton1, const FittedTauLepton& fittedTauLepton2)
{
  std::vector<const FittedTauLepton*> fittedTauLeptons;
  fittedTauLeptons.push_back(&fittedTauLepton1);
  fittedTauLeptons.push_back(&fittedTauLepton2);
  const FittedTauLepton* fittedTauPlus = nullptr;
  const FittedTauLepton* fittedTauMinus = nullptr;
  for ( const FittedTauLepton* fittedTauLepton : fittedTauLeptons )
  {
    if ( fittedTauLepton->getMeasuredTauLepton().charge() > 0 )
    {
      fittedTauPlus = fittedTauLepton;
    }
    else if ( fittedTauLepton->getMeasuredTauLepton().charge() < 0 )
    {
      fittedTauMinus = &fittedTauLepton;
    }
  }
  assert(fittedTauPlus && fittedTauMinus);
  LorentzVector tauPlusP4 = fittedTauPlus->tauP4();
  LorentzVector tauMinusP4 = fittedTauMinus->tauP4();
  LorentzVector diTauP4 = tauPlusP4 + tauMinusP4;

  boost_ttrf_ = ROOT::Math::Boost(diTauP4.BoostToCM());
  get_localCoordinateSystem(tauMinusP4, boost_ttrf_, beamP4_, r_, n_, k_);

  LorentzVector tauPlusP4_ttrf = getP4_rf(tauPlusP4, boost_ttrf_);
  LorentzVector tauPlusP4_hf = getP4_hf(tauPlusP4_ttrf, r_, n_, k_);
  boost_tprf_ = ROOT::Math::Boost(tauPlusP4_hf.BoostToCM());

  LorentzVector tauMinusP4_ttrf = getP4_rf(tauMinusP4, boost_ttrf_);
  LorentzVector tauMinusP4_hf = getP4_hf(tauMinusP4_ttrf, r_, n_, k_);
  boost_tmrf_ = ROOT::Math::Boost(tauMinusP4_hf.BoostToCM());
}

classic_svFit::LorentzVector
BoostToHelicityFrame::operator()(const classic_svFit::LorentzVector& p4)
{
  // CV: boost given four-vector to restframe of tau pair,
  //     rotate to helicity frame,
  //     and finally boost to tau restframe
  LorentzVector p4_ttrf = getP4_rf(p4, boost_ttrf_);
  LorentzVector p4_hf = getP4_hf(p4_ttrf, r_, n_, k_);
  ROOT::Math::Boost* boost_trf = nullptr;
  if      ( tau == kTauPlus  ) boost_trf = &boost_tprf_;
  else if ( tau == kTauMinus ) boost_trf = &boost_tmrf_;
  assert(boost_trf);
  LorentzVector p4_trf = getP4_rf(p4_hf, *boost_trf);
  return p4_trf;
}
