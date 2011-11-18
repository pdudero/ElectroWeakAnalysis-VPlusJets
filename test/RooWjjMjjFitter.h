// -*- mode: C++ -*-

#ifndef RooWjjMjjFitter_h
#define RooWjjMjjFitter_h

#include "RooWjjFitterParams.h"
#include "RooWjjFitterUtils.h"
#include "RooAbsData.h"

#include "RooWorkspace.h"

class RooFitResult;

class RooWjjMjjFitter {
public:
  RooWjjMjjFitter();
  RooWjjMjjFitter(RooWjjFitterParams & pars);
  virtual ~RooWjjMjjFitter() { }

  RooFitResult * fit();
  RooPlot * computeChi2(double& chi2, int& ndf);

  RooAbsPdf * makeFitter();
  RooAbsData * loadData(bool trunc = false);

  RooAbsPdf * makeDibosonPdf();
  RooAbsPdf * makeWpJPdf();
  RooAbsPdf * makettbarPdf();
  RooAbsPdf * makeSingleTopPdf();
  RooAbsPdf * makeQCDPdf();
  RooAbsPdf * makeZpJPdf();
  RooAbsPdf * makeNPPdf();

  RooPlot * stackedPlot(bool logy = false);
  RooPlot * residualPlot(RooPlot * thePlot, TString curveName,
			 TString pdfName = "", bool normalize = false);

  void loadParameters(TString fname);

  RooWorkspace& getWorkSpace() { return ws_; }

  void resetYields();

  ////   Use For MC Dataset Toy Generation
  void generateToyMCSet(RooAbsPdf *inputPdf, const char* outFileName, int NEvts, int seedInitializer);
  void resetfSUfMU(double fSU, double fMU);


private:
  RooWorkspace ws_;
  RooWjjFitterParams params_;
  RooWjjFitterUtils utils_;

  double initWjets_;
  double initDiboson_;
  double ttbarNorm_;
  double singleTopNorm_;
  double zjetsNorm_;
  double QCDNorm_;
  RooAbsData::ErrorType errorType_;

  TString rangeString_;
};

#endif
