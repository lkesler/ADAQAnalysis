/////////////////////////////////////////////////////////////////////////////////
//                                                                             //
//                           Copyright (C) 2012-2015                           //
//                 Zachary Seth Hartwig : All rights reserved                  //
//                                                                             //
//      The ADAQAnalysis source code is licensed under the GNU GPL v3.0.       //
//      You have the right to modify and/or redistribute this source code      //      
//      under the terms specified in the license, which may be found online    //
//      at http://www.gnu.org/licenses or at $ADAQANALYSIS/License.txt.        //
//                                                                             //
/////////////////////////////////////////////////////////////////////////////////

#ifndef __AASettings_hh__
#define __AASettings_hh__ 1

#include <TObject.h>
#include <TGraph.h>
#include <TCutG.h>

#include <vector>
#include <string>
using namespace std;


class AASettings : public TObject
{
public:

  /////////////////////
  // Waveform frame  //
  /////////////////////
  
  Int_t WaveformChannel, WaveformToPlot;
  Bool_t RawWaveform, BSWaveform, ZSWaveform;
  Int_t WaveformPolarity;
  
  Bool_t PlotZeroSuppressionCeiling;
  Int_t ZeroSuppressionCeiling;
  Int_t ZeroSuppressionBuffer;

  Bool_t FindPeaks, UseMarkovSmoothing;
  Int_t MaxPeaks, Sigma, Floor;
  Double_t Resolution;
  
  Bool_t PlotFloor, PlotCrossings, PlotPeakIntegrationRegion;
  
  Bool_t UsePileupRejection;
  
  Bool_t PlotBaselineCalcRegion;
  Int_t BaselineCalcMin, BaselineCalcMax;
  
  Bool_t PlotTrigger, WaveformAnalysis;

  
  ////////////////////
  // Spectrum frame //
  ////////////////////

  Int_t WaveformsToHistogram;
  Int_t SpectrumNumBins;
  Double_t SpectrumMinBin, SpectrumMaxBin;
  
  Bool_t ADAQSpectrumTypePAS, ADAQSpectrumTypePHS;
  Bool_t ADAQSpectrumAlgorithmWW, ADAQSpectrumAlgorithmPF, ADAQSpectrumAlgorithmWD;
  
  Bool_t ASIMSpectrumTypeEnergy;
  Bool_t ASIMSpectrumTypePhotonsCreated;
  Bool_t ASIMSpectrumTypePhotonsDetected;
  string ASIMEventTreeName;

  Int_t EnergyUnit;
  vector<TGraph *> SpectraCalibrations;
  vector<bool> UseSpectraCalibrations;
  
  
  ////////////////////
  // Analysis frame //
  ////////////////////

  Bool_t FindBackground;
  Int_t BackgroundIterations;
  Bool_t BackgroundCompton, BackgroundSmoothing;
  Double_t BackgroundMinBin, BackgroundMaxBin;
  Int_t BackgroundDirection;
  Int_t BackgroundFilterOrder;
  Int_t BackgroundSmoothingWidth;

  Bool_t PlotWithBackground, PlotLessBackground;

  Bool_t SpectrumFindIntegral, SpectrumIntegralInCounts;
  Bool_t SpectrumUseGaussianFit, SpectrumNormalizeToCurrent;
  

  ////////////////////
  // Graphics frame //
  ////////////////////
  
  Bool_t WaveformLine, WaveformCurve, WaveformMarkers, WaveformBoth;
  Int_t WaveformLineWidth;
  Double_t WaveformMarkerSize;
  Bool_t SpectrumLine, SpectrumCurve, SpectrumError, SpectrumBars;
  Int_t SpectrumLineWidth, SpectrumFillStyle;
  Bool_t HistogramStats, CanvasGrid;
  Bool_t CanvasXAxisLog, CanvasYAxisLog, CanvasZAxisLog;
  Bool_t PlotSpectrumDerivativeError, PlotAbsValueSpectrumDerivative, PlotYAxisWithAutoRange;
  Bool_t OverrideGraphicalDefault;

  string PlotTitle, XAxisTitle, YAxisTitle, ZAxisTitle, PaletteTitle;
  Double_t XSize, YSize, ZSize, PaletteSize;
  Double_t XOffset, YOffset, ZOffset, PaletteOffset;
  Int_t XDivs, YDivs, ZDivs;
  Double_t PaletteX1, PaletteX2, PaletteY1, PaletteY2;


  //////////////////////
  // Processing frame //
  //////////////////////

  Bool_t SeqProcessing, ParProcessing;
  Int_t NumProcessors, UpdateFreq;

  Bool_t PSDEnable;
  Int_t PSDChannel, PSDWaveformsToDiscriminate;
  Bool_t PSDAlgorithmPF, PSDAlgorithmWW, PSDAlgorithmWD;
  Int_t PSDTotalStart, PSDTotalStop, PSDTailStart, PSDTailStop;
  Int_t PSDThreshold, PSDTailOffset, PSDPeakOffset;
  Int_t PSDNumTotalBins;
  Double_t PSDMinTotalBin, PSDMaxTotalBin;
  Int_t PSDNumTailBins;
  Double_t PSDMinTailBin, PSDMaxTailBin;
  Bool_t PSDYAxisTail, PSDYAxisTailTotal;

  string PSDPlotType;
  Bool_t PSDPlotTailIntegrationRegion, EnableHistogramSlicing, PSDXSlice, PSDYSlice;

  Bool_t PSDEnableRegionCreation, PSDEnableFilterUse;
  Bool_t PSDInsideRegion, PSDOutsideRegion;

  vector<TCutG *> PSDRegions;
  vector<bool> UsePSDRegions;

  Double_t RFQPulseWidth, RFQRepRate;
  Int_t RFQCountRateWaveforms;

  Bool_t IntegratePearson;
  Int_t PearsonChannel, PearsonPolarity;
  Bool_t IntegrateRawPearson, IntegrateFitToPearson;
  Int_t PearsonLowerLimit, PearsonMiddleLimit, PearsonUpperLimit;
  Bool_t PlotPearsonIntegration;

  Int_t TotalDeuterons;
  
  Int_t WaveformsToDesplice, DesplicedWaveformBuffer, DesplicedWaveformLength;
  string DesplicedFileName;


  // Canvas

  Double_t XAxisMin, XAxisMax, XAxisPtr;
  Double_t YAxisMin, YAxisMax;

  Int_t WaveformSelector;
  
  Double_t SpectrumIntegrationMin, SpectrumIntegrationMax;
  
  // General
  
  string ADAQFileName;
  string ASIMFileName;
  
  ClassDef(AASettings, 1);
};

#endif
