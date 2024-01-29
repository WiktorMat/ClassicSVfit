#include "TauAnalysis/ClassicSVfit/interface/ClassicSVfitIntegrand.h"

#include "TauAnalysis/ClassicSVfit/interface/comp_PCA.h" // comp_PCA()

#include <TMath.h>                                       // TMath::Pi()
#include <TString.h>                                     // Form()
#include <TVectorD.h>                                    // TVectorD

#include <cmath>                                         // exp(), std::fabs(), std::sqrt()

namespace classic_svFit
{

/// global function pointer, needed for Markov Chain integration
const ClassicSVfitIntegrand* ClassicSVfitIntegrand::gSVfitIntegrand = 0;

ClassicSVfitIntegrand::ClassicSVfitIntegrand(int verbosity)
  : numTaus_(0)
  , fittedTauLepton1_(0, verbosity)
  , fittedTauLepton2_(1, verbosity)
  , leg1leadChargedHadron_(nullptr)
  , leg2leadChargedHadron_(nullptr)
  , isCentral_(true)
  , idxMEtSystematic_(0)
  , useTauFlightLength_(false)
  , const_FlightLength1_(0.)
  , const_FlightLength2_(0.)
  , diTauMassConstraint_(-1.)
  , addLogM_(false)
  , addLogM_power_(0.)
#ifdef USE_SVFITTF
  , rhoHadTau_(0.)
  , useHadTauTF_(false)
#endif
  , numDimensions_(0)
  , errorCode_(None)
  , probPS_(0.)
  , probFlightLength_(0.)
  , histogramAdapter_(nullptr)
  , verbosity_(verbosity)
{
  numTaus_ = 2;
  legIntegrationParams_.resize(numTaus_);

  maxNumberOfDimensions_ = 4*numTaus_;

  // CV: enable log(M) term with kappa = 6, unless explicitely requested by user otherwise,
  //     as this setting provides best compatibility with "old" SVfitStandalone algorithm
  addLogM_ = true;
  addLogM_power_ = 6.; 

  fittedTauLeptons_.resize(numTaus_);
  fittedTauLeptons_[0] = &fittedTauLepton1_;
  fittedTauLeptons_[1] = &fittedTauLepton2_;

  // set global function pointer to this
  gSVfitIntegrand = this;
}

ClassicSVfitIntegrand::~ClassicSVfitIntegrand()
{
#ifdef USE_SVFITTF
  for ( const HadTauTFBase* hadTauTF : hadTauTFs_ )
  {
    delete hadTauTF;
  }
#endif
}

void
ClassicSVfitIntegrand::setCentral()
{
  isCentral_ = true;
  idxMEtSystematic_ = 0;
}

void
ClassicSVfitIntegrand::setMEtSystematic(unsigned int idx)
{
  isCentral_ = false;
  idxMEtSystematic_ = idx;
}

void
ClassicSVfitIntegrand::enableTauFlightLength()
{
  useTauFlightLength_ = true;
}

void
ClassicSVfitIntegrand::disableTauFlightLength()
{
  useTauFlightLength_ = false;
}

void
ClassicSVfitIntegrand::enableDiTauMassConstraint(double diTauMass)
{
  diTauMassConstraint_ = diTauMass;
  diTauMassConstraint2_ = square(diTauMassConstraint_);
}

void
ClassicSVfitIntegrand::disableDiTauMassConstraint()
{
  diTauMassConstraint_ = -1.;
  diTauMassConstraint2_ = -1.;
}

void
ClassicSVfitIntegrand::enableLogM(double power)
{
  addLogM_ = true;
  addLogM_power_ = power;
}

void
ClassicSVfitIntegrand::disableLogM()
{
  addLogM_ = false;
  addLogM_power_ = 0.;
}

void
ClassicSVfitIntegrand::setHistogramAdapter(HistogramAdapterDiTau* histogramAdapter)
{
  histogramAdapter_ = histogramAdapter;
}

void
ClassicSVfitIntegrand::initializeLegIntegrationParams(size_t iLeg, const classic_svFit::integrationParameters& aParams)
{ 
  assert(iLeg < legIntegrationParams_.size());
  legIntegrationParams_[iLeg] = aParams;
}

void
ClassicSVfitIntegrand::setNumDimensions(unsigned int numDimensions) 
{ 
  assert(numDimensions <= maxNumberOfDimensions_);
  numDimensions_ = numDimensions;
  x_.resize(numDimensions_);
}

void
ClassicSVfitIntegrand::setVerbosity(int aVerbosity)
{
  verbosity_ = aVerbosity;
}

void
ClassicSVfitIntegrand::setIntegrationRanges(const std::vector<double>& xl, const std::vector<double>& xh)
{
  xMin_ = xl;
  xMax_ = xh;
}

#ifdef USE_SVFITTF
void
ClassicSVfitIntegrand::enableHadTauTF(const HadTauTFBase* hadTauTF, double rhoHadTau)
{
  for ( const HadTauTFBase* hadTauTF : hadTauTFs_ )
  {
    delete hadTauTF;
  }
  hadTauTFs_.clear();
  for ( size_t iTau = 0; iTau < numTaus_; ++iTau )
  {
    hadTauTFs_.push_back(hadTauTF->Clone(Form("leg%i" + iTau)));
  }
  rhoHadTau_ = rhoHadTau;
  useHadTauTF_ = true;
}

void
ClassicSVfitIntegrand::disableHadTauTF()
{
  useHadTauTF_ = false;
}
#endif

void
ClassicSVfitIntegrand::setMeasurement(const MeasuredEvent& measuredEvent)
{
  measuredEvent_ = measuredEvent;

  // reset all error codes
  errorCode_ = None;

  const std::vector<MeasuredTauLepton>& measuredTauLeptons = measuredEvent.tauLeptons();
  if ( measuredTauLeptons.size() != numTaus_ )
  {
    std::cerr << "ERROR: Number of MeasuredTauLeptons is not equal to " << numTaus_ << " !!" << std::endl;
    errorCode_ |= LeptonNumber;
  }

  measuredTauLepton1_ = measuredTauLeptons[0];
  fittedTauLepton1_.setMeasuredTauLepton(measuredTauLepton1_);
  leg1isLeptonicTauDecay_ = measuredTauLepton1_.isLeptonicTauDecay();
  leg1isHadronicTauDecay_ = measuredTauLepton1_.isHadronicTauDecay();
  leg1isPrompt_ = measuredTauLepton1_.isPrompt();
  measuredTauLepton2_ = measuredTauLeptons[1];
  fittedTauLepton2_.setMeasuredTauLepton(measuredTauLepton2_);
  leg2isLeptonicTauDecay_ = measuredTauLepton2_.isLeptonicTauDecay();
  leg2isHadronicTauDecay_ = measuredTauLepton2_.isHadronicTauDecay();
  leg2isPrompt_ = measuredTauLepton2_.isPrompt();
  
  mVis_measured_ = (measuredTauLepton1_.p4() + measuredTauLepton2_.p4()).mass();
  if ( verbosity_ >= 2 )
  {
    std::cout << "mVis(ditau) = " << mVis_measured_ << std::endl;
  }
  mVis2_measured_ = square(mVis_measured_);

#ifdef USE_SVFITTF
  if ( useHadTauTF_ )
  {
    for ( size_t iTau = 0; iTau < numTaus_; ++iTau )
    {
      const FittedTauLepton* fittedTauLepton = fittedTauLeptons_[iTau];
      const MeasuredTauLepton& measuredTauLepton = fittedTauLepton->getMeasuredTauLepton();
      if ( measuredTauLepton.isHadronicTauDecay() )
      {
	hadTauTFs_[iTau]->setDecayMode(measuredTauLepton.decayMode());
      }
    }
  }
#endif

  const std::vector<MeasuredMEt>& measuredMEt = measuredEvent_.MEt();
  for ( const MeasuredMEt& measuredMEt_i : measuredMEt )
  {
    if ( !measuredMEt_i.covInv_isValid() )
    {
      std::cerr << "ERROR: Failed to invert MET covariance matrix (det=0) !!" << std::endl;
      errorCode_ |= MatrixInversion;
    }
  }

  if ( useTauFlightLength_ )
  {
    bool hasPrimaryVertex = measuredEvent.hasPrimaryVertex();
    if ( !hasPrimaryVertex )
    {
      std::cerr << "ERROR: No primary vertex given !!" << std::endl;
      errorCode_ |= MissingVertex;
    }
    if ( hasPrimaryVertex && !measuredEvent.primaryVertexCovInv_isValid() )
    {
      std::cerr << "ERROR: Failed to invert covariance matrix of primary vertex (det=0) !!" << std::endl;
      errorCode_ |= MatrixInversion;
    }

    primaryVertex_ = measuredEvent.primaryVertex();

    bool hasDecayVertices = true;
    for ( const MeasuredTauLepton& measuredTauLepton : measuredTauLeptons )
    {
      if ( measuredTauLepton.isPrompt() ) continue;
      if ( !measuredTauLepton.hasDecayVertex() ) hasDecayVertices = false;
    }
    if ( !hasDecayVertices )
    {
      std::cerr << "ERROR: No decay vertex given !!" << std::endl;
      errorCode_ |= MissingVertex;
    }   
    if ( hasDecayVertices )
    {
      bool covInv_isValid = true;
      for ( const MeasuredTauLepton& measuredTauLepton : measuredTauLeptons )
      {
        if ( !measuredTauLepton.decayVertexCovInv_isValid() ) covInv_isValid = false;
      }
      if ( !covInv_isValid )
      {
        std::cerr << "ERROR: Failed to invert covariance matrix of decay vertex (det=0) !!" << std::endl;
        errorCode_ |= MatrixInversion;
      }
    }

    leg1decayVertexCov_.ResizeTo(3,3);
    leg1decayVertexCov_ = measuredTauLepton1_.decayVertexCov() + measuredEvent.primaryVertexCov();
    leg1decayVertexCovInv_.ResizeTo(3,3);
    bool errorFlag1 = false;
    leg1decayVertexCovInv_ = invertMatrix("leg1decayVertexCov", leg1decayVertexCov_, errorFlag1);
    leg2decayVertexCov_.ResizeTo(3,3);
    leg2decayVertexCov_ = measuredTauLepton2_.decayVertexCov() + measuredEvent.primaryVertexCov();
    leg2decayVertexCovInv_.ResizeTo(3,3);
    bool errorFlag2 = false;
    leg2decayVertexCovInv_ = invertMatrix("leg2decayVertexCov", leg2decayVertexCov_, errorFlag2);
    if ( errorFlag1 || errorFlag2 )
    {
      std::cerr << "ERROR: Failed to invert covariance matrix of decay vertex (det=0) !!" << std::endl;
      errorCode_ |= MatrixInversion;
    }

    const_FlightLength1_ = 1./(pow(2.*TMath::Pi(), 1.5)*std::sqrt(std::fabs(leg1decayVertexCov_.Determinant())));
    const_FlightLength2_ = 1./(pow(2.*TMath::Pi(), 1.5)*std::sqrt(std::fabs(leg2decayVertexCov_.Determinant())));
  
    leg1leadChargedHadron_ = measuredTauLepton1_.leadChargedHadron();
    leg2leadChargedHadron_ = measuredTauLepton2_.leadChargedHadron();
    if ( !((leg1leadChargedHadron_ || leg1isPrompt_) && (leg2leadChargedHadron_ || leg2isPrompt_)) )
    {
      std::cerr << "ERROR: Failed to find leading charged hadron !!" << std::endl;
      errorCode_ |= MissingLeadChargedHadron;
    }
  }

  probPS_ = 0.;
  probFlightLength_ = 0.;
}

double
ClassicSVfitIntegrand::Eval(const double* q) const
{
  // in case of initialization errors don't start to do anything
  if ( errorCode_ & MatrixInversion          || 
       errorCode_ & LeptonNumber             || 
       errorCode_ & MissingVertex            ||
       errorCode_ & MissingLeadChargedHadron ) 
  {
    return 0.; 
  }

  rescaleX(q);

  if ( isCentral_ || idxMEtSystematic_ == 0 )
  {
    probPS_ = EvalPS();
    if ( errorCode_ & TauDecayParameters ) return 0.;
    probFlightLength_ = EvalFlightLength();
  }
  if ( (probPS_*probFlightLength_) < 1.e-300 ) return 0.;

  const std::vector<MeasuredMEt>& measuredMEt = measuredEvent_.MEt();
  unsigned int idxMeasuredMEt = ( isCentral_ ) ? 0 : idxMEtSystematic_ + 1;
  assert(idxMeasuredMEt < measuredMEt.size());
  double probMEtTF = EvalMEtTF(measuredMEt[idxMeasuredMEt]);

  double prob = probPS_*probFlightLength_*probMEtTF;
  if ( histogramAdapter_ && prob > 1.e-300 )
  {
    histogramAdapter_->setFittedTauLeptons(fittedTauLepton1_, fittedTauLepton2_);
  }
  return prob;
}

void
ClassicSVfitIntegrand::rescaleX(const double* q) const
{
  for ( unsigned int iDimension = 0; iDimension < numDimensions_; ++iDimension )
  {
    const double& q_i = q[iDimension];
    x_[iDimension] = (1. - q_i)*xMin_[iDimension] + q_i*xMax_[iDimension];
  }
}

double
ClassicSVfitIntegrand::EvalPS() const
{
  if ( verbosity_ >= 2 )
  {
    std::cout << "<ClassicSVfitIntegrand::EvalPS(const double*)>:" << std::endl;
    std::cout << " x = { ";
    for ( unsigned int iDimension = 0; iDimension < numDimensions_; ++iDimension )
    {
      std::cout << x_[iDimension];
      if ( iDimension < (numDimensions_ - 1) ) std::cout << ", ";
    }
    std::cout << " }" << std::endl;
  }

  // reset 'TauDecayParameters' error code
  errorCode_ &= (errorCode_ ^ TauDecayParameters);

  double visPtShift1 = 1.;
  double visPtShift2 = 1.;
#ifdef USE_SVFITTF
  int idx_visPtShift1 = legIntegrationParams_[0].idx_VisPtShift_;
  int idx_visPtShift2 = legIntegrationParams_[1].idx_VisPtShift_;
  if( useHadTauTF_ && idx_visPtShift1 != -1 && !leg1isLeptonicTauDecay_ ) visPtShift1 = (1./x_[idx_visPtShift1]);
  if( useHadTauTF_ && idx_visPtShift2 != -1 && !leg2isLeptonicTauDecay_ ) visPtShift2 = (1./x_[idx_visPtShift2]);
#endif
  if ( visPtShift1 < 1.e-2 || visPtShift2 < 1.e-2 ) return 0.;

  // scale momenta of visible tau decays products
  fittedTauLepton1_.updateVisMomentum(visPtShift1);
  fittedTauLepton2_.updateVisMomentum(visPtShift2);

  // compute visible energy fraction for first tau
  double x1_dash = 1.;
  if ( !leg1isPrompt_ )
  {
    int idx_x1 = legIntegrationParams_[0].idx_X_;
    assert(idx_x1 != -1);
    x1_dash = x_[idx_x1];
  }
  double x1 = x1_dash/visPtShift1;
  if ( !(x1 >= 1.e-5 && x1 <= 1.) ) return 0.;

  // compute neutrino and tau lepton momentum for first tau
  if ( !leg1isPrompt_ )
  {
    int idx_phiNu1 = legIntegrationParams_[0].idx_phi_;
    assert(idx_phiNu1 != -1);
    double phiNu1 = x_[idx_phiNu1];
    int idx_nu1Mass = legIntegrationParams_[0].idx_mNuNu_;
    double nu1Mass = ( idx_nu1Mass != -1 ) ? std::sqrt(x_[idx_nu1Mass]) : 0.;
    fittedTauLepton1_.updateTauMomentum(x1, phiNu1, nu1Mass);
    //std::cout << "fittedTauLepton1: errorCode = " << fittedTauLepton1_.errorCode() << std::endl;
    if ( fittedTauLepton1_.errorCode() != FittedTauLepton::None )
    {
      errorCode_ |= TauDecayParameters;
      return 0.;
    }
  }

  // compute visible energy fraction for second tau
  double x2_dash = 1.;
  if ( !leg2isPrompt_ )
  {
    int idx_x2 = legIntegrationParams_[1].idx_X_;
    if ( idx_x2 != -1 )
    {
      x2_dash = x_[idx_x2];
    }
    else
    {
      x2_dash = (mVis2_measured_/diTauMassConstraint2_)/x1_dash;
/*
      Vector p1 = fittedTauLepton1_.tauP4().Vect();
      //std::cout << "p1: Px = " << p1.x() << ", Py = " << p1.y() << ", Pz = " << p1.z() << std::endl;
      Vector ep2 = fittedTauLepton2_.visP4().Vect().unit();
      //std::cout << "ep2: Px = " << ep2.x() << ", Py = " << ep2.y() << ", Pz = " << ep2.z() << std::endl;
      double p1_times_ep2 = p1.Dot(ep2);
      //std::cout << "p1*ep2 = " << p1_times_ep2 << std::endl;
      double term1 = 4.*tauLeptonMass4 + square(diTauMassConstraint2_) - 4.*tauLeptonMass2*(diTauMassConstraint2_ - square(p1_times_ep2));
      //std::cout << "term1 = " << term1 << std::endl;
      double E1 = fittedTauLepton1_.tauP4().energy();
      double Evis2 = fittedTauLepton2_.visP4().energy();
      double pvis2 = fittedTauLepton2_.visP4().P();
      double term2 = square(p1_times_ep2)*(term1*(square(pvis2) + tauLeptonMass2) - 4.*square(E1*Evis2)*tauLeptonMass2);
      //std::cout << "term2 = " << term2 << std::endl;
      x2_dash = 2.*(E1*Evis2*(diTauMassConstraint2_ - 2.*tauLeptonMass2) + std::sqrt(term2))/term1;
      //std::cout << "x2_dash = " << x2_dash << std::endl;
 */
    }
  }
  double x2 = x2_dash/visPtShift2;
  if ( !(x2 >= 1.e-5 && x2 <= 1.) ) return 0.;

  // compute neutrino and tau lepton momentum for second tau
  if ( !leg2isPrompt_ )
  {
    int idx_phiNu2 = legIntegrationParams_[1].idx_phi_;
    assert(idx_phiNu2 != -1);
    double phiNu2 = x_[idx_phiNu2];
    int idx_nu2Mass = legIntegrationParams_[1].idx_mNuNu_;
    double nu2Mass = ( idx_nu2Mass != -1 ) ? std::sqrt(x_[idx_nu2Mass]) : 0.;
    fittedTauLepton2_.updateTauMomentum(x2, phiNu2, nu2Mass);
    //std::cout << "fittedTauLepton2: errorCode = " << fittedTauLepton2_.errorCode() << std::endl;
    if ( fittedTauLepton2_.errorCode() != FittedTauLepton::None )
    {
      errorCode_ |= TauDecayParameters;
      return 0.;
    }
  }

  if ( verbosity_ >= 2 )
  {
    for ( unsigned int iTau = 0; iTau < numTaus_; ++iTau )
    {
      const FittedTauLepton* fittedTauLepton = fittedTauLeptons_[iTau];
      const LorentzVector& visP4 = fittedTauLepton->visP4();
      const LorentzVector& nuP4 = fittedTauLepton->nuP4();
      const LorentzVector& tauP4 = fittedTauLepton->tauP4();
      std::cout << "leg" << (iTau + 1) << ": En = " << visP4.E() << ", Px = " << visP4.px()
		<< ", Py = " << visP4.py() << ", Pz = " << visP4.pz() << ";"
		<< " Pt = " << visP4.pt() << ", eta = " << visP4.eta()
		<< ", phi = " << visP4.phi() << ", mass = " << visP4.mass()
		<< " (x = " << fittedTauLepton->x() << ")" << std::endl;
      std::cout << "tau" << (iTau + 1) << ": En = " << tauP4.E() << ", Px = " << tauP4.px() << ", Py = " << tauP4.py() << ", Pz = " << tauP4.pz() << ";"
		<< " Pt = " << tauP4.pt() << ", eta = " << tauP4.eta() << ", phi = " << tauP4.phi() << std::endl;
      std::cout << "nu" << (iTau + 1) << ": En = " << nuP4.E() << ", Px = " << nuP4.px() << ", Py = " << nuP4.py() << ", Pz = " << nuP4.pz() << ";"
		<< " Pt = " << nuP4.pt() << ", eta = " << nuP4.eta() << ", phi = " << nuP4.phi() << ", mass = " << nuP4.mass() << std::endl;
    }
  }

  double prob_PS_and_tauDecay = classic_svFit::constFactor;
  double prob_tauDecay = 1.;
  double prob_TF = 1.;
  for ( unsigned int iTau = 0; iTau < numTaus_; ++iTau )
  {
    const FittedTauLepton* fittedTauLepton = fittedTauLeptons_[iTau];
    const MeasuredTauLepton& measuredTauLepton = fittedTauLepton->getMeasuredTauLepton();
    double x = fittedTauLepton->x();
    double nuMass = fittedTauLepton->nuMass();
    const LorentzVector& visP4 = fittedTauLepton->visP4();
    const LorentzVector& nuP4 = fittedTauLepton->nuP4();

    // evaluate tau decay matrix elements
    double prob = 1.;
    if      ( measuredTauLepton.isLeptonicTauDecay() ) prob = compPSfactor_tauToLepDecay(x, visP4.E(), visP4.P(), measuredTauLepton.mass(), nuP4.E(), nuP4.P(), nuMass);
    else if ( measuredTauLepton.isHadronicTauDecay() ) prob = compPSfactor_tauToHadDecay(x, visP4.E(), visP4.P(), measuredTauLepton.mass(), nuP4.E(), nuP4.P());
    prob_tauDecay *= prob;

    // evaluate transfer functions for tau energy reconstruction
#ifdef USE_SVFITTF
    if ( useHadTauTF_ && legIntegrationParams_[iTau].idx_VisPtShift_ != -1 && measuredTauLepton.isHadronicTauDecay() )
    {
      double prob = (*hadTauTFs_[iTau])(measuredTauLepton.pt(), visP4.pt(), visP4.eta());
      if ( verbosity_ >= 2 )
      {
	std::cout << "TF(leg" << iTau << "): recPt = " << measuredTauLepton.pt() << ", genPt = " << visP4.pt()
		  << ", genEta = " << visP4.eta() << " --> prob = " << prob << std::endl;
      }
      prob_TF *= prob;
    }
#endif
  }
  prob_PS_and_tauDecay *= prob_tauDecay;
  prob_PS_and_tauDecay *= classic_svFit::matrixElementNorm;

  double mTauTau = (fittedTauLepton1_.tauP4() + fittedTauLepton2_.tauP4()).mass();
  double prob_logM = 1.;
  if ( addLogM_ )
  {
    prob_logM = 1./pow(std::max(1., mTauTau), addLogM_power_);
  }

  double jacobiFactor = 1./(visPtShift1*visPtShift2); // product of derrivatives dx1/dx1' and dx2/dx2' for parametrization of x1, x2 by x1', x2'
  if ( diTauMassConstraint_ > 0. )
  {
    jacobiFactor *= (2.*x2/diTauMassConstraint_);
  }

  //static int numCalls = 0;
  //++numCalls;
  //std::cout << "call #" << numCalls << ":" << std::endl;
  //if ( numCalls > 100 ) assert(0);

  double prob = prob_PS_and_tauDecay*prob_TF*prob_logM*jacobiFactor;
  if ( verbosity_ >= 2 )
  {
    std::cout << "mTauTau = " << mTauTau << std::endl;
    std::cout << "prob: PS+decay = " << prob_PS_and_tauDecay << ","
              << " TF = " << prob_TF << ", log(M) = " << prob_logM << ", Jacobi = " << jacobiFactor 
	      << " --> returning " << prob << std::endl;
  }
  if ( TMath::IsNaN(prob) )
  {
    prob = 0.;
  }

  return prob;
}

double
ClassicSVfitIntegrand::EvalFlightLength() const
{
  if ( !useTauFlightLength_ ) return 1.;

  double prob = 1.;
  for ( unsigned iTau = 0; iTau < numTaus_; ++iTau )
  {
    const FittedTauLepton* fittedTauLepton = fittedTauLeptons_[iTau];
    const LorentzVector& tauP4 = fittedTauLepton->tauP4();
    Vector tauP3 = tauP4.Vect();
    const MeasuredTauLepton& measuredTauLepton = fittedTauLepton->getMeasuredTauLepton();
    if ( measuredTauLepton.isPrompt() ) continue;
    const Point& decayVertex = measuredTauLepton.decayVertex();
    const TMatrixD* decayVertexCov = nullptr;
    const TMatrixD* decayVertexCovInv = nullptr;
    const MeasuredHadTauDecayProduct* leadChargedHadron = nullptr;
    if ( iTau == 0 )
    {
      decayVertexCov = &leg1decayVertexCov_;
      decayVertexCovInv = &leg1decayVertexCovInv_;
      leadChargedHadron = leg1leadChargedHadron_;
    }
    else if ( iTau == 1 )
    {
      decayVertexCov = &leg2decayVertexCov_;
      decayVertexCovInv = &leg2decayVertexCovInv_;
      leadChargedHadron = leg2leadChargedHadron_;
    } else assert(0);

    Point pca = comp_PCA(tauP4, measuredTauLepton, *leadChargedHadron, primaryVertex_, decayVertex, *decayVertexCovInv);
    Vector flightLength = pca - primaryVertex_;

    std::pair<double,double> dmin_and_dmax = comp_dmin_and_dmax(tauP4, flightLength, *decayVertexCov);
    double dmin = dmin_and_dmax.first;
    double dmax = dmin_and_dmax.second;

    int idx_flightLength = legIntegrationParams_[iTau].idx_flightLength_;
    double x_flightLength = x_[idx_flightLength];
    double d = (1. - x_flightLength)*dmin + x_flightLength*dmax;
    double gamma = tauP4.energy()/tauLeptonMass;
    if ( gamma < 1. )
    {
      std::cerr << "WARNING: gamma = " << gamma << " is unphysical, setting it to 1 !!" << std::endl;
      gamma = 1.;
    }
    double gamma_times_cTauLifetime = gamma*classic_svFit::cTauLifetime;
    double prob_expDecay = (1./gamma_times_cTauLifetime)*exp(-d/gamma_times_cTauLifetime);

    double const_FlightLength = 0.;
    if      ( iTau == 0 ) const_FlightLength = const_FlightLength1_;
    else if ( iTau == 1 ) const_FlightLength = const_FlightLength2_;
    else assert(0);
    TVectorD eTau = convert_to_mathVector(normalize(tauP3));
    TVectorD residual = convert_to_mathVector(decayVertex - primaryVertex_) - d*eTau;
    double pull2 = residual*((*decayVertexCovInv)*residual);
    double prob_TF = const_FlightLength*exp(-0.5*pull2);

    double jacobiFactor = dmax - dmin;
    assert(jacobiFactor > 0.);
    double prob_i = prob_expDecay*prob_TF*jacobiFactor;
    //double prob_i = prob_TF*jacobiFactor;
    if ( verbosity_ >= 2 )
    {
      std::cout << "prob(flightLength #" << iTau << "):" 
                << " expDecay = " << prob_expDecay << ", TF = " << prob_TF << ", Jacobi = " << jacobiFactor << ","
                << " prob_i = " << prob_i << std::endl;
    }

    prob *= prob_i;
  }
  if ( verbosity_ >= 2 )
  {
    std::cout << " --> returning " << prob << std::endl;
  }
  return prob;
}

double
ClassicSVfitIntegrand::EvalMEtTF(const MeasuredMEt& measuredMEt) const
{
  if ( measuredMEt.type() == kProtonProtonCollisions )
  {
    // compute sum of momenta of all neutrinos produced in tau decays
    double sumNuPx = 0.;
    double sumNuPy = 0.;
    for ( unsigned int iTau = 0; iTau < numTaus_; ++iTau )
    {
      const FittedTauLepton* fittedTauLepton = fittedTauLeptons_[iTau];
      const LorentzVector& nuP4 = fittedTauLepton->nuP4();
      sumNuPx += nuP4.px();
      sumNuPy += nuP4.py();
    }

    // evaluate transfer function for MET/hadronic recoil
    double residualPx = measuredMEt.px() - sumNuPx;
    double residualPy = measuredMEt.py() - sumNuPy;
#ifdef USE_SVFITTF
    if ( rhoHadTau_ != 0. )
    {
      for ( unsigned int iTau = 0; iTau < numTaus_; ++iTau )
      {
        const FittedTauLepton* fittedTauLepton = fittedTauLeptons_[iTau];
        const MeasuredTauLepton& measuredTauLepton = fittedTauLepton->getMeasuredTauLepton();
        if ( measuredTauLepton.isHadronicTauDecay() )
        {
	  int idx_visPtShift = legIntegrationParams_[iTau].idx_VisPtShift_;
	  if ( idx_visPtShift != -1 )
          {
	    double visPtShift = 1./x_[idx_visPtShift];
	    if ( visPtShift < 1.e-2 ) continue;
	    residualPx += (rhoHadTau_*(visPtShift - 1.)*measuredTauLepton.px());
	    residualPy += (rhoHadTau_*(visPtShift - 1.)*measuredTauLepton.py());
	  }
        }
      }
    }
#endif
    const TMatrixD& covInv = measuredMEt.covInv();
    double pull2 = residualPx*(covInv(0,0)*residualPx + covInv(0,1)*residualPy) +
                   residualPy*(covInv(1,0)*residualPx + covInv(1,1)*residualPy);
    double prob = measuredMEt.const_MET()*exp(-0.5*pull2);

    if ( verbosity_ >= 2 )
    {    
      std::cout << "TF(met):" 
                << " recPx = " << measuredMEt.px() << ", recPy = " << measuredMEt.py() << ","
	        << " genPx = " << sumNuPx << ", genPy = " << sumNuPy << ","
	        << " pull2 = " << pull2 << ", prob = " << prob << std::endl;
    }
    return prob;
  }
  else if ( measuredMEt.type() == kElectronPositronCollisions )
  {
    // compute sum of momenta of all neutrinos produced in tau decays
    double sumNuPx = 0.;
    double sumNuPy = 0.;
    double sumNuPz = 0.;
    double sumNuE  = 0.;
    for ( unsigned int iTau = 0; iTau < numTaus_; ++iTau )
    {
      const FittedTauLepton* fittedTauLepton = fittedTauLeptons_[iTau];
      const LorentzVector& nuP4 = fittedTauLepton->nuP4();
      sumNuPx += nuP4.px();
      sumNuPy += nuP4.py();
      sumNuPz += nuP4.pz();
      sumNuE  += nuP4.energy();      
    }

    // evaluate transfer function for MET/hadronic recoil    
    double residualPx = measuredMEt.px()     - sumNuPx;
    double residualPy = measuredMEt.py()     - sumNuPy;
    double residualPz = measuredMEt.pz()     - sumNuPz;
    double residualE  = measuredMEt.energy() - sumNuE;    
#ifdef USE_SVFITTF
    if ( rhoHadTau_ != 0. )
    {
      for ( unsigned int iTau = 0; iTau < numTaus_; ++iTau )
      {
        const FittedTauLepton* fittedTauLepton = fittedTauLeptons_[iTau];
        const MeasuredTauLepton& measuredTauLepton = fittedTauLepton->getMeasuredTauLepton();
        if ( measuredTauLepton.isHadronicTauDecay() )
        {
	  int idx_visPtShift = legIntegrationParams_[iTau].idx_VisPtShift_;
	  if ( idx_visPtShift != -1 )
          {
	    double visPtShift = 1./x_[idx_visPtShift];
	    if ( visPtShift < 1.e-2 ) continue;
	    residualPx += (rhoHadTau_*(visPtShift - 1.)*measuredTauLepton.px());
	    residualPy += (rhoHadTau_*(visPtShift - 1.)*measuredTauLepton.py());
	    residualPz += (rhoHadTau_*(visPtShift - 1.)*measuredTauLepton.pz());
	    residualE  += (rhoHadTau_*(visPtShift - 1.)*measuredTauLepton.energy());
	  }
        }
      }
    }
#endif
    TVectorD residual(4);
    residual(0) = residualPx;
    residual(1) = residualPy;
    residual(2) = residualPz;
    residual(3) = residualE;
    const TMatrixD& covInv = measuredMEt.covInv();
    double pull2 = residual*(covInv*residual);
    double prob = measuredMEt.const_MET()*exp(-0.5*pull2);

    if ( verbosity_ >= 2 )
    {    
      std::cout << "TF(met):" 
                << " recPx = " << measuredMEt.px() << ", recPy = " << measuredMEt.py() << ","
                << " recPz = " << measuredMEt.pz() << ", recE = " << measuredMEt.energy() << ","
	        << " genPx = " << sumNuPx << ", genPy = " << sumNuPy << ","
                << " genPz = " << sumNuPz << ", genE = " << sumNuE << ","
	        << " pull2 = " << pull2 << ", prob = " << prob << std::endl;
    }
    return prob;
  } else assert(0);
}

}
