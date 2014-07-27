//////////////////////////////////////////////////////////////////////////////////////////
//
// name: AAInterface.cc
// date: 16 Jul 14
// auth: Zach Hartwig
//
// desc:
//
//////////////////////////////////////////////////////////////////////////////////////////

// ROOT
#include <TApplication.h>
#include <TGClient.h>
#include <TGFileDialog.h>
#include <TGButtonGroup.h>
#include <TFile.h>

// C++
#include <iostream>
#include <sstream>
#include <string>
using namespace std;

// Boost
#include <boost/thread.hpp>

// MPI
#ifdef MPI_ENABLED
#include <mpi.h>
#endif

// ADAQ
#include "AAInterface.hh"
#include "AAWaveformSlots.hh"
#include "AASpectrumSlots.hh"
#include "AAAnalysisSlots.hh"
#include "AAGraphicsSlots.hh"
#include "AAProcessingSlots.hh"
#include "AAConstants.hh"
#include "AAVersion.hh"


AAInterface::AAInterface(string CmdLineArg)
  : TGMainFrame(gClient->GetRoot()),
    NumDataChannels(8), NumProcessors(boost::thread::hardware_concurrency()),
    CanvasX(700), CanvasY(500), CanvasFrameWidth(700), 
    SliderBuffer(45), LeftFrameLength(720), TotalX(1125), TotalY(800),
    NumEdgeBoundingPoints(0), EdgeBoundX0(0.), EdgeBoundY0(0.),
    DataDirectory(getenv("PWD")), PrintDirectory(getenv("HOME")),
    DesplicedDirectory(getenv("HOME")), HistogramDirectory(getenv("HOME")),
    ADAQFileLoaded(false), ACRONYMFileLoaded(false),
    ColorMgr(new TColor), RndmMgr(new TRandom3)
{
  SetCleanup(kDeepCleanup);

  // Allow env. variable to control small version of GUI
  if(getenv("ADAQANALYSIS_SMALL")!=NULL){
    CanvasX = 500;
    CanvasY = 300;
    CanvasFrameWidth = 400;
    LeftFrameLength = 540;
    TotalX = 950;
    TotalY = 610;
  }

  ///////////////

  WaveformSlots = new AAWaveformSlots(this);
  SpectrumSlots = new AASpectrumSlots(this);
  AnalysisSlots = new AAAnalysisSlots(this);
  GraphicsSlots = new AAGraphicsSlots(this);
  ProcessingSlots = new AAProcessingSlots(this);

  //////////////
  
  ThemeForegroundColor = ColorMgr->Number2Pixel(18);
  ThemeBackgroundColor = ColorMgr->Number2Pixel(22);
  
  CreateTheMainFrames();
 
  ComputationMgr = AAComputation::GetInstance();
  ComputationMgr->SetProgressBarPointer(ProcessingProgress_PB);
  
  GraphicsMgr = AAGraphics::GetInstance();
  GraphicsMgr->SetCanvasPointer(Canvas_EC->GetCanvas());

  InterpolationMgr = AAInterpolation::GetInstance();
  
  if(CmdLineArg != "Unspecified"){
    
    size_t Pos = CmdLineArg.find_last_of(".");
    if(Pos != string::npos){
      
      if(CmdLineArg.substr(Pos,5) == ".root" or
	 CmdLineArg.substr(Pos,5) == ".adaq"){
	
	// Ensure that if an ADAQ or ACRO file is specified from the
	// cmd line with a relative path that the absolute path to the
	// file is specified to ensure the universal access
	
	string CurrentDir = getenv("PWD");
	string RelativeFilePath = CmdLineArg;
	
	ADAQFileName = CurrentDir + "/" + RelativeFilePath;
	ADAQFileLoaded = ComputationMgr->LoadADAQRootFile(ADAQFileName);
	
	if(ADAQFileLoaded)
	  UpdateForADAQFile();
	else
	  CreateMessageBox("The ADAQ ROOT file that you specified fail to load for some reason!\n","Stop");
	
	ACRONYMFileLoaded = false;
      }
      else if(CmdLineArg.substr(Pos,5) == ".acro"){
	ACRONYMFileName = CmdLineArg;
	ACRONYMFileLoaded = ComputationMgr->LoadACRONYMRootFile(ACRONYMFileName);
	
	if(ACRONYMFileLoaded)
	  UpdateForACRONYMFile();
	else
	  CreateMessageBox("The ACRONYM ROOT file that you specified fail to load for some reason!\n","Stop");
      }
      else
	CreateMessageBox("Recognized files must end in '.root or '.adaq' (ADAQ) or '.acro' (ACRONYM)","Stop");
    }
    else
      CreateMessageBox("Could not find an acceptable file to open. Please try again.","Stop");
  }
  
  string USER = getenv("USER");
  ADAQSettingsFileName = "/tmp/ADAQSettings_" + USER + ".root";
}


AAInterface::~AAInterface()
{
  delete RndmMgr;
  delete ColorMgr;
}


void AAInterface::CreateTheMainFrames()
{
  /////////////////////////
  // Create the menu bar //
  /////////////////////////

  TGHorizontalFrame *MenuFrame = new TGHorizontalFrame(this); 
  MenuFrame->SetBackgroundColor(ThemeBackgroundColor);

  TGPopupMenu *SaveSpectrumSubMenu = new TGPopupMenu(gClient->GetRoot());
  SaveSpectrumSubMenu->AddEntry("&raw", MenuFileSaveSpectrum_ID);
  SaveSpectrumSubMenu->AddEntry("&background", MenuFileSaveSpectrumBackground_ID);
  SaveSpectrumSubMenu->AddEntry("&derivative", MenuFileSaveSpectrumDerivative_ID);

  TGPopupMenu *SaveCalibrationSubMenu = new TGPopupMenu(gClient->GetRoot());
  SaveCalibrationSubMenu->AddEntry("&values to file", MenuFileSaveSpectrumCalibration_ID);
  
  TGPopupMenu *SavePSDSubMenu = new TGPopupMenu(gClient->GetRoot());
  SavePSDSubMenu->AddEntry("histo&gram", MenuFileSavePSDHistogram_ID);
  SavePSDSubMenu->AddEntry("sli&ce", MenuFileSavePSDHistogramSlice_ID);
  
  TGPopupMenu *MenuFile = new TGPopupMenu(gClient->GetRoot());
  MenuFile->AddEntry("&Open ADAQ ROOT file ...", MenuFileOpenADAQ_ID);
  MenuFile->AddEntry("Ope&n ACRONYM ROOT file ...", MenuFileOpenACRONYM_ID);
  MenuFile->AddSeparator();
  MenuFile->AddEntry("Save &waveform ...", MenuFileSaveWaveform_ID);
  MenuFile->AddPopup("Save &spectrum ...", SaveSpectrumSubMenu);
  MenuFile->AddPopup("Save &calibration ...", SaveCalibrationSubMenu);
  MenuFile->AddPopup("Save &PSD ...",SavePSDSubMenu);
  MenuFile->AddSeparator();
  MenuFile->AddEntry("&Print canvas ...", MenuFilePrint_ID);
  MenuFile->AddSeparator();
  MenuFile->AddEntry("E&xit", MenuFileExit_ID);
  MenuFile->Connect("Activated(int)", "AAInterface", this, "HandleMenu(int)");

  TGMenuBar *MenuBar = new TGMenuBar(MenuFrame, 100, 20, kHorizontalFrame);
  MenuBar->SetBackgroundColor(ThemeBackgroundColor);
  MenuBar->AddPopup("&File", MenuFile, new TGLayoutHints(kLHintsTop | kLHintsLeft, 0,0,0,0));
  MenuFrame->AddFrame(MenuBar, new TGLayoutHints(kLHintsTop | kLHintsLeft, 0,0,0,0));
  AddFrame(MenuFrame, new TGLayoutHints(kLHintsTop | kLHintsExpandX));


  //////////////////////////////////////////////////////////
  // Create the main frame to hold the options and canvas //
  //////////////////////////////////////////////////////////

  AddFrame(OptionsAndCanvas_HF = new TGHorizontalFrame(this),
	   new TGLayoutHints(kLHintsTop, 5,5,5,5));
  
  OptionsAndCanvas_HF->SetBackgroundColor(ThemeBackgroundColor);

  
  //////////////////////////////////////////////////////////////
  // Create the left-side vertical options frame with tab holder 

  TGVerticalFrame *Options_VF = new TGVerticalFrame(OptionsAndCanvas_HF);
  Options_VF->SetBackgroundColor(ThemeBackgroundColor);
  OptionsAndCanvas_HF->AddFrame(Options_VF);
  
  OptionsTabs_T = new TGTab(Options_VF);
  OptionsTabs_T->SetBackgroundColor(ThemeBackgroundColor);
  Options_VF->AddFrame(OptionsTabs_T, new TGLayoutHints(kLHintsLeft, 15,15,5,5));


  //////////////////////////////////////
  // Create the individual tabs + frames

  // The tabbed frame to hold waveform widgets
  WaveformOptionsTab_CF = OptionsTabs_T->AddTab("Waveform");
  WaveformOptionsTab_CF->AddFrame(WaveformOptions_CF = new TGCompositeFrame(WaveformOptionsTab_CF, 1, 1, kVerticalFrame));
  WaveformOptions_CF->Resize(320,LeftFrameLength);
  WaveformOptions_CF->ChangeOptions(WaveformOptions_CF->GetOptions() | kFixedSize);

  // The tabbed frame to hold spectrum widgets
  SpectrumOptionsTab_CF = OptionsTabs_T->AddTab("Spectrum");
  SpectrumOptionsTab_CF->AddFrame(SpectrumOptions_CF = new TGCompositeFrame(SpectrumOptionsTab_CF, 1, 1, kVerticalFrame));
  SpectrumOptions_CF->Resize(320,LeftFrameLength);
  SpectrumOptions_CF->ChangeOptions(SpectrumOptions_CF->GetOptions() | kFixedSize);

  // The tabbed frame to hold analysis widgets
  AnalysisOptionsTab_CF = OptionsTabs_T->AddTab("Analysis");
  AnalysisOptionsTab_CF->AddFrame(AnalysisOptions_CF = new TGCompositeFrame(AnalysisOptionsTab_CF, 1, 1, kVerticalFrame));
  AnalysisOptions_CF->Resize(320,LeftFrameLength);
  AnalysisOptions_CF->ChangeOptions(AnalysisOptions_CF->GetOptions() | kFixedSize);

  // The tabbed frame to hold graphical widgets
  GraphicsOptionsTab_CF = OptionsTabs_T->AddTab("Graphics");
  GraphicsOptionsTab_CF->AddFrame(GraphicsOptions_CF = new TGCompositeFrame(GraphicsOptionsTab_CF, 1, 1, kVerticalFrame));
  GraphicsOptions_CF->Resize(320,LeftFrameLength);
  GraphicsOptions_CF->ChangeOptions(GraphicsOptions_CF->GetOptions() | kFixedSize);

  // The tabbed frame to hold parallel processing widgets
  ProcessingOptionsTab_CF = OptionsTabs_T->AddTab("Processing");
  ProcessingOptionsTab_CF->AddFrame(ProcessingOptions_CF = new TGCompositeFrame(ProcessingOptionsTab_CF, 1, 1, kVerticalFrame));
  ProcessingOptions_CF->Resize(320,LeftFrameLength);
  ProcessingOptions_CF->ChangeOptions(ProcessingOptions_CF->GetOptions() | kFixedSize);

  FillWaveformFrame();
  FillSpectrumFrame();
  FillAnalysisFrame();
  FillGraphicsFrame();
  FillProcessingFrame();
  FillCanvasFrame();


  //////////////////////////////////////
  // Finalize options and map windows //
  //////////////////////////////////////
  
  SetBackgroundColor(ThemeBackgroundColor);
  
  // Create a string that will be located in the title bar of the
  // ADAQAnalysisGUI to identify the version number of the code
  string TitleString;
  if(VersionString == "Development")
    TitleString = "ADAQ Analysis (Development version)               Fear is the mind-killer.";
  else
    TitleString = "ADAQ Analysis (Production version " + VersionString + ")               Fear is the mind-killer.";

  SetWindowName(TitleString.c_str());
  MapSubwindows();
  Resize(TotalX, TotalY);
  MapWindow();

  WaveformOptionsTab_CF->HideFrame(WaveformOptions_CF);
  WaveformOptionsTab_CF->ShowFrame(WaveformOptions_CF);
}


void AAInterface::FillWaveformFrame()
{
  /////////////////////////////////////////
  // Fill the waveform options tabbed frame
  
  TGCanvas *WaveformFrame_C = new TGCanvas(WaveformOptions_CF, 320, LeftFrameLength-20, kDoubleBorder);
  WaveformOptions_CF->AddFrame(WaveformFrame_C, new TGLayoutHints(kLHintsCenterX, 0,0,10,10));

  TGVerticalFrame *WaveformFrame_VF = new TGVerticalFrame(WaveformFrame_C->GetViewPort(), 320, LeftFrameLength);
  WaveformFrame_C->SetContainer(WaveformFrame_VF);
  
  TGHorizontalFrame *WaveformSelection_HF = new TGHorizontalFrame(WaveformFrame_VF);
  WaveformFrame_VF->AddFrame(WaveformSelection_HF, new TGLayoutHints(kLHintsLeft, 15,5,15,5));
  
  WaveformSelection_HF->AddFrame(ChannelSelector_CBL = new ADAQComboBoxWithLabel(WaveformSelection_HF, "", ChannelSelector_CBL_ID),
				 new TGLayoutHints(kLHintsLeft, 0,5,5,5));
  stringstream ss;
  string entry;
  for(int ch=0; ch<NumDataChannels; ch++){
    ss << ch;
    entry.assign("Channel " + ss.str());
    ChannelSelector_CBL->GetComboBox()->AddEntry(entry.c_str(),ch);
    ss.str("");
  }
  ChannelSelector_CBL->GetComboBox()->Select(0);

  WaveformSelection_HF->AddFrame(WaveformSelector_NEL = new ADAQNumberEntryWithLabel(WaveformSelection_HF, "Waveform", WaveformSelector_NEL_ID),
				 new TGLayoutHints(kLHintsLeft, 5,5,5,5));
  WaveformSelector_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  WaveformSelector_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEANonNegative);
  WaveformSelector_NEL->GetEntry()->Connect("ValueSet(long)", "AAWaveformSlots", WaveformSlots, "HandleNumberEntries()");
  
  // Waveform specification (type and polarity)

  TGHorizontalFrame *WaveformSpecification_HF = new TGHorizontalFrame(WaveformFrame_VF);
  WaveformFrame_VF->AddFrame(WaveformSpecification_HF, new TGLayoutHints(kLHintsLeft, 15,5,5,5));
  
  WaveformSpecification_HF->AddFrame(WaveformType_BG = new TGButtonGroup(WaveformSpecification_HF, "Type", kVerticalFrame),
				     new TGLayoutHints(kLHintsLeft, 0,5,0,5));
  RawWaveform_RB = new TGRadioButton(WaveformType_BG, "Raw voltage", RawWaveform_RB_ID);
  RawWaveform_RB->Connect("Clicked()", "AAWaveformSlots", WaveformSlots, "HandleRadioButtons()");
  RawWaveform_RB->SetState(kButtonDown);
  
  BaselineSubtractedWaveform_RB = new TGRadioButton(WaveformType_BG, "Baseline-subtracted", BaselineSubtractedWaveform_RB_ID);
  BaselineSubtractedWaveform_RB->Connect("Clicked()", "AAWaveformSlots", WaveformSlots, "HandleRadioButtons()");

  ZeroSuppressionWaveform_RB = new TGRadioButton(WaveformType_BG, "Zero suppression", ZeroSuppressionWaveform_RB_ID);
  ZeroSuppressionWaveform_RB->Connect("Clicked()", "AAWaveformSlots", WaveformSlots, "HandleRadioButtons()");

  WaveformSpecification_HF->AddFrame(WaveformPolarity_BG = new TGButtonGroup(WaveformSpecification_HF, "Polarity", kVerticalFrame),
				     new TGLayoutHints(kLHintsLeft, 5,5,0,5));
  
  PositiveWaveform_RB = new TGRadioButton(WaveformPolarity_BG, "Positive", PositiveWaveform_RB_ID);
  PositiveWaveform_RB->Connect("Clicked()", "AAWaveformSlots", WaveformSlots, "HandleRadioButtons()");
  
  NegativeWaveform_RB = new TGRadioButton(WaveformPolarity_BG, "Negative", NegativeWaveform_RB_ID);
  NegativeWaveform_RB->Connect("Clicked()", "AAWaveformSlots", WaveformSlots, "HandleRadioButtons()");
  NegativeWaveform_RB->SetState(kButtonDown);

TGGroupFrame *PeakFindingOptions_GF = new TGGroupFrame(WaveformFrame_VF, "Peak finding options", kVerticalFrame);
  WaveformFrame_VF->AddFrame(PeakFindingOptions_GF, new TGLayoutHints(kLHintsCenterX, 5,5,5,5));
  
  TGHorizontalFrame *PeakFinding_HF0 = new TGHorizontalFrame(PeakFindingOptions_GF);
  PeakFindingOptions_GF->AddFrame(PeakFinding_HF0, new TGLayoutHints(kLHintsLeft, 0,0,0,5));
  
  PeakFinding_HF0->AddFrame(FindPeaks_CB = new TGCheckButton(PeakFinding_HF0, "Find peaks", FindPeaks_CB_ID),
			    new TGLayoutHints(kLHintsLeft, 5,5,5,0));
  FindPeaks_CB->SetState(kButtonDisabled);
  FindPeaks_CB->Connect("Clicked()", "AAWaveformSlots", WaveformSlots, "HandleCheckButtons()");
  
  PeakFinding_HF0->AddFrame(UseMarkovSmoothing_CB = new TGCheckButton(PeakFinding_HF0, "Use Markov smoothing", UseMarkovSmoothing_CB_ID),
			    new TGLayoutHints(kLHintsLeft, 15,5,5,0));
  UseMarkovSmoothing_CB->SetState(kButtonDown);
  UseMarkovSmoothing_CB->SetState(kButtonDisabled);
  UseMarkovSmoothing_CB->Connect("Clicked()", "AAWaveformSlots", WaveformSlots, "HandleCheckButtons()");


  TGHorizontalFrame *PeakFinding_HF1 = new TGHorizontalFrame(PeakFindingOptions_GF);
  PeakFindingOptions_GF->AddFrame(PeakFinding_HF1, new TGLayoutHints(kLHintsLeft, 0,0,0,5));

  PeakFinding_HF1->AddFrame(MaxPeaks_NEL = new ADAQNumberEntryWithLabel(PeakFinding_HF1, "Max. peaks", MaxPeaks_NEL_ID),
			    new TGLayoutHints(kLHintsLeft, 5,5,0,0));
  MaxPeaks_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  MaxPeaks_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  MaxPeaks_NEL->GetEntry()->SetNumber(1);
  MaxPeaks_NEL->GetEntry()->Resize(65, 20);
  MaxPeaks_NEL->GetEntry()->SetState(false);
  MaxPeaks_NEL->GetEntry()->Connect("ValueSet(long)", "AAWaveformSlots", WaveformSlots, "HandleNumberEntries()");
  
  PeakFinding_HF1->AddFrame(Sigma_NEL = new ADAQNumberEntryWithLabel(PeakFinding_HF1, "Sigma", Sigma_NEL_ID),
			    new TGLayoutHints(kLHintsLeft, 10,5,0,0));
  Sigma_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  Sigma_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  Sigma_NEL->GetEntry()->SetNumber(5);
  Sigma_NEL->GetEntry()->Resize(65, 20);
  Sigma_NEL->GetEntry()->SetState(false);
  Sigma_NEL->GetEntry()->Connect("ValueSet(long)", "AAWaveformSlots", WaveformSlots, "HandleNumberEntries()");


  TGHorizontalFrame *PeakFinding_HF2 = new TGHorizontalFrame(PeakFindingOptions_GF);
  PeakFindingOptions_GF->AddFrame(PeakFinding_HF2, new TGLayoutHints(kLHintsLeft, 0,0,0,5));

  PeakFinding_HF2->AddFrame(Resolution_NEL = new ADAQNumberEntryWithLabel(PeakFinding_HF2, "Resolution", Resolution_NEL_ID),
			    new TGLayoutHints(kLHintsLeft, 5,5,0,0));
  Resolution_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  Resolution_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  Resolution_NEL->GetEntry()->SetNumber(0.005);
  Resolution_NEL->GetEntry()->Resize(65, 20);
  Resolution_NEL->GetEntry()->SetState(false);
  Resolution_NEL->GetEntry()->Connect("ValueSet(long)", "AAWaveformSlots", WaveformSlots, "HandleNumberEntries()");
  
  PeakFinding_HF2->AddFrame(Floor_NEL = new ADAQNumberEntryWithLabel(PeakFinding_HF2, "Floor", Floor_NEL_ID),
			    new TGLayoutHints(kLHintsLeft, 10,5,0,0));
  Floor_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  Floor_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  Floor_NEL->GetEntry()->SetNumber(50);
  Floor_NEL->GetEntry()->Resize(65, 20);
  Floor_NEL->GetEntry()->SetState(false);
  Floor_NEL->GetEntry()->Connect("ValueSet(long)", "AAWaveformSlots", WaveformSlots, "HandleNumberEntries()");

  
  TGVerticalFrame *PeakFinding_VF0 = new TGVerticalFrame(PeakFindingOptions_GF);
  PeakFindingOptions_GF->AddFrame(PeakFinding_VF0, new TGLayoutHints(kLHintsLeft, 5,5,0,0));

  PeakFinding_VF0->AddFrame(PlotFloor_CB = new TGCheckButton(PeakFinding_VF0, "Plot the floor", PlotFloor_CB_ID),
			    new TGLayoutHints(kLHintsLeft, 0,0,5,0));
  PlotFloor_CB->SetState(kButtonDisabled);
  PlotFloor_CB->Connect("Clicked()", "AAWaveformSlots", WaveformSlots, "HandleCheckButtons()");
  
  PeakFinding_VF0->AddFrame(PlotCrossings_CB = new TGCheckButton(PeakFinding_VF0, "Plot waveform intersections", PlotCrossings_CB_ID),
			    new TGLayoutHints(kLHintsLeft, 0,0,5,0));
  PlotCrossings_CB->SetState(kButtonDisabled);
  PlotCrossings_CB->Connect("Clicked()", "AAWaveformSlots", WaveformSlots, "HandleCheckButtons()");
  
  PeakFinding_VF0->AddFrame(PlotPeakIntegratingRegion_CB = new TGCheckButton(PeakFinding_VF0, "Plot integration region", PlotPeakIntegratingRegion_CB_ID),
			    new TGLayoutHints(kLHintsLeft, 0,0,5,0));
  PlotPeakIntegratingRegion_CB->SetState(kButtonDisabled);
  PlotPeakIntegratingRegion_CB->Connect("Clicked()", "AAWaveformSlots", WaveformSlots, "HandleCheckButtons()");
  
  
  ////////////////////////
  // Waveform baseline  //
  ////////////////////////

  WaveformFrame_VF->AddFrame(PlotBaseline_CB = new TGCheckButton(WaveformFrame_VF, "Plot baseline calculation region.", PlotBaseline_CB_ID),
			     new TGLayoutHints(kLHintsLeft, 15,5,5,0));
  PlotBaseline_CB->Connect("Clicked()", "AAWaveformSlots", WaveformSlots, "HandleCheckButtons()");
  
  TGHorizontalFrame *BaselineRegion_HF = new TGHorizontalFrame(WaveformFrame_VF);
  WaveformFrame_VF->AddFrame(BaselineRegion_HF, new TGLayoutHints(kLHintsLeft, 15,5,5,5));
  
  BaselineRegion_HF->AddFrame(BaselineCalcMin_NEL = new ADAQNumberEntryWithLabel(BaselineRegion_HF, "Min.", BaselineCalcMin_NEL_ID),
			      new TGLayoutHints(kLHintsLeft, 0,5,0,0));
  BaselineCalcMin_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  BaselineCalcMin_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEANonNegative);
  BaselineCalcMin_NEL->GetEntry()->SetNumLimits(TGNumberFormat::kNELLimitMinMax);
  BaselineCalcMin_NEL->GetEntry()->SetLimitValues(0,1); // Set when ADAQRootFile loaded
  BaselineCalcMin_NEL->GetEntry()->SetNumber(0); // Set when ADAQRootFile loaded
  BaselineCalcMin_NEL->GetEntry()->Resize(55, 20);
  BaselineCalcMin_NEL->GetEntry()->Connect("ValueSet(long)", "AAWaveformSlots", WaveformSlots, "HandleNumberEntries()");
  
  BaselineRegion_HF->AddFrame(BaselineCalcMax_NEL = new ADAQNumberEntryWithLabel(BaselineRegion_HF, "Max.", BaselineCalcMax_NEL_ID),
			      new TGLayoutHints(kLHintsLeft, 0,5,0,5)); 
  BaselineCalcMax_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  BaselineCalcMax_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEANonNegative);
  BaselineCalcMax_NEL->GetEntry()->SetNumLimits(TGNumberFormat::kNELLimitMinMax);
  BaselineCalcMax_NEL->GetEntry()->SetLimitValues(1,2); // Set When ADAQRootFile loaded
  BaselineCalcMax_NEL->GetEntry()->SetNumber(1); // Set when ADAQRootFile loaded
  BaselineCalcMax_NEL->GetEntry()->Resize(55, 20);
  BaselineCalcMax_NEL->GetEntry()->Connect("ValueSet(long)", "AAWaveformSlots", WaveformSlots, "HandleNumberEntries()");


  //////////////////////////////
  // Zero suppression options //
  //////////////////////////////

  WaveformFrame_VF->AddFrame(PlotZeroSuppressionCeiling_CB = new TGCheckButton(WaveformFrame_VF, "Plot zero suppression ceiling", PlotZeroSuppressionCeiling_CB_ID),
			       new TGLayoutHints(kLHintsLeft, 15,5,5,0));
  PlotZeroSuppressionCeiling_CB->Connect("Clicked()", "AAWaveformSlots", WaveformSlots, "HandleCheckButtons()");
  
  WaveformFrame_VF->AddFrame(ZeroSuppressionCeiling_NEL = new ADAQNumberEntryWithLabel(WaveformFrame_VF, "Zero suppression ceiling", ZeroSuppressionCeiling_NEL_ID),
			       new TGLayoutHints(kLHintsLeft, 15,5,5,5));
  ZeroSuppressionCeiling_NEL->GetEntry()->SetNumber(15);
  ZeroSuppressionCeiling_NEL->GetEntry()->Connect("ValueSet(long)", "AAWaveformSlots", WaveformSlots, "HandleNumberEntries()");


  /////////////////////
  // Trigger options //
  /////////////////////

  WaveformFrame_VF->AddFrame(PlotTrigger_CB = new TGCheckButton(WaveformFrame_VF, "Plot trigger", PlotTrigger_CB_ID),
		       new TGLayoutHints(kLHintsLeft, 15,5,10,0));
  PlotTrigger_CB->Connect("Clicked()", "AAWaveformSlots", WaveformSlots, "HandleCheckButtons()");
  
  WaveformFrame_VF->AddFrame(TriggerLevel_NEFL = new ADAQNumberEntryFieldWithLabel(WaveformFrame_VF, "Trigger level (ADC)", -1),
			     new TGLayoutHints(kLHintsNormal, 15,5,5,5));
  TriggerLevel_NEFL->GetEntry()->SetFormat(TGNumberFormat::kNESInteger, TGNumberFormat::kNEAAnyNumber);
  TriggerLevel_NEFL->GetEntry()->Resize(70, 20);
  TriggerLevel_NEFL->GetEntry()->SetState(false);


  ////////////////////
  // Pileup options //
  ////////////////////
  
  WaveformFrame_VF->AddFrame(UsePileupRejection_CB = new TGCheckButton(WaveformFrame_VF, "Use pileup rejection", UsePileupRejection_CB_ID),
			       new TGLayoutHints(kLHintsNormal, 15,5,5,5));
  UsePileupRejection_CB->Connect("Clicked()", "AAWaveformSlots", WaveformSlots, "HandleCheckButtons()");
  UsePileupRejection_CB->SetState(kButtonDown);
			       

  ///////////////////////
  // Waveform analysis //
  ///////////////////////
  
  TGGroupFrame *WaveformAnalysis_GF = new TGGroupFrame(WaveformFrame_VF, "Waveform analysis", kVerticalFrame);
  WaveformFrame_VF->AddFrame(WaveformAnalysis_GF, new TGLayoutHints(kLHintsLeft, 15,5,5,5));
  
  WaveformAnalysis_GF->AddFrame(WaveformAnalysis_CB = new TGCheckButton(WaveformAnalysis_GF, "Analyze waveform", WaveformAnalysis_CB_ID),
				new TGLayoutHints(kLHintsLeft, 0,5,5,5));
  WaveformAnalysis_CB->SetState(kButtonDisabled);
  WaveformAnalysis_CB->Connect("Clicked()", "AAWaveformSlots", WaveformSlots, "HandleCheckButtons()");
  
  WaveformAnalysis_GF->AddFrame(WaveformIntegral_NEL = new ADAQNumberEntryWithLabel(WaveformAnalysis_GF, "Integral (ADC)", -1),
				new TGLayoutHints(kLHintsLeft, 0,5,0,0));
  WaveformIntegral_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  WaveformIntegral_NEL->GetEntry()->SetNumber(0.);
  WaveformIntegral_NEL->GetEntry()->SetState(false);
  WaveformIntegral_NEL->GetEntry()->Resize(125,20);
  
  WaveformAnalysis_GF->AddFrame(WaveformHeight_NEL = new ADAQNumberEntryWithLabel(WaveformAnalysis_GF, "Height (ADC)", -1),
				new TGLayoutHints(kLHintsLeft, 0,5,0,0));
  WaveformHeight_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  WaveformHeight_NEL->GetEntry()->SetNumber(0.);
  WaveformHeight_NEL->GetEntry()->SetState(false);
  WaveformHeight_NEL->GetEntry()->Resize(125,20);
}


void AAInterface::FillSpectrumFrame()
{
  /////////////////////////////////////////
  // Fill the spectrum options tabbed frame
  
  TGCanvas *SpectrumFrame_C = new TGCanvas(SpectrumOptions_CF, 320, LeftFrameLength-20, kDoubleBorder);
  SpectrumOptions_CF->AddFrame(SpectrumFrame_C, new TGLayoutHints(kLHintsCenterX, 0,0,10,10));
  
  TGVerticalFrame *SpectrumFrame_VF = new TGVerticalFrame(SpectrumFrame_C->GetViewPort(), 320, LeftFrameLength);
  SpectrumFrame_C->SetContainer(SpectrumFrame_VF);
  
  TGHorizontalFrame *WaveformAndBins_HF = new TGHorizontalFrame(SpectrumFrame_VF);
  SpectrumFrame_VF->AddFrame(WaveformAndBins_HF, new TGLayoutHints(kLHintsNormal, 0,0,0,0));


  // Number of waveforms to bin in the histogram
  WaveformAndBins_HF->AddFrame(WaveformsToHistogram_NEL = new ADAQNumberEntryWithLabel(WaveformAndBins_HF, "Waveforms", WaveformsToHistogram_NEL_ID),
			       new TGLayoutHints(kLHintsLeft, 15,0,8,5));
  WaveformsToHistogram_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  WaveformsToHistogram_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  WaveformsToHistogram_NEL->GetEntry()->SetNumLimits(TGNumberFormat::kNELLimitMinMax);
  WaveformsToHistogram_NEL->GetEntry()->SetLimitValues(1,2);
  WaveformsToHistogram_NEL->GetEntry()->SetNumber(0);
  
  // Number of spectrum bins number entry
  WaveformAndBins_HF->AddFrame(SpectrumNumBins_NEL = new ADAQNumberEntryWithLabel(WaveformAndBins_HF, "# Bins", SpectrumNumBins_NEL_ID),
			       new TGLayoutHints(kLHintsLeft, 15,15,8,5));
  SpectrumNumBins_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  SpectrumNumBins_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEANonNegative);
  SpectrumNumBins_NEL->GetEntry()->SetNumber(200);
  
  TGHorizontalFrame *SpectrumBinning_HF = new TGHorizontalFrame(SpectrumFrame_VF);
  SpectrumFrame_VF->AddFrame(SpectrumBinning_HF, new TGLayoutHints(kLHintsNormal, 0,0,0,0));

  // Minimum spectrum bin number entry
  SpectrumBinning_HF->AddFrame(SpectrumMinBin_NEL = new ADAQNumberEntryWithLabel(SpectrumBinning_HF, "Minimum  ", SpectrumMinBin_NEL_ID),
		       new TGLayoutHints(kLHintsLeft, 15,0,0,0));
  SpectrumMinBin_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  SpectrumMinBin_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEANonNegative);
  SpectrumMinBin_NEL->GetEntry()->SetNumber(0);

  // Maximum spectrum bin number entry
  SpectrumBinning_HF->AddFrame(SpectrumMaxBin_NEL = new ADAQNumberEntryWithLabel(SpectrumBinning_HF, "Maximum", SpectrumMaxBin_NEL_ID),
		       new TGLayoutHints(kLHintsRight, 15,0,0,0));
  SpectrumMaxBin_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  SpectrumMaxBin_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  SpectrumMaxBin_NEL->GetEntry()->SetNumber(22000);


  ///////////////////////
  // ADAQ spectra options

  TGHorizontalFrame *ADAQSpectrumOptions_HF = new TGHorizontalFrame(SpectrumFrame_VF);
  SpectrumFrame_VF->AddFrame(ADAQSpectrumOptions_HF, new TGLayoutHints(kLHintsNormal, 15,0,0,0));

  // Spectrum type radio buttons
  ADAQSpectrumOptions_HF->AddFrame(ADAQSpectrumType_BG = new TGButtonGroup(ADAQSpectrumOptions_HF, "ADAQ spectra", kVerticalFrame),
				   new TGLayoutHints(kLHintsCenterX, 5,5,15,0));
  ADAQSpectrumTypePAS_RB = new TGRadioButton(ADAQSpectrumType_BG, "Pulse area", -1);
  ADAQSpectrumTypePAS_RB->SetState(kButtonDown);
  
  ADAQSpectrumTypePHS_RB = new TGRadioButton(ADAQSpectrumType_BG, "Pulse height", -1);

  // Integration type radio buttons
  ADAQSpectrumOptions_HF->AddFrame(ADAQSpectrumIntType_BG = new TGButtonGroup(ADAQSpectrumOptions_HF, "ADAQ integration", kVerticalFrame),
				   new TGLayoutHints(kLHintsCenterX, 5,5,15,0));
  ADAQSpectrumIntTypeWW_RB = new TGRadioButton(ADAQSpectrumIntType_BG, "Whole waveform", -1);
  ADAQSpectrumIntTypeWW_RB->SetState(kButtonDown);
  
  ADAQSpectrumIntTypePF_RB = new TGRadioButton(ADAQSpectrumIntType_BG, "Peak finder", -1);


  //////////////////////////
  // ACRONYM spectra options

  TGHorizontalFrame *ACROSpectrumOptions_HF = new TGHorizontalFrame(SpectrumFrame_VF);
  SpectrumFrame_VF->AddFrame(ACROSpectrumOptions_HF, new TGLayoutHints(kLHintsNormal, 15,0,0,0));
  
  // Spectrum type radio buttons
  ACROSpectrumOptions_HF->AddFrame(ACROSpectrumType_BG = new TGButtonGroup(ACROSpectrumOptions_HF, "ACRO spectra", kVerticalFrame),
				   new TGLayoutHints(kLHintsCenterX, 5,5,15,0));
  ACROSpectrumTypeEnergy_RB = new TGRadioButton(ACROSpectrumType_BG, "Energy deposited", -1);
  ACROSpectrumTypeEnergy_RB->SetState(kButtonDown);
  ACROSpectrumTypeEnergy_RB->SetState(kButtonDisabled);
  
  ACROSpectrumTypeScintCreated_RB = new TGRadioButton(ACROSpectrumType_BG, "Photons created", -1);
  ACROSpectrumTypeScintCreated_RB->SetState(kButtonDisabled);

  ACROSpectrumTypeScintCounted_RB = new TGRadioButton(ACROSpectrumType_BG, "Photons counted", -1);
  ACROSpectrumTypeScintCounted_RB->SetState(kButtonDisabled);

  // Detector type radio buttons
  ACROSpectrumOptions_HF->AddFrame(ACROSpectrumDetector_BG = new TGButtonGroup(ACROSpectrumOptions_HF, "ACRO detector", kVerticalFrame),
				   new TGLayoutHints(kLHintsCenterX, 5,5,15,0));
  ACROSpectrumLS_RB = new TGRadioButton(ACROSpectrumDetector_BG, "LaBr3", ACROSpectrumLS_RB_ID);
  ACROSpectrumLS_RB->Connect("Clicked()", "AASpectrumSlots", SpectrumSlots, "HandleRadioButtons()");
  ACROSpectrumLS_RB->SetState(kButtonDown);
  ACROSpectrumLS_RB->SetState(kButtonDisabled);
  
  ACROSpectrumES_RB = new TGRadioButton(ACROSpectrumDetector_BG, "EJ301", ACROSpectrumES_RB_ID);
  ACROSpectrumES_RB->Connect("Clicked()", "AASpectrumSlots", SpectrumSlots, "HandleRadioButtons()");
  ACROSpectrumES_RB->SetState(kButtonDisabled);
  
  
  //////////////////////////////
  // Spectrum energy calibration

  TGGroupFrame *SpectrumCalibration_GF = new TGGroupFrame(SpectrumFrame_VF, "Energy calibration", kVerticalFrame);
  SpectrumFrame_VF->AddFrame(SpectrumCalibration_GF, new TGLayoutHints(kLHintsCenterX,5,5,10,0));

  TGHorizontalFrame *SpectrumCalibration_HF0 = new TGHorizontalFrame(SpectrumCalibration_GF);
  SpectrumCalibration_GF->AddFrame(SpectrumCalibration_HF0, new TGLayoutHints(kLHintsNormal, 0,0,0,0));

  // Energy calibration 
  SpectrumCalibration_HF0->AddFrame(SpectrumCalibration_CB = new TGCheckButton(SpectrumCalibration_HF0, "Make it so", SpectrumCalibration_CB_ID),
				    new TGLayoutHints(kLHintsLeft, 0,0,10,0));
  SpectrumCalibration_CB->Connect("Clicked()", "AASpectrumSlots", SpectrumSlots, "HandleCheckButtons()");
  SpectrumCalibration_CB->SetState(kButtonUp);

  // Load from file text button
  SpectrumCalibration_HF0->AddFrame(SpectrumCalibrationLoad_TB = new TGTextButton(SpectrumCalibration_HF0, "Load from file", SpectrumCalibrationLoad_TB_ID),
				    new TGLayoutHints(kLHintsRight, 15,0,5,5));
  SpectrumCalibrationLoad_TB->Connect("Clicked()", "AASpectrumSlots", SpectrumSlots, "HandleTextButtons()");
  SpectrumCalibrationLoad_TB->Resize(120,25);
  SpectrumCalibrationLoad_TB->ChangeOptions(SpectrumCalibrationLoad_TB->GetOptions() | kFixedSize);
  SpectrumCalibrationLoad_TB->SetState(kButtonDisabled);

  TGHorizontalFrame *SpectrumCalibration_HF5 = new TGHorizontalFrame(SpectrumCalibration_GF);
  SpectrumCalibration_GF->AddFrame(SpectrumCalibration_HF5, new TGLayoutHints(kLHintsNormal, 0,0,0,0));

  SpectrumCalibration_HF5->AddFrame(SpectrumCalibrationStandard_RB = new TGRadioButton(SpectrumCalibration_HF5, "Standard", SpectrumCalibrationStandard_RB_ID),
				    new TGLayoutHints(kLHintsNormal, 10,5,5,5));
  SpectrumCalibrationStandard_RB->SetState(kButtonDown);
  SpectrumCalibrationStandard_RB->SetState(kButtonDisabled);
  SpectrumCalibrationStandard_RB->Connect("Clicked()", "AASpectrumSlots", SpectrumSlots, "HandleRadioButtons()");
  
  SpectrumCalibration_HF5->AddFrame(SpectrumCalibrationEdgeFinder_RB = new TGRadioButton(SpectrumCalibration_HF5, "Edge finder", SpectrumCalibrationEdgeFinder_RB_ID),
				       new TGLayoutHints(kLHintsNormal, 30,5,5,5));
  SpectrumCalibrationEdgeFinder_RB->SetState(kButtonDisabled);
  SpectrumCalibrationEdgeFinder_RB->Connect("Clicked()", "AASpectrumSlots", SpectrumSlots, "HandleRadioButtons()");


  TGHorizontalFrame *SpectrumCalibrationType_HF = new TGHorizontalFrame(SpectrumCalibration_GF);
  SpectrumCalibration_GF->AddFrame(SpectrumCalibrationType_HF, new TGLayoutHints(kLHintsNormal, 0,0,0,0));
  
  SpectrumCalibrationType_HF->AddFrame(SpectrumCalibrationPoint_CBL = new ADAQComboBoxWithLabel(SpectrumCalibrationType_HF, "", SpectrumCalibrationPoint_CBL_ID),
				       new TGLayoutHints(kLHintsNormal, 0,0,10,3));
  SpectrumCalibrationPoint_CBL->GetComboBox()->Resize(150,20);
  SpectrumCalibrationPoint_CBL->GetComboBox()->AddEntry("Calibration point 0",0);
  SpectrumCalibrationPoint_CBL->GetComboBox()->Select(0);
  SpectrumCalibrationPoint_CBL->GetComboBox()->SetEnabled(false);
  SpectrumCalibrationPoint_CBL->GetComboBox()->Connect("Selected(int,int)", "AASpectrumSlots", SpectrumSlots, "HandleComboBoxes(int,int)");

  TGVerticalFrame *SpectrumCalibrationType_VF = new TGVerticalFrame(SpectrumCalibrationType_HF);
  SpectrumCalibrationType_HF->AddFrame(SpectrumCalibrationType_VF);

  SpectrumCalibration_GF->AddFrame(SpectrumCalibrationUnit_CBL = new ADAQComboBoxWithLabel(SpectrumCalibration_GF, "Energy unit", -1),
				   new TGLayoutHints(kLHintsLeft, 0,0,0,0));
  SpectrumCalibrationUnit_CBL->GetComboBox()->Resize(72, 20);
  SpectrumCalibrationUnit_CBL->GetComboBox()->AddEntry("keV", 0);
  SpectrumCalibrationUnit_CBL->GetComboBox()->AddEntry("MeV", 1);
  SpectrumCalibrationUnit_CBL->GetComboBox()->Select(1);
  SpectrumCalibrationUnit_CBL->GetComboBox()->SetEnabled(false);

  SpectrumCalibration_GF->AddFrame(SpectrumCalibrationEnergy_NEL = new ADAQNumberEntryWithLabel(SpectrumCalibration_GF, "Energy (keV or MeV)", SpectrumCalibrationEnergy_NEL_ID),
					  new TGLayoutHints(kLHintsLeft,0,0,0,0));
  SpectrumCalibrationEnergy_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  SpectrumCalibrationEnergy_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEANonNegative);
  SpectrumCalibrationEnergy_NEL->GetEntry()->SetNumber(0.0);
  SpectrumCalibrationEnergy_NEL->GetEntry()->SetState(false);
  SpectrumCalibrationEnergy_NEL->GetEntry()->Connect("ValueSet(long)", "AASpectrumSlots", SpectrumSlots, "HandleNumberEntries()");

  SpectrumCalibration_GF->AddFrame(SpectrumCalibrationPulseUnit_NEL = new ADAQNumberEntryWithLabel(SpectrumCalibration_GF, "Pulse unit (ADC)", SpectrumCalibrationPulseUnit_NEL_ID),
				       new TGLayoutHints(kLHintsLeft,0,0,0,5));
  SpectrumCalibrationPulseUnit_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  SpectrumCalibrationPulseUnit_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  SpectrumCalibrationPulseUnit_NEL->GetEntry()->SetNumber(1.0);
  SpectrumCalibrationPulseUnit_NEL->GetEntry()->SetState(false);
  SpectrumCalibrationPulseUnit_NEL->GetEntry()->Connect("ValueSet(long)", "AASpectrumSlots", SpectrumSlots, "HandleNumberEntries()");

  TGHorizontalFrame *SpectrumCalibration_HF1 = new TGHorizontalFrame(SpectrumCalibration_GF);
  SpectrumCalibration_GF->AddFrame(SpectrumCalibration_HF1);
  
  // Set point text button
  SpectrumCalibration_HF1->AddFrame(SpectrumCalibrationSetPoint_TB = new TGTextButton(SpectrumCalibration_HF1, "Set Pt.", SpectrumCalibrationSetPoint_TB_ID),
				    new TGLayoutHints(kLHintsNormal, 5,5,5,0));
  SpectrumCalibrationSetPoint_TB->Connect("Clicked()", "AASpectrumSlots", SpectrumSlots, "HandleTextButtons()");
  SpectrumCalibrationSetPoint_TB->Resize(100,25);
  SpectrumCalibrationSetPoint_TB->ChangeOptions(SpectrumCalibrationSetPoint_TB->GetOptions() | kFixedSize);
  SpectrumCalibrationSetPoint_TB->SetState(kButtonDisabled);

  // Calibrate text button
  SpectrumCalibration_HF1->AddFrame(SpectrumCalibrationCalibrate_TB = new TGTextButton(SpectrumCalibration_HF1, "Calibrate", SpectrumCalibrationCalibrate_TB_ID),
				    new TGLayoutHints(kLHintsNormal, 0,0,5,0));
  SpectrumCalibrationCalibrate_TB->Connect("Clicked()", "AASpectrumSlots", SpectrumSlots, "HandleTextButtons()");
  SpectrumCalibrationCalibrate_TB->Resize(100,25);
  SpectrumCalibrationCalibrate_TB->ChangeOptions(SpectrumCalibrationCalibrate_TB->GetOptions() | kFixedSize);
  SpectrumCalibrationCalibrate_TB->SetState(kButtonDisabled);

  TGHorizontalFrame *SpectrumCalibration_HF2 = new TGHorizontalFrame(SpectrumCalibration_GF);
  SpectrumCalibration_GF->AddFrame(SpectrumCalibration_HF2);
  
  // Plot text button
  SpectrumCalibration_HF2->AddFrame(SpectrumCalibrationPlot_TB = new TGTextButton(SpectrumCalibration_HF2, "Plot", SpectrumCalibrationPlot_TB_ID),
				    new TGLayoutHints(kLHintsNormal, 5,5,5,5));
  SpectrumCalibrationPlot_TB->Connect("Clicked()", "AASpectrumSlots", SpectrumSlots, "HandleTextButtons()");
  SpectrumCalibrationPlot_TB->Resize(100,25);
  SpectrumCalibrationPlot_TB->ChangeOptions(SpectrumCalibrationPlot_TB->GetOptions() | kFixedSize);
  SpectrumCalibrationPlot_TB->SetState(kButtonDisabled);

  // Reset text button
  SpectrumCalibration_HF2->AddFrame(SpectrumCalibrationReset_TB = new TGTextButton(SpectrumCalibration_HF2, "Reset", SpectrumCalibrationReset_TB_ID),
					  new TGLayoutHints(kLHintsNormal, 0,0,5,5));
  SpectrumCalibrationReset_TB->Connect("Clicked()", "AASpectrumSlots", SpectrumSlots, "HandleTextButtons()");
  SpectrumCalibrationReset_TB->Resize(100,25);
  SpectrumCalibrationReset_TB->ChangeOptions(SpectrumCalibrationReset_TB->GetOptions() | kFixedSize);
  SpectrumCalibrationReset_TB->SetState(kButtonDisabled);

  TGHorizontalFrame *SpectrumCalibration_HF3 = new TGHorizontalFrame(SpectrumCalibration_GF);
  SpectrumCalibration_GF->AddFrame(SpectrumCalibration_HF3);

  /////////////////////////
  // Create spectrum button

  SpectrumFrame_VF->AddFrame(CreateSpectrum_TB = new TGTextButton(SpectrumFrame_VF, "Create spectrum", CreateSpectrum_TB_ID),
			     new TGLayoutHints(kLHintsCenterX | kLHintsTop, 5,5,15,0));
  CreateSpectrum_TB->Resize(200, 30);
  CreateSpectrum_TB->SetBackgroundColor(ColorMgr->Number2Pixel(36));
  CreateSpectrum_TB->SetForegroundColor(ColorMgr->Number2Pixel(0));
  CreateSpectrum_TB->ChangeOptions(CreateSpectrum_TB->GetOptions() | kFixedSize);
  CreateSpectrum_TB->Connect("Clicked()", "AASpectrumSlots", SpectrumSlots, "HandleTextButtons()");

}


void AAInterface::FillAnalysisFrame()
{
  TGCanvas *AnalysisFrame_C = new TGCanvas(AnalysisOptions_CF, 320, LeftFrameLength-20, kDoubleBorder);
  AnalysisOptions_CF->AddFrame(AnalysisFrame_C, new TGLayoutHints(kLHintsCenterX, 0,0,10,10));

  TGVerticalFrame *AnalysisFrame_VF = new TGVerticalFrame(AnalysisFrame_C->GetViewPort(), 320, LeftFrameLength);
  AnalysisFrame_C->SetContainer(AnalysisFrame_VF);

  
  //////////////////////////////
  // Spectrum background fitting 
  
  TGGroupFrame *BackgroundAnalysis_GF = new TGGroupFrame(AnalysisFrame_VF, "Background fitting", kVerticalFrame);
  AnalysisFrame_VF->AddFrame(BackgroundAnalysis_GF, new TGLayoutHints(kLHintsCenterX, 5,5,5,5));
  
  TGHorizontalFrame *SpectrumBackgroundOptions0_HF = new TGHorizontalFrame(BackgroundAnalysis_GF);
  BackgroundAnalysis_GF->AddFrame(SpectrumBackgroundOptions0_HF);
  
  SpectrumBackgroundOptions0_HF->AddFrame(SpectrumFindBackground_CB = new TGCheckButton(SpectrumBackgroundOptions0_HF, "Find background", SpectrumFindBackground_CB_ID),
					  new TGLayoutHints(kLHintsNormal, 5,5,6,0));
  SpectrumFindBackground_CB->Connect("Clicked()", "AAInterface", this, "HandleCheckButtons()");

  SpectrumBackgroundOptions0_HF->AddFrame(SpectrumBackgroundIterations_NEL = new ADAQNumberEntryWithLabel(SpectrumBackgroundOptions0_HF, "Iterations", SpectrumBackgroundIterations_NEL_ID),
					  new TGLayoutHints(kLHintsNormal,10,0,5,0));
  SpectrumBackgroundIterations_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  SpectrumBackgroundIterations_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  SpectrumBackgroundIterations_NEL->GetEntry()->SetNumber(5);
  SpectrumBackgroundIterations_NEL->GetEntry()->Resize(40,20);
  SpectrumBackgroundIterations_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");
  SpectrumBackgroundIterations_NEL->GetEntry()->SetState(false);
  
  ////

  TGHorizontalFrame *SpectrumBackgroundOptions1_HF = new TGHorizontalFrame(BackgroundAnalysis_GF);
  BackgroundAnalysis_GF->AddFrame(SpectrumBackgroundOptions1_HF);
  
  SpectrumBackgroundOptions1_HF->AddFrame(SpectrumBackgroundCompton_CB = new TGCheckButton(SpectrumBackgroundOptions1_HF, "Compton", SpectrumBackgroundCompton_CB_ID),
					  new TGLayoutHints(kLHintsNormal,5,5,0,5));
  SpectrumBackgroundCompton_CB->Connect("Clicked()", "AAInterface", this, "HandleCheckButtons()");
  SpectrumBackgroundCompton_CB->SetState(kButtonDisabled);
  

  SpectrumBackgroundOptions1_HF->AddFrame(SpectrumBackgroundSmoothing_CB = new TGCheckButton(SpectrumBackgroundOptions1_HF, "Smoothing", SpectrumBackgroundSmoothing_CB_ID),
					  new TGLayoutHints(kLHintsNormal,70,5,0,5));
  SpectrumBackgroundSmoothing_CB->Connect("Clicked()", "AAInterface", this, "HandleCheckButtons()");
  SpectrumBackgroundSmoothing_CB->SetState(kButtonDisabled);

  ////

  TGHorizontalFrame *SpectrumBackgroundOptions2_HF = new TGHorizontalFrame(BackgroundAnalysis_GF);
  BackgroundAnalysis_GF->AddFrame(SpectrumBackgroundOptions2_HF);
  
  SpectrumBackgroundOptions2_HF->AddFrame(SpectrumRangeMin_NEL = new ADAQNumberEntryWithLabel(SpectrumBackgroundOptions2_HF, "Min.", SpectrumRangeMin_NEL_ID),
					  new TGLayoutHints(kLHintsNormal, 5,5,0,0));
  SpectrumRangeMin_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  SpectrumRangeMin_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  SpectrumRangeMin_NEL->GetEntry()->SetNumber(0);
  SpectrumRangeMin_NEL->GetEntry()->Resize(75,20);
  SpectrumRangeMin_NEL->GetEntry()->SetState(false);
  SpectrumRangeMin_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");

  
  SpectrumBackgroundOptions2_HF->AddFrame(SpectrumRangeMax_NEL = new ADAQNumberEntryWithLabel(SpectrumBackgroundOptions2_HF, "Max.", SpectrumRangeMax_NEL_ID),
					  new TGLayoutHints(kLHintsNormal, 5,5,0,0));
  SpectrumRangeMax_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  SpectrumRangeMax_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  SpectrumRangeMax_NEL->GetEntry()->SetNumber(2000);
  SpectrumRangeMax_NEL->GetEntry()->Resize(75,20);
  SpectrumRangeMax_NEL->GetEntry()->SetState(false);
  SpectrumRangeMax_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");

  ////

  BackgroundAnalysis_GF->AddFrame(SpectrumBackgroundDirection_CBL = new ADAQComboBoxWithLabel(BackgroundAnalysis_GF, "Filter direction", SpectrumBackgroundDirection_CBL_ID),
				new TGLayoutHints(kLHintsNormal,5,5,0,0));
  SpectrumBackgroundDirection_CBL->GetComboBox()->AddEntry("Increase",0);
  SpectrumBackgroundDirection_CBL->GetComboBox()->AddEntry("Decrease",1);
  SpectrumBackgroundDirection_CBL->GetComboBox()->Resize(75,20);
  SpectrumBackgroundDirection_CBL->GetComboBox()->Select(1);
  SpectrumBackgroundDirection_CBL->GetComboBox()->Connect("Selected(int,int)", "AAInterface", this, "HandleComboBox(int,int)");
  SpectrumBackgroundDirection_CBL->GetComboBox()->SetEnabled(false);
  
  BackgroundAnalysis_GF->AddFrame(SpectrumBackgroundFilterOrder_CBL = new ADAQComboBoxWithLabel(BackgroundAnalysis_GF, "Filter order", SpectrumBackgroundFilterOrder_CBL_ID),
				new TGLayoutHints(kLHintsNormal,5,5,0,0));
  SpectrumBackgroundFilterOrder_CBL->GetComboBox()->Resize(75,20);
  SpectrumBackgroundFilterOrder_CBL->GetComboBox()->AddEntry("2",2);
  SpectrumBackgroundFilterOrder_CBL->GetComboBox()->AddEntry("4",4);
  SpectrumBackgroundFilterOrder_CBL->GetComboBox()->AddEntry("6",6);
  SpectrumBackgroundFilterOrder_CBL->GetComboBox()->AddEntry("8",8);
  SpectrumBackgroundFilterOrder_CBL->GetComboBox()->Select(2);
  SpectrumBackgroundFilterOrder_CBL->GetComboBox()->Connect("Selected(int,int)", "AAInterface", this, "HandleComboBox(int,int)");
  SpectrumBackgroundFilterOrder_CBL->GetComboBox()->SetEnabled(false);
  
  BackgroundAnalysis_GF->AddFrame(SpectrumBackgroundSmoothingWidth_CBL = new ADAQComboBoxWithLabel(BackgroundAnalysis_GF, "Smoothing width", SpectrumBackgroundSmoothingWidth_CBL_ID),
				new TGLayoutHints(kLHintsNormal,5,5,0,0));
  SpectrumBackgroundSmoothingWidth_CBL->GetComboBox()->Resize(75,20);
  SpectrumBackgroundSmoothingWidth_CBL->GetComboBox()->AddEntry("3",3);
  SpectrumBackgroundSmoothingWidth_CBL->GetComboBox()->AddEntry("5",5);
  SpectrumBackgroundSmoothingWidth_CBL->GetComboBox()->AddEntry("7",7);
  SpectrumBackgroundSmoothingWidth_CBL->GetComboBox()->AddEntry("9",9);
  SpectrumBackgroundSmoothingWidth_CBL->GetComboBox()->AddEntry("11",11);
  SpectrumBackgroundSmoothingWidth_CBL->GetComboBox()->AddEntry("13",13);
  SpectrumBackgroundSmoothingWidth_CBL->GetComboBox()->AddEntry("15",15);
  SpectrumBackgroundSmoothingWidth_CBL->GetComboBox()->Select(3);
  SpectrumBackgroundSmoothingWidth_CBL->GetComboBox()->Connect("Selected(int,int)", "AAInterface", this, "HandleComboBox(int,int)");
  SpectrumBackgroundSmoothingWidth_CBL->GetComboBox()->SetEnabled(false);
  
  ////

  TGHorizontalFrame *BackgroundPlotting_HF = new TGHorizontalFrame(BackgroundAnalysis_GF);
  BackgroundAnalysis_GF->AddFrame(BackgroundPlotting_HF, new TGLayoutHints(kLHintsNormal, 0,0,10,0));

  BackgroundPlotting_HF->AddFrame(SpectrumWithBackground_RB = new TGRadioButton(BackgroundPlotting_HF, "Plot with bckgnd", SpectrumWithBackground_RB_ID),
				  new TGLayoutHints(kLHintsNormal, 5,5,0,0));
  SpectrumWithBackground_RB->SetState(kButtonDown);
  SpectrumWithBackground_RB->SetState(kButtonDisabled);
  SpectrumWithBackground_RB->Connect("Clicked()", "AAInterface", this, "HandleRadioButtons()");
  
  BackgroundPlotting_HF->AddFrame(SpectrumLessBackground_RB = new TGRadioButton(BackgroundPlotting_HF, "Plot less bckgnd", SpectrumLessBackground_RB_ID),
				  new TGLayoutHints(kLHintsNormal, 5,5,0,5));
  SpectrumLessBackground_RB->SetState(kButtonDisabled);
  SpectrumLessBackground_RB->Connect("Clicked()", "AAInterface", this, "HandleRadioButtons()");

  ////////////////////////////////////////
  // Spectrum integration and peak fitting
  
  TGGroupFrame *SpectrumAnalysis_GF = new TGGroupFrame(AnalysisFrame_VF, "Integration and peak fitting", kVerticalFrame);
  AnalysisFrame_VF->AddFrame(SpectrumAnalysis_GF, new TGLayoutHints(kLHintsCenterX, 15,5,5,5));
  
  TGHorizontalFrame *Horizontal0_HF = new TGHorizontalFrame(SpectrumAnalysis_GF);
  SpectrumAnalysis_GF->AddFrame(Horizontal0_HF, new TGLayoutHints(kLHintsNormal, 0,0,5,5));
  
  Horizontal0_HF->AddFrame(SpectrumFindIntegral_CB = new TGCheckButton(Horizontal0_HF, "Find integral", SpectrumFindIntegral_CB_ID),
			   new TGLayoutHints(kLHintsNormal, 5,5,0,0));
  SpectrumFindIntegral_CB->Connect("Clicked()", "AAInterface", this, "HandleCheckButtons()");
  
  Horizontal0_HF->AddFrame(SpectrumIntegralInCounts_CB = new TGCheckButton(Horizontal0_HF, "Integral in counts", SpectrumIntegralInCounts_CB_ID),
			   new TGLayoutHints(kLHintsNormal, 5,5,0,0));
  SpectrumIntegralInCounts_CB->Connect("Clicked()", "AAInterface", this, "HandleCheckButtons()");
  SpectrumIntegralInCounts_CB->SetState(kButtonDown);
  
  SpectrumAnalysis_GF->AddFrame(SpectrumIntegral_NEFL = new ADAQNumberEntryFieldWithLabel(SpectrumAnalysis_GF, "Integral", -1),
				new TGLayoutHints(kLHintsNormal, 5,5,0,0));
  SpectrumIntegral_NEFL->GetEntry()->SetFormat(TGNumberFormat::kNESReal, TGNumberFormat::kNEANonNegative);
  SpectrumIntegral_NEFL->GetEntry()->Resize(100,20);
  SpectrumIntegral_NEFL->GetEntry()->SetState(false);
  
  SpectrumAnalysis_GF->AddFrame(SpectrumIntegralError_NEFL = new ADAQNumberEntryFieldWithLabel(SpectrumAnalysis_GF, "Error", -1),
				new TGLayoutHints(kLHintsNormal, 5,5,0,0));
  SpectrumIntegralError_NEFL->GetEntry()->SetFormat(TGNumberFormat::kNESReal, TGNumberFormat::kNEANonNegative);
  SpectrumIntegralError_NEFL->GetEntry()->Resize(100,20);
  SpectrumIntegralError_NEFL->GetEntry()->SetState(false);


  TGHorizontalFrame *Horizontal2_HF = new TGHorizontalFrame(SpectrumAnalysis_GF);
  SpectrumAnalysis_GF->AddFrame(Horizontal2_HF, new TGLayoutHints(kLHintsNormal, 0,0,10,5));

  Horizontal2_HF->AddFrame(SpectrumUseGaussianFit_CB = new TGCheckButton(Horizontal2_HF, "Use gaussian fit", SpectrumUseGaussianFit_CB_ID),
			   new TGLayoutHints(kLHintsNormal, 5,5,0,0));
  SpectrumUseGaussianFit_CB->Connect("Clicked()", "AAInterface", this, "HandleCheckButtons()");


  TGHorizontalFrame *Horizontal3_HF = new TGHorizontalFrame(SpectrumAnalysis_GF);
  SpectrumAnalysis_GF->AddFrame(Horizontal3_HF, new TGLayoutHints(kLHintsNormal, 0,0,0,0));
  
  Horizontal3_HF->AddFrame(SpectrumFitHeight_NEFL = new ADAQNumberEntryFieldWithLabel(Horizontal3_HF, "Height",-1),
			   new TGLayoutHints(kLHintsNormal, 5,5,0,0));
  SpectrumFitHeight_NEFL->GetEntry()->SetFormat(TGNumberFormat::kNESReal);
  SpectrumFitHeight_NEFL->GetEntry()->SetState(false);
  SpectrumFitHeight_NEFL->GetEntry()->Resize(60, 20);
  
  Horizontal3_HF->AddFrame(SpectrumFitSigma_NEFL = new ADAQNumberEntryFieldWithLabel(Horizontal3_HF, "Sigma  ",-1),
				new TGLayoutHints(kLHintsNormal, 10,5,0,0));
  SpectrumFitSigma_NEFL->GetEntry()->SetFormat(TGNumberFormat::kNESReal);
  SpectrumFitSigma_NEFL->GetEntry()->SetState(false);
  SpectrumFitSigma_NEFL->GetEntry()->Resize(60, 20);
  
  
  TGHorizontalFrame *Horizontal4_HF = new TGHorizontalFrame(SpectrumAnalysis_GF);
  SpectrumAnalysis_GF->AddFrame(Horizontal4_HF, new TGLayoutHints(kLHintsNormal, 0,0,0,5));

  Horizontal4_HF->AddFrame(SpectrumFitMean_NEFL = new ADAQNumberEntryFieldWithLabel(Horizontal4_HF, "Mean  ",-1),
			   new TGLayoutHints(kLHintsNormal, 5,5,0,0));
  SpectrumFitMean_NEFL->GetEntry()->SetFormat(TGNumberFormat::kNESReal);
  SpectrumFitMean_NEFL->GetEntry()->SetState(false);
  SpectrumFitMean_NEFL->GetEntry()->Resize(60, 20);

  Horizontal4_HF->AddFrame(SpectrumFitRes_NEFL = new ADAQNumberEntryFieldWithLabel(Horizontal4_HF, "Res. (%)",-1),
				new TGLayoutHints(kLHintsNormal, 10,5,0,0));
  SpectrumFitRes_NEFL->GetEntry()->SetFormat(TGNumberFormat::kNESReal);
  SpectrumFitRes_NEFL->GetEntry()->SetState(false);
  SpectrumFitRes_NEFL->GetEntry()->Resize(60, 20);


  ////////////////////////////////
  // Spectrum energy analysis (EA)

  TGGroupFrame *EA_GF = new TGGroupFrame(AnalysisFrame_VF, "Energy analysis", kVerticalFrame);
  AnalysisFrame_VF->AddFrame(EA_GF, new TGLayoutHints(kLHintsNormal, 15,5,5,5));


  TGHorizontalFrame *EA_HF0 = new TGHorizontalFrame(EA_GF);
  EA_GF->AddFrame(EA_HF0, new TGLayoutHints(kLHintsNormal, 5,5,5,5));
  
  EA_HF0->AddFrame(EAEnable_CB = new TGCheckButton(EA_HF0, "Enable", EAEnable_CB_ID),
		   new TGLayoutHints(kLHintsNormal, 0,0,7,5));
  EAEnable_CB->Connect("Clicked()", "AAInterface", this, "HandleCheckButtons()");
  
  EA_HF0->AddFrame(EASpectrumType_CBL = new ADAQComboBoxWithLabel(EA_HF0, "", EASpectrumType_CBL_ID),
		   new TGLayoutHints(kLHintsNormal, 20,5,5,5));
  EASpectrumType_CBL->GetComboBox()->AddEntry("Gamma spectrum", 0);
  EASpectrumType_CBL->GetComboBox()->AddEntry("Neutron spectrum",1);
  EASpectrumType_CBL->GetComboBox()->Select(0);
  EASpectrumType_CBL->GetComboBox()->Resize(130,20);
  EASpectrumType_CBL->GetComboBox()->SetEnabled(false);
  EASpectrumType_CBL->GetComboBox()->Connect("Selected(int,int)", "AAInterface", this, "HandleComboBox(int,int)");
  

  EA_GF->AddFrame(EAGammaEDep_NEL = new ADAQNumberEntryWithLabel(EA_GF, "Energy deposited", EAGammaEDep_NEL_ID),
		  new TGLayoutHints(kLHintsNormal, 5,5,5,0));
  EAGammaEDep_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  EAGammaEDep_NEL->GetEntry()->SetNumber(0.);
  EAGammaEDep_NEL->GetEntry()->SetState(false);
  EAGammaEDep_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");

  EA_GF->AddFrame(EAEscapePeaks_CB = new TGCheckButton(EA_GF, "Predict escape peaks", EAEscapePeaks_CB_ID),
		  new TGLayoutHints(kLHintsNormal, 5,5,0,10));
  EAEscapePeaks_CB->Connect("Clicked()", "AAInterface", this, "HandleCheckButtons()");
  EAEscapePeaks_CB->SetState(kButtonDisabled);


  EA_GF->AddFrame(EALightConversionFactor_NEL = new ADAQNumberEntryWithLabel(EA_GF, "Conversion factor", EALightConversionFactor_NEL_ID),
		   new TGLayoutHints(kLHintsNormal, 5,5,5,0));
  EALightConversionFactor_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  EALightConversionFactor_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  EALightConversionFactor_NEL->GetEntry()->SetNumber(1.);
  EALightConversionFactor_NEL->GetEntry()->SetState(false);
  EALightConversionFactor_NEL->GetEntry()->Connect("ValueSet(long", "AAInterface", this, "HandleNumberEntries()");
    
  EA_GF->AddFrame(EAErrorWidth_NEL = new ADAQNumberEntryWithLabel(EA_GF, "Error width [%]", EAErrorWidth_NEL_ID),
		   new TGLayoutHints(kLHintsNormal, 5,5,0,5));
  EAErrorWidth_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  EAErrorWidth_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  EAErrorWidth_NEL->GetEntry()->SetNumber(5);
  EAErrorWidth_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");
  EAErrorWidth_NEL->GetEntry()->SetState(false);
  
  EA_GF->AddFrame(EAElectronEnergy_NEL = new ADAQNumberEntryWithLabel(EA_GF, "Electron [MeV]", EAElectronEnergy_NEL_ID),
		   new TGLayoutHints(kLHintsNormal, 5,5,0,0));
  EAElectronEnergy_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESRealThree);
  EAElectronEnergy_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  EAElectronEnergy_NEL->GetEntry()->Resize(70,20);
  EAElectronEnergy_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");
  EAElectronEnergy_NEL->GetEntry()->SetState(false);

  EA_GF->AddFrame(EAGammaEnergy_NEL = new ADAQNumberEntryWithLabel(EA_GF, "Gamma [MeV]", EAGammaEnergy_NEL_ID),
		   new TGLayoutHints(kLHintsNormal, 5,5,0,0));
  EAGammaEnergy_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESRealThree);
  EAGammaEnergy_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  EAGammaEnergy_NEL->GetEntry()->Resize(70,20);
  EAGammaEnergy_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");
  EAGammaEnergy_NEL->GetEntry()->SetState(false);

  EA_GF->AddFrame(EAProtonEnergy_NEL = new ADAQNumberEntryWithLabel(EA_GF, "Proton (neutron) [MeV]", EAProtonEnergy_NEL_ID),
		   new TGLayoutHints(kLHintsNormal, 5,5,0,0));
  EAProtonEnergy_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESRealThree);
  EAProtonEnergy_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  EAProtonEnergy_NEL->GetEntry()->Resize(70,20);
  EAProtonEnergy_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");
  EAProtonEnergy_NEL->GetEntry()->SetState(false);

  EA_GF->AddFrame(EAAlphaEnergy_NEL = new ADAQNumberEntryWithLabel(EA_GF, "Alpha [MeV]", EAAlphaEnergy_NEL_ID),
		   new TGLayoutHints(kLHintsNormal, 5,5,0,0));
  EAAlphaEnergy_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESRealThree);
  EAAlphaEnergy_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  EAAlphaEnergy_NEL->GetEntry()->Resize(70,20);
  EAAlphaEnergy_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");
  EAAlphaEnergy_NEL->GetEntry()->SetState(false);

  EA_GF->AddFrame(EACarbonEnergy_NEL = new ADAQNumberEntryWithLabel(EA_GF, "Carbon [GeV]", EACarbonEnergy_NEL_ID),
		   new TGLayoutHints(kLHintsNormal, 5,5,0,5));
  EACarbonEnergy_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESRealThree);
  EACarbonEnergy_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  EACarbonEnergy_NEL->GetEntry()->Resize(70,20);
  EACarbonEnergy_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");
  EACarbonEnergy_NEL->GetEntry()->SetState(false);
}


void AAInterface::FillGraphicsFrame()
{
  /////////////////////////////////////////
  // Fill the graphics options tabbed frame

  TGCanvas *GraphicsFrame_C = new TGCanvas(GraphicsOptions_CF, 320, LeftFrameLength-20, kDoubleBorder);
  GraphicsOptions_CF->AddFrame(GraphicsFrame_C, new TGLayoutHints(kLHintsCenterX, 0,0,10,10));
  
  TGVerticalFrame *GraphicsFrame_VF = new TGVerticalFrame(GraphicsFrame_C->GetViewPort(), 320, LeftFrameLength);
  GraphicsFrame_C->SetContainer(GraphicsFrame_VF);
  
  GraphicsFrame_VF->AddFrame(WaveformDrawOptions_BG = new TGButtonGroup(GraphicsFrame_VF, "Waveform draw options", kHorizontalFrame),
			      new TGLayoutHints(kLHintsNormal, 5,5,5,5));
  
  DrawWaveformWithCurve_RB = new TGRadioButton(WaveformDrawOptions_BG, "Curve   ", DrawWaveformWithCurve_RB_ID);
  DrawWaveformWithCurve_RB->Connect("Clicked()", "AAInterface", this, "HandleRadioButtons()");
  DrawWaveformWithCurve_RB->SetState(kButtonDown);
  
  DrawWaveformWithMarkers_RB = new TGRadioButton(WaveformDrawOptions_BG, "Markers   ", DrawWaveformWithMarkers_RB_ID);
  DrawWaveformWithMarkers_RB->Connect("Clicked()", "AAInterface", this, "HandleRadioButtons()");
  
  DrawWaveformWithBoth_RB = new TGRadioButton(WaveformDrawOptions_BG, "Both", DrawWaveformWithBoth_RB_ID);
  DrawWaveformWithBoth_RB->Connect("Clicked()", "AAInterface", this, "HandleRadioButtons()");
  
  GraphicsFrame_VF->AddFrame(SpectrumDrawOptions_BG = new TGButtonGroup(GraphicsFrame_VF, "Spectrum draw options", kHorizontalFrame),
			      new TGLayoutHints(kLHintsNormal, 5,5,5,5));
  
  DrawSpectrumWithCurve_RB = new TGRadioButton(SpectrumDrawOptions_BG, "Curve  ", DrawSpectrumWithCurve_RB_ID);
  DrawSpectrumWithCurve_RB->Connect("Clicked()", "AAInterface", this, "HandleRadioButtons()");
  DrawSpectrumWithCurve_RB->SetState(kButtonDown);
  
  DrawSpectrumWithMarkers_RB = new TGRadioButton(SpectrumDrawOptions_BG, "Markers  ", DrawSpectrumWithMarkers_RB_ID);
  DrawSpectrumWithMarkers_RB->Connect("Clicked()", "AAInterface", this, "HandleRadioButtons()");

  DrawSpectrumWithError_RB = new TGRadioButton(SpectrumDrawOptions_BG, "Error  ", DrawSpectrumWithError_RB_ID);
  DrawSpectrumWithError_RB->Connect("Clicked()", "AAInterface", this, "HandleRadioButtons()");

  DrawSpectrumWithBars_RB = new TGRadioButton(SpectrumDrawOptions_BG, "Bars", DrawSpectrumWithBars_RB_ID);
  DrawSpectrumWithBars_RB->Connect("Clicked()", "AAInterface", this, "HandleRadioButtons()");


  TGHorizontalFrame *CanvasOptions_HF0 = new TGHorizontalFrame(GraphicsFrame_VF);
  GraphicsFrame_VF->AddFrame(CanvasOptions_HF0, new TGLayoutHints(kLHintsLeft, 5,5,5,0));

  CanvasOptions_HF0->AddFrame(HistogramStats_CB = new TGCheckButton(CanvasOptions_HF0, "Histogram stats", HistogramStats_CB_ID),
			      new TGLayoutHints(kLHintsLeft, 0,0,0,0));
  HistogramStats_CB->Connect("Clicked()", "AAInterface", this, "HandleCheckButtons()");
  
  CanvasOptions_HF0->AddFrame(CanvasGrid_CB = new TGCheckButton(CanvasOptions_HF0, "Grid", CanvasGrid_CB_ID),
			      new TGLayoutHints(kLHintsLeft, 25,0,0,0));
  CanvasGrid_CB->Connect("Clicked()", "AAInterface", this, "HandleCheckButtons()");
  CanvasGrid_CB->SetState(kButtonDown);


  TGHorizontalFrame *CanvasOptions_HF1 = new TGHorizontalFrame(GraphicsFrame_VF);
  GraphicsFrame_VF->AddFrame(CanvasOptions_HF1, new TGLayoutHints(kLHintsLeft, 5,5,5,0));
  
  CanvasOptions_HF1->AddFrame(CanvasXAxisLog_CB = new TGCheckButton(CanvasOptions_HF1, "X-axis log", CanvasXAxisLog_CB_ID),
			     new TGLayoutHints(kLHintsLeft, 0,0,0,0));
  CanvasXAxisLog_CB->Connect("Clicked()", "AAInterface", this, "HandleCheckButtons()");
  
  CanvasOptions_HF1->AddFrame(CanvasYAxisLog_CB = new TGCheckButton(CanvasOptions_HF1, "Y-axis log", CanvasYAxisLog_CB_ID),
			      new TGLayoutHints(kLHintsLeft, 15,0,0,0));
  CanvasYAxisLog_CB->Connect("Clicked()", "AAInterface", this, "HandleCheckButtons()");
  
  CanvasOptions_HF1->AddFrame(CanvasZAxisLog_CB = new TGCheckButton(CanvasOptions_HF1, "Z-axis log", CanvasZAxisLog_CB_ID),
			      new TGLayoutHints(kLHintsLeft, 5,5,0,0));
  CanvasZAxisLog_CB->Connect("Clicked()", "AAInterface", this, "HandleCheckButtons()");
  

  GraphicsFrame_VF->AddFrame(PlotSpectrumDerivativeError_CB = new TGCheckButton(GraphicsFrame_VF, "Plot spectrum derivative error", PlotSpectrumDerivativeError_CB_ID),
			     new TGLayoutHints(kLHintsNormal, 5,5,5,0));
  PlotSpectrumDerivativeError_CB->Connect("Clicked()", "AAInterface", this, "HandleCheckButtons()");
  
  GraphicsFrame_VF->AddFrame(PlotAbsValueSpectrumDerivative_CB = new TGCheckButton(GraphicsFrame_VF, "Plot abs. value of spectrum derivative ", PlotAbsValueSpectrumDerivative_CB_ID),
			     new TGLayoutHints(kLHintsNormal, 5,5,5,0));
  PlotAbsValueSpectrumDerivative_CB->Connect("Clicked()", "AAInterface", this, "HandleCheckButtons()");
  
  GraphicsFrame_VF->AddFrame(AutoYAxisRange_CB = new TGCheckButton(GraphicsFrame_VF, "Auto. Y Axis Range (waveform only)", -1),
			       new TGLayoutHints(kLHintsLeft, 5,5,5,5));

  TGHorizontalFrame *ResetAxesLimits_HF = new TGHorizontalFrame(GraphicsFrame_VF);
  GraphicsFrame_VF->AddFrame(ResetAxesLimits_HF, new TGLayoutHints(kLHintsLeft,15,5,5,5));

  TGHorizontalFrame *PlotButtons_HF0 = new TGHorizontalFrame(GraphicsFrame_VF);
  GraphicsFrame_VF->AddFrame(PlotButtons_HF0, new TGLayoutHints(kLHintsCenterX, 5,5,5,5));
  
  PlotButtons_HF0->AddFrame(ReplotWaveform_TB = new TGTextButton(PlotButtons_HF0, "Plot waveform", ReplotWaveform_TB_ID),
			    new TGLayoutHints(kLHintsCenterX, 5,5,0,0));
  ReplotWaveform_TB->SetBackgroundColor(ColorMgr->Number2Pixel(36));
  ReplotWaveform_TB->SetForegroundColor(ColorMgr->Number2Pixel(0));
  ReplotWaveform_TB->Resize(130, 30);
  ReplotWaveform_TB->ChangeOptions(ReplotWaveform_TB->GetOptions() | kFixedSize);
  ReplotWaveform_TB->Connect("Clicked()", "AAInterface", this, "HandleTextButtons()");
  
  PlotButtons_HF0->AddFrame(ReplotSpectrum_TB = new TGTextButton(PlotButtons_HF0, "Plot spectrum", ReplotSpectrum_TB_ID),
			    new TGLayoutHints(kLHintsCenterX, 5,5,0,0));
  ReplotSpectrum_TB->SetBackgroundColor(ColorMgr->Number2Pixel(36));
  ReplotSpectrum_TB->SetForegroundColor(ColorMgr->Number2Pixel(0));
  ReplotSpectrum_TB->Resize(130, 30);
  ReplotSpectrum_TB->ChangeOptions(ReplotSpectrum_TB->GetOptions() | kFixedSize);
  ReplotSpectrum_TB->Connect("Clicked()", "AAInterface", this, "HandleTextButtons()");
  

  TGHorizontalFrame *PlotButtons_HF1 = new TGHorizontalFrame(GraphicsFrame_VF);
  GraphicsFrame_VF->AddFrame(PlotButtons_HF1, new TGLayoutHints(kLHintsCenterX, 5,5,5,20));

  PlotButtons_HF1->AddFrame(ReplotSpectrumDerivative_TB = new TGTextButton(PlotButtons_HF1, "Plot derivative", ReplotSpectrumDerivative_TB_ID),
			    new TGLayoutHints(kLHintsCenterX, 5,5,0,0));
  ReplotSpectrumDerivative_TB->Resize(130, 30);
  ReplotSpectrumDerivative_TB->SetBackgroundColor(ColorMgr->Number2Pixel(36));
  ReplotSpectrumDerivative_TB->SetForegroundColor(ColorMgr->Number2Pixel(0));
  ReplotSpectrumDerivative_TB->ChangeOptions(ReplotSpectrumDerivative_TB->GetOptions() | kFixedSize);
  ReplotSpectrumDerivative_TB->Connect("Clicked()", "AAInterface", this, "HandleTextButtons()");

  PlotButtons_HF1->AddFrame(ReplotPSDHistogram_TB = new TGTextButton(PlotButtons_HF1, "Plot PSD histogram", ReplotPSDHistogram_TB_ID),
			    new TGLayoutHints(kLHintsCenterX, 5,5,0,0));
  ReplotPSDHistogram_TB->SetBackgroundColor(ColorMgr->Number2Pixel(36));
  ReplotPSDHistogram_TB->SetForegroundColor(ColorMgr->Number2Pixel(0));
  ReplotPSDHistogram_TB->Resize(130, 30);
  ReplotPSDHistogram_TB->ChangeOptions(ReplotPSDHistogram_TB->GetOptions() | kFixedSize);
  ReplotPSDHistogram_TB->Connect("Clicked()", "AAInterface", this, "HandleTextButtons()");

  // Override default plot options
  GraphicsFrame_VF->AddFrame(OverrideTitles_CB = new TGCheckButton(GraphicsFrame_VF, "Override default graphical options", OverrideTitles_CB_ID),
			       new TGLayoutHints(kLHintsCenterX, 5,5,5,5));
  OverrideTitles_CB->Connect("Clicked()", "AAInterface", this, "HandleCheckButtons()");

  // Plot title text entry
  GraphicsFrame_VF->AddFrame(Title_TEL = new ADAQTextEntryWithLabel(GraphicsFrame_VF, "Plot title", Title_TEL_ID),
			       new TGLayoutHints(kLHintsCenterX, 5,5,5,5));

  // X-axis title options

  TGGroupFrame *XAxis_GF = new TGGroupFrame(GraphicsFrame_VF, "X Axis", kVerticalFrame);
  GraphicsFrame_VF->AddFrame(XAxis_GF, new TGLayoutHints(kLHintsCenterX, 5,5,5,5));

  XAxis_GF->AddFrame(XAxisTitle_TEL = new ADAQTextEntryWithLabel(XAxis_GF, "Title", XAxisTitle_TEL_ID),
		     new TGLayoutHints(kLHintsNormal, 0,5,5,0));
  XAxisTitle_TEL->GetEntry()->Resize(200, 20);

  TGHorizontalFrame *XAxisTitleOptions_HF = new TGHorizontalFrame(XAxis_GF);
  XAxis_GF->AddFrame(XAxisTitleOptions_HF, new TGLayoutHints(kLHintsNormal, 0,5,0,5));
  
  XAxisTitleOptions_HF->AddFrame(XAxisSize_NEL = new ADAQNumberEntryWithLabel(XAxisTitleOptions_HF, "Size", XAxisSize_NEL_ID),		
				 new TGLayoutHints(kLHintsNormal, 0,5,0,0));
  XAxisSize_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  XAxisSize_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  XAxisSize_NEL->GetEntry()->SetNumber(0.04);
  XAxisSize_NEL->GetEntry()->Resize(50, 20);
  XAxisSize_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");
  
  XAxisTitleOptions_HF->AddFrame(XAxisOffset_NEL = new ADAQNumberEntryWithLabel(XAxisTitleOptions_HF, "Offset", XAxisOffset_NEL_ID),		
				 new TGLayoutHints(kLHintsNormal, 5,2,0,0));
  XAxisOffset_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  XAxisOffset_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  XAxisOffset_NEL->GetEntry()->SetNumber(1.5);
  XAxisOffset_NEL->GetEntry()->Resize(50, 20);
  XAxisOffset_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");

  XAxis_GF->AddFrame(XAxisDivs_NEL = new ADAQNumberEntryWithLabel(XAxis_GF, "Divisions", XAxisDivs_NEL_ID),		
		     new TGLayoutHints(kLHintsNormal, 0,0,0,0));
  XAxisDivs_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  XAxisDivs_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  XAxisDivs_NEL->GetEntry()->SetNumber(510);
  XAxisDivs_NEL->GetEntry()->Resize(50, 20);
  XAxisDivs_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");

  // Y-axis title options

  TGGroupFrame *YAxis_GF = new TGGroupFrame(GraphicsFrame_VF, "Y Axis", kVerticalFrame);
  GraphicsFrame_VF->AddFrame(YAxis_GF, new TGLayoutHints(kLHintsCenterX, 5,5,5,5));
  
  YAxis_GF->AddFrame(YAxisTitle_TEL = new ADAQTextEntryWithLabel(YAxis_GF, "Title", YAxisTitle_TEL_ID),
			       new TGLayoutHints(kLHintsNormal, 0,5,5,0));
  YAxisTitle_TEL->GetEntry()->Resize(200, 20);
  
  TGHorizontalFrame *YAxisTitleOptions_HF = new TGHorizontalFrame(YAxis_GF);
  YAxis_GF->AddFrame(YAxisTitleOptions_HF, new TGLayoutHints(kLHintsNormal, 0,5,0,5));

  YAxisTitleOptions_HF->AddFrame(YAxisSize_NEL = new ADAQNumberEntryWithLabel(YAxisTitleOptions_HF, "Size", YAxisSize_NEL_ID),		
				 new TGLayoutHints(kLHintsNormal, 0,5,0,0));
  YAxisSize_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  YAxisSize_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  YAxisSize_NEL->GetEntry()->SetNumber(0.04);
  YAxisSize_NEL->GetEntry()->Resize(50, 20);
  YAxisSize_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");
  
  YAxisTitleOptions_HF->AddFrame(YAxisOffset_NEL = new ADAQNumberEntryWithLabel(YAxisTitleOptions_HF, "Offset", YAxisOffset_NEL_ID),		
				 new TGLayoutHints(kLHintsNormal, 5,2,0,0));
  YAxisOffset_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  YAxisOffset_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  YAxisOffset_NEL->GetEntry()->SetNumber(1.5);
  YAxisOffset_NEL->GetEntry()->Resize(50, 20);
  YAxisOffset_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");

  YAxis_GF->AddFrame(YAxisDivs_NEL = new ADAQNumberEntryWithLabel(YAxis_GF, "Divisions", YAxisDivs_NEL_ID),		
		     new TGLayoutHints(kLHintsNormal, 0,0,0,0));
  YAxisDivs_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  YAxisDivs_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  YAxisDivs_NEL->GetEntry()->SetNumber(510);
  YAxisDivs_NEL->GetEntry()->Resize(50, 20);
  YAxisDivs_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");

  // Z-axis options

  TGGroupFrame *ZAxis_GF = new TGGroupFrame(GraphicsFrame_VF, "Z Axis", kVerticalFrame);
  GraphicsFrame_VF->AddFrame(ZAxis_GF, new TGLayoutHints(kLHintsCenterX, 5,5,5,5));

  ZAxis_GF->AddFrame(ZAxisTitle_TEL = new ADAQTextEntryWithLabel(ZAxis_GF, "Title", ZAxisTitle_TEL_ID),
			       new TGLayoutHints(kLHintsNormal, 0,5,5,0));
  ZAxisTitle_TEL->GetEntry()->Resize(200,20);

  TGHorizontalFrame *ZAxisTitleOptions_HF = new TGHorizontalFrame(ZAxis_GF);
  ZAxis_GF->AddFrame(ZAxisTitleOptions_HF, new TGLayoutHints(kLHintsNormal, 0,5,0,5));

  ZAxisTitleOptions_HF->AddFrame(ZAxisSize_NEL = new ADAQNumberEntryWithLabel(ZAxisTitleOptions_HF, "Size", ZAxisSize_NEL_ID),		
				 new TGLayoutHints(kLHintsNormal, 0,5,0,0));
  ZAxisSize_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  ZAxisSize_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  ZAxisSize_NEL->GetEntry()->SetNumber(0.04);
  ZAxisSize_NEL->GetEntry()->Resize(50, 20);
  ZAxisSize_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");
  
  ZAxisTitleOptions_HF->AddFrame(ZAxisOffset_NEL = new ADAQNumberEntryWithLabel(ZAxisTitleOptions_HF, "Offset", ZAxisOffset_NEL_ID),		
				 new TGLayoutHints(kLHintsNormal, 5,2,0,0));
  ZAxisOffset_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  ZAxisOffset_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  ZAxisOffset_NEL->GetEntry()->SetNumber(1.5);
  ZAxisOffset_NEL->GetEntry()->Resize(50, 20);
  ZAxisOffset_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");

  ZAxis_GF->AddFrame(ZAxisDivs_NEL = new ADAQNumberEntryWithLabel(ZAxis_GF, "Divisions", ZAxisDivs_NEL_ID),		
		     new TGLayoutHints(kLHintsNormal, 0,0,0,0));
  ZAxisDivs_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  ZAxisDivs_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  ZAxisDivs_NEL->GetEntry()->SetNumber(510);
  ZAxisDivs_NEL->GetEntry()->Resize(50, 20);
  ZAxisDivs_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");

  // Color palette options

  TGGroupFrame *PaletteAxis_GF = new TGGroupFrame(GraphicsFrame_VF, "Palette Axis", kVerticalFrame);
  GraphicsFrame_VF->AddFrame(PaletteAxis_GF, new TGLayoutHints(kLHintsCenterX, 5,5,5,5));

  PaletteAxis_GF->AddFrame(PaletteAxisTitle_TEL = new ADAQTextEntryWithLabel(PaletteAxis_GF, "Title", PaletteAxisTitle_TEL_ID),
			       new TGLayoutHints(kLHintsNormal, 0,5,5,0));
  PaletteAxisTitle_TEL->GetEntry()->Resize(200,20);
  
  // A horizontal frame for the Z-axis title/label size and offset
  TGHorizontalFrame *PaletteAxisTitleOptions_HF = new TGHorizontalFrame(PaletteAxis_GF);
  PaletteAxis_GF->AddFrame(PaletteAxisTitleOptions_HF, new TGLayoutHints(kLHintsNormal, 0,5,0,5));

  PaletteAxisTitleOptions_HF->AddFrame(PaletteAxisSize_NEL = new ADAQNumberEntryWithLabel(PaletteAxisTitleOptions_HF, "Size", PaletteAxisSize_NEL_ID),		
				 new TGLayoutHints(kLHintsNormal, 0,5,0,0));
  PaletteAxisSize_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  PaletteAxisSize_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  PaletteAxisSize_NEL->GetEntry()->SetNumber(0.04);
  PaletteAxisSize_NEL->GetEntry()->Resize(50, 20);
  PaletteAxisSize_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");
  
  PaletteAxisTitleOptions_HF->AddFrame(PaletteAxisOffset_NEL = new ADAQNumberEntryWithLabel(PaletteAxisTitleOptions_HF, "Offset", PaletteAxisOffset_NEL_ID),		
				 new TGLayoutHints(kLHintsNormal, 5,2,0,0));
  PaletteAxisOffset_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  PaletteAxisOffset_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  PaletteAxisOffset_NEL->GetEntry()->SetNumber(1.1);
  PaletteAxisOffset_NEL->GetEntry()->Resize(50, 20);
  PaletteAxisOffset_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");

  TGHorizontalFrame *PaletteAxisPositions_HF1 = new TGHorizontalFrame(PaletteAxis_GF);
  PaletteAxis_GF->AddFrame(PaletteAxisPositions_HF1, new TGLayoutHints(kLHintsNormal, 0,5,0,5));

  PaletteAxisPositions_HF1->AddFrame(PaletteX1_NEL = new ADAQNumberEntryWithLabel(PaletteAxisPositions_HF1, "X1", -1),
				    new TGLayoutHints(kLHintsNormal, 0,5,0,0));
  PaletteX1_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  PaletteX1_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  PaletteX1_NEL->GetEntry()->SetNumber(0.85);
  PaletteX1_NEL->GetEntry()->Resize(50, 20);

  PaletteAxisPositions_HF1->AddFrame(PaletteX2_NEL = new ADAQNumberEntryWithLabel(PaletteAxisPositions_HF1, "X2", -1),
				    new TGLayoutHints(kLHintsNormal, 0,5,0,0));
  PaletteX2_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  PaletteX2_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  PaletteX2_NEL->GetEntry()->SetNumber(0.90);
  PaletteX2_NEL->GetEntry()->Resize(50, 20);

  TGHorizontalFrame *PaletteAxisPositions_HF2 = new TGHorizontalFrame(PaletteAxis_GF);
  PaletteAxis_GF->AddFrame(PaletteAxisPositions_HF2, new TGLayoutHints(kLHintsNormal, 0,5,0,5));

  PaletteAxisPositions_HF2->AddFrame(PaletteY1_NEL = new ADAQNumberEntryWithLabel(PaletteAxisPositions_HF2, "Y1", -1),
				    new TGLayoutHints(kLHintsNormal, 0,5,0,0));
  PaletteY1_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  PaletteY1_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  PaletteY1_NEL->GetEntry()->SetNumber(0.05);
  PaletteY1_NEL->GetEntry()->Resize(50, 20);

  PaletteAxisPositions_HF2->AddFrame(PaletteY2_NEL = new ADAQNumberEntryWithLabel(PaletteAxisPositions_HF2, "Y2", -1),
				    new TGLayoutHints(kLHintsNormal, 0,5,0,0));
  PaletteY2_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  PaletteY2_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  PaletteY2_NEL->GetEntry()->SetNumber(0.65);
  PaletteY2_NEL->GetEntry()->Resize(50, 20);
}


void AAInterface::FillProcessingFrame()
{
  TGCanvas *ProcessingFrame_C = new TGCanvas(ProcessingOptions_CF, 320, LeftFrameLength-20, kDoubleBorder);
  ProcessingOptions_CF->AddFrame(ProcessingFrame_C, new TGLayoutHints(kLHintsCenterX, 0,0,10,10));
  
  TGVerticalFrame *ProcessingFrame_VF = new TGVerticalFrame(ProcessingFrame_C->GetViewPort(), 320, LeftFrameLength);
  ProcessingFrame_C->SetContainer(ProcessingFrame_VF);


  //////////////////////////////
  // Waveform processing options
  
  TGGroupFrame *ProcessingOptions_GF = new TGGroupFrame(ProcessingFrame_VF, "Waveform processing options", kVerticalFrame);
  ProcessingFrame_VF->AddFrame(ProcessingOptions_GF, new TGLayoutHints(kLHintsCenterX, 5,5,5,5));

  TGHorizontalFrame *Architecture_HF = new TGHorizontalFrame(ProcessingOptions_GF);
  ProcessingOptions_GF->AddFrame(Architecture_HF, new TGLayoutHints(kLHintsCenterX, 5,5,5,5));

  Architecture_HF->AddFrame(ProcessingSeq_RB = new TGRadioButton(Architecture_HF, "Sequential", ProcessingSeq_RB_ID),
			    new TGLayoutHints(kLHintsLeft, 5,5,0,0));
  ProcessingSeq_RB->Connect("Clicked()", "AAInterface", this, "HandleRadioButtons()");
  ProcessingSeq_RB->SetState(kButtonDown);

  Architecture_HF->AddFrame(ProcessingPar_RB = new TGRadioButton(Architecture_HF, "Parallel", ProcessingPar_RB_ID),
			    new TGLayoutHints(kLHintsLeft, 15,5,0,0));
  ProcessingPar_RB->Connect("Clicked()", "AAInterface", this, "HandleRadioButtons()");
  
  ProcessingOptions_GF->AddFrame(NumProcessors_NEL = new ADAQNumberEntryWithLabel(ProcessingOptions_GF, "Number of Processors", -1),
				 new TGLayoutHints(kLHintsNormal, 0,0,5,0));
  NumProcessors_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  NumProcessors_NEL->GetEntry()->SetNumLimits(TGNumberFormat::kNELLimitMinMax);
  NumProcessors_NEL->GetEntry()->SetLimitValues(1,NumProcessors);
  NumProcessors_NEL->GetEntry()->SetNumber(1);
  NumProcessors_NEL->GetEntry()->SetState(false);
  
  ProcessingOptions_GF->AddFrame(UpdateFreq_NEL = new ADAQNumberEntryWithLabel(ProcessingOptions_GF, "Update freq (% done)", -1),
				 new TGLayoutHints(kLHintsNormal, 0,0,5,0));
  UpdateFreq_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  UpdateFreq_NEL->GetEntry()->SetNumLimits(TGNumberFormat::kNELLimitMinMax);
  UpdateFreq_NEL->GetEntry()->SetLimitValues(1,100);
  UpdateFreq_NEL->GetEntry()->SetNumber(2);

  
  ///////////////////////////////////////////
  // Pulse-shape discrimination (PSD) options

  TGGroupFrame *PSDAnalysis_GF = new TGGroupFrame(ProcessingFrame_VF, "Pulse Shape Discrimination", kVerticalFrame);
  ProcessingFrame_VF->AddFrame(PSDAnalysis_GF, new TGLayoutHints(kLHintsCenterX, 5,5,5,5));
  
  PSDAnalysis_GF->AddFrame(PSDEnable_CB = new TGCheckButton(PSDAnalysis_GF, "Discriminate pulse shapes", PSDEnable_CB_ID),
                           new TGLayoutHints(kLHintsNormal, 0,5,5,0));
  PSDEnable_CB->Connect("Clicked()", "AAInterface", this, "HandleCheckButtons()");

  PSDAnalysis_GF->AddFrame(PSDChannel_CBL = new ADAQComboBoxWithLabel(PSDAnalysis_GF, "", -1),
                           new TGLayoutHints(kLHintsNormal, 0,5,0,0));

  stringstream ss;
  string entry;
  for(int ch=0; ch<NumDataChannels; ch++){
    ss << ch;
    entry.assign("Channel " + ss.str());
    PSDChannel_CBL->GetComboBox()->AddEntry(entry.c_str(),ch);
    ss.str("");
  }
  PSDChannel_CBL->GetComboBox()->Select(0);
  PSDChannel_CBL->GetComboBox()->SetEnabled(false);

  PSDAnalysis_GF->AddFrame(PSDWaveforms_NEL = new ADAQNumberEntryWithLabel(PSDAnalysis_GF, "Number of waveforms", -1),
                           new TGLayoutHints(kLHintsNormal, 0,5,10,0));
  PSDWaveforms_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  PSDWaveforms_NEL->GetEntry()->SetNumLimits(TGNumberFormat::kNELLimitMinMax);
  PSDWaveforms_NEL->GetEntry()->SetLimitValues(0,1); // Updated when ADAQ ROOT file loaded
  PSDWaveforms_NEL->GetEntry()->SetNumber(0); // Updated when ADAQ ROOT file loaded
  PSDWaveforms_NEL->GetEntry()->SetState(false);

  PSDAnalysis_GF->AddFrame(PSDNumTotalBins_NEL = new ADAQNumberEntryWithLabel(PSDAnalysis_GF, "Num. total bins (X axis)", -1),
			   new TGLayoutHints(kLHintsNormal, 0,5,5,0));
  PSDNumTotalBins_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  PSDNumTotalBins_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  PSDNumTotalBins_NEL->GetEntry()->SetNumber(150);
  PSDNumTotalBins_NEL->GetEntry()->SetState(false);

  TGHorizontalFrame *TotalBins_HF = new TGHorizontalFrame(PSDAnalysis_GF);
  PSDAnalysis_GF->AddFrame(TotalBins_HF);

  TotalBins_HF->AddFrame(PSDMinTotalBin_NEL = new ADAQNumberEntryWithLabel(TotalBins_HF, "Min.", -1),
			 new TGLayoutHints(kLHintsNormal, 0,5,0,0));
  PSDMinTotalBin_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  PSDMinTotalBin_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  PSDMinTotalBin_NEL->GetEntry()->SetNumber(0);
  PSDMinTotalBin_NEL->GetEntry()->SetState(false);

  TotalBins_HF->AddFrame(PSDMaxTotalBin_NEL = new ADAQNumberEntryWithLabel(TotalBins_HF, "Max.", -1),
			 new TGLayoutHints(kLHintsNormal, 0,5,0,0));
  PSDMaxTotalBin_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  PSDMaxTotalBin_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  PSDMaxTotalBin_NEL->GetEntry()->SetNumber(20000);
  PSDMaxTotalBin_NEL->GetEntry()->SetState(false);
  
  PSDAnalysis_GF->AddFrame(PSDNumTailBins_NEL = new ADAQNumberEntryWithLabel(PSDAnalysis_GF, "Num. tail bins (Y axis)", -1),
			   new TGLayoutHints(kLHintsNormal, 0,5,5,0));
  PSDNumTailBins_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  PSDNumTailBins_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  PSDNumTailBins_NEL->GetEntry()->SetNumber(150);
  PSDNumTailBins_NEL->GetEntry()->SetState(false);

  TGHorizontalFrame *TailBins_HF = new TGHorizontalFrame(PSDAnalysis_GF);
  PSDAnalysis_GF->AddFrame(TailBins_HF);

  TailBins_HF->AddFrame(PSDMinTailBin_NEL = new ADAQNumberEntryWithLabel(TailBins_HF, "Min.", -1),
			   new TGLayoutHints(kLHintsNormal, 0,5,0,0));
  PSDMinTailBin_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  PSDMinTailBin_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  PSDMinTailBin_NEL->GetEntry()->SetNumber(0);
  PSDMinTailBin_NEL->GetEntry()->SetState(false);

  TailBins_HF->AddFrame(PSDMaxTailBin_NEL = new ADAQNumberEntryWithLabel(TailBins_HF, "Max.", -1),
			   new TGLayoutHints(kLHintsNormal, 0,5,0,0));
  PSDMaxTailBin_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  PSDMaxTailBin_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  PSDMaxTailBin_NEL->GetEntry()->SetNumber(5000);
  PSDMaxTailBin_NEL->GetEntry()->SetState(false);


  PSDAnalysis_GF->AddFrame(PSDThreshold_NEL = new ADAQNumberEntryWithLabel(PSDAnalysis_GF, "Threshold (ADC)", -1),
                           new TGLayoutHints(kLHintsNormal, 0,5,5,0));
  PSDThreshold_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  PSDThreshold_NEL->GetEntry()->SetNumLimits(TGNumberFormat::kNELLimitMinMax);
  PSDThreshold_NEL->GetEntry()->SetLimitValues(0,4095); // Updated when ADAQ ROOT file loaded
  PSDThreshold_NEL->GetEntry()->SetNumber(1500); // Updated when ADAQ ROOT file loaded
  PSDThreshold_NEL->GetEntry()->SetState(false);
  
  PSDAnalysis_GF->AddFrame(PSDPeakOffset_NEL = new ADAQNumberEntryWithLabel(PSDAnalysis_GF, "Peak offset (sample)", PSDPeakOffset_NEL_ID),
                           new TGLayoutHints(kLHintsNormal, 0,5,0,0));
  PSDPeakOffset_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  PSDPeakOffset_NEL->GetEntry()->SetNumber(7);
  PSDPeakOffset_NEL->GetEntry()->SetState(false);
  PSDPeakOffset_NEL->GetEntry()->Connect("ValueSet(long", "AAInterface", this, "HandleNumberEntries()");

  PSDAnalysis_GF->AddFrame(PSDTailOffset_NEL = new ADAQNumberEntryWithLabel(PSDAnalysis_GF, "Tail offset (sample)", PSDTailOffset_NEL_ID),
                           new TGLayoutHints(kLHintsNormal, 0,5,0,0));
  PSDTailOffset_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  PSDTailOffset_NEL->GetEntry()->SetNumber(29);
  PSDTailOffset_NEL->GetEntry()->SetState(false);
  PSDTailOffset_NEL->GetEntry()->Connect("ValueSet(long", "AAInterface", this, "HandleNumberEntries()");
  
  PSDAnalysis_GF->AddFrame(PSDPlotType_CBL = new ADAQComboBoxWithLabel(PSDAnalysis_GF, "Plot type", PSDPlotType_CBL_ID),
			   new TGLayoutHints(kLHintsNormal, 0,5,5,5));
  PSDPlotType_CBL->GetComboBox()->AddEntry("COL",0);
  PSDPlotType_CBL->GetComboBox()->AddEntry("LEGO2",1);
  PSDPlotType_CBL->GetComboBox()->AddEntry("SURF3",2);
  PSDPlotType_CBL->GetComboBox()->AddEntry("SURF4",3);
  PSDPlotType_CBL->GetComboBox()->AddEntry("CONT",4);
  PSDPlotType_CBL->GetComboBox()->Select(0);
  PSDPlotType_CBL->GetComboBox()->Connect("Selected(int,int)", "AAInterface", this, "HandleComboBox(int,int)");
  PSDPlotType_CBL->GetComboBox()->SetEnabled(false);

  PSDAnalysis_GF->AddFrame(PSDPlotTailIntegration_CB = new TGCheckButton(PSDAnalysis_GF, "Plot tail integration region", PSDPlotTailIntegration_CB_ID),
                           new TGLayoutHints(kLHintsNormal, 0,5,0,5));
  PSDPlotTailIntegration_CB->Connect("Clicked()", "AAInterface", this, "HandleCheckButtons()");
  PSDPlotTailIntegration_CB->SetState(kButtonDisabled);
  
  PSDAnalysis_GF->AddFrame(PSDEnableHistogramSlicing_CB = new TGCheckButton(PSDAnalysis_GF, "Enable histogram slicing", PSDEnableHistogramSlicing_CB_ID),
			   new TGLayoutHints(kLHintsNormal, 0,5,5,0));
  PSDEnableHistogramSlicing_CB->Connect("Clicked()", "AAInterface", this, "HandleCheckButtons()");
  PSDEnableHistogramSlicing_CB->SetState(kButtonDisabled);

  TGHorizontalFrame *PSDHistogramSlicing_HF = new TGHorizontalFrame(PSDAnalysis_GF);
  PSDAnalysis_GF->AddFrame(PSDHistogramSlicing_HF, new TGLayoutHints(kLHintsCenterX, 0,5,0,5));
  
  PSDHistogramSlicing_HF->AddFrame(PSDHistogramSliceX_RB = new TGRadioButton(PSDHistogramSlicing_HF, "X slice", PSDHistogramSliceX_RB_ID),
				   new TGLayoutHints(kLHintsNormal, 0,0,0,0));
  PSDHistogramSliceX_RB->Connect("Clicked()", "AAInterface", this, "HandleRadioButtons()");
  PSDHistogramSliceX_RB->SetState(kButtonDown);
  PSDHistogramSliceX_RB->SetState(kButtonDisabled);

  PSDHistogramSlicing_HF->AddFrame(PSDHistogramSliceY_RB = new TGRadioButton(PSDHistogramSlicing_HF, "Y slice", PSDHistogramSliceY_RB_ID),
				   new TGLayoutHints(kLHintsNormal, 20,0,0,0));
  PSDHistogramSliceY_RB->Connect("Clicked()", "AAInterface", this, "HandleRadioButtons()");
  PSDHistogramSliceY_RB->SetState(kButtonDisabled);
  
  PSDAnalysis_GF->AddFrame(PSDEnableFilterCreation_CB = new TGCheckButton(PSDAnalysis_GF, "Enable filter creation", PSDEnableFilterCreation_CB_ID),
			   new TGLayoutHints(kLHintsNormal, 0,5,5,0));
  PSDEnableFilterCreation_CB->Connect("Clicked()", "AAInterface", this, "HandleCheckButtons()");
  PSDEnableFilterCreation_CB->SetState(kButtonDisabled);

  
  PSDAnalysis_GF->AddFrame(PSDEnableFilter_CB = new TGCheckButton(PSDAnalysis_GF, "Enable filter use", PSDEnableFilter_CB_ID),
			   new TGLayoutHints(kLHintsNormal, 0,5,0,0));
  PSDEnableFilter_CB->Connect("Clicked()", "AAInterface", this, "HandleCheckButtons()");
  PSDEnableFilter_CB->SetState(kButtonDisabled);

  TGHorizontalFrame *PSDFilterPolarity_HF = new TGHorizontalFrame(PSDAnalysis_GF);
  PSDAnalysis_GF->AddFrame(PSDFilterPolarity_HF, new TGLayoutHints(kLHintsCenterX, 0,5,0,5));

  PSDFilterPolarity_HF->AddFrame(PSDPositiveFilter_RB = new TGRadioButton(PSDFilterPolarity_HF, "Positive  ", PSDPositiveFilter_RB_ID),
				 new TGLayoutHints(kLHintsNormal, 5,5,0,5));
  PSDPositiveFilter_RB->Connect("Clicked()", "AAInterface", this, "HandleRadioButtons()");
  PSDPositiveFilter_RB->SetState(kButtonDown);
  PSDPositiveFilter_RB->SetState(kButtonDisabled);

  PSDFilterPolarity_HF->AddFrame(PSDNegativeFilter_RB = new TGRadioButton(PSDFilterPolarity_HF, "Negative", PSDNegativeFilter_RB_ID),
				 new TGLayoutHints(kLHintsNormal, 5,5,0,5));
  PSDNegativeFilter_RB->Connect("Clicked()", "AAInterface", this, "HandleRadioButtons()");
  PSDNegativeFilter_RB->SetState(kButtonDisabled);

  
  PSDAnalysis_GF->AddFrame(PSDClearFilter_TB = new TGTextButton(PSDAnalysis_GF, "Clear filter", PSDClearFilter_TB_ID),
			   new TGLayoutHints(kLHintsLeft, 15,5,0,5));
  PSDClearFilter_TB->Resize(200,30);
  PSDClearFilter_TB->ChangeOptions(PSDClearFilter_TB->GetOptions() | kFixedSize);
  PSDClearFilter_TB->Connect("Clicked()", "AAInterface", this, "HandleTextButtons()");
  PSDClearFilter_TB->SetState(kButtonDisabled);
  
  PSDAnalysis_GF->AddFrame(PSDCalculate_TB = new TGTextButton(PSDAnalysis_GF, "Create PSD histogram", PSDCalculate_TB_ID),
                           new TGLayoutHints(kLHintsLeft, 15,5,5,5));
  PSDCalculate_TB->Resize(200,30);
  PSDCalculate_TB->SetBackgroundColor(ColorMgr->Number2Pixel(36));
  PSDCalculate_TB->SetForegroundColor(ColorMgr->Number2Pixel(0));
  PSDCalculate_TB->ChangeOptions(PSDCalculate_TB->GetOptions() | kFixedSize);
  PSDCalculate_TB->Connect("Clicked()", "AAInterface", this, "HandleTextButtons()");
  PSDCalculate_TB->SetState(kButtonDisabled);


  TGGroupFrame *PearsonAnalysis_GF = new TGGroupFrame(ProcessingFrame_VF, "Pearson (RFQ current) integration", kVerticalFrame);
  ProcessingFrame_VF->AddFrame(PearsonAnalysis_GF, new TGLayoutHints(kLHintsCenterX, 15,5,0,0));
  
  PearsonAnalysis_GF->AddFrame(IntegratePearson_CB = new TGCheckButton(PearsonAnalysis_GF, "Integrate signal", IntegratePearson_CB_ID),
			       new TGLayoutHints(kLHintsNormal, 0,5,5,0));
  IntegratePearson_CB->Connect("Clicked()", "AAInterface", this, "HandleCheckButtons()");
  
  PearsonAnalysis_GF->AddFrame(PearsonChannel_CBL = new ADAQComboBoxWithLabel(PearsonAnalysis_GF, "Contains signal", -1),
			       new TGLayoutHints(kLHintsNormal, 0,5,5,0));

  ss.str("");
  for(int ch=0; ch<NumDataChannels; ch++){
    ss << ch;
    entry.assign("Channel " + ss.str());
    PearsonChannel_CBL->GetComboBox()->AddEntry(entry.c_str(),ch);
    ss.str("");
  }
  PearsonChannel_CBL->GetComboBox()->Select(3);
  PearsonChannel_CBL->GetComboBox()->SetEnabled(false);

  TGHorizontalFrame *PearsonPolarity_HF = new TGHorizontalFrame(PearsonAnalysis_GF);
  PearsonAnalysis_GF->AddFrame(PearsonPolarity_HF, new TGLayoutHints(kLHintsNormal));

  PearsonPolarity_HF->AddFrame(PearsonPolarityPositive_RB = new TGRadioButton(PearsonPolarity_HF, "Positive", PearsonPolarityPositive_RB_ID),
			       new TGLayoutHints(kLHintsNormal, 0,5,0,0));
  PearsonPolarityPositive_RB->SetState(kButtonDown);
  PearsonPolarityPositive_RB->SetState(kButtonDisabled);
  PearsonPolarityPositive_RB->Connect("Clicked()", "AAInterface", this, "HandleRadioButtons()");

  PearsonPolarity_HF->AddFrame(PearsonPolarityNegative_RB = new TGRadioButton(PearsonPolarity_HF, "Negative", PearsonPolarityNegative_RB_ID),
			       new TGLayoutHints(kLHintsNormal, 0,5,0,0));
  PearsonPolarityNegative_RB->Connect("Clicked()", "AAInterface", this, "HandleRadioButtons()");

  TGHorizontalFrame *PearsonIntegrationType_HF = new TGHorizontalFrame(PearsonAnalysis_GF);
  PearsonAnalysis_GF->AddFrame(PearsonIntegrationType_HF, new TGLayoutHints(kLHintsNormal));

  PearsonIntegrationType_HF->AddFrame(IntegrateRawPearson_RB = new TGRadioButton(PearsonIntegrationType_HF, "Integrate raw", IntegrateRawPearson_RB_ID),
				      new TGLayoutHints(kLHintsLeft, 0,5,5,0));
  IntegrateRawPearson_RB->Connect("Clicked()", "AAInterface", this, "HandleRadioButtons()");
  IntegrateRawPearson_RB->SetState(kButtonDown);
  IntegrateRawPearson_RB->SetState(kButtonDisabled);

  PearsonIntegrationType_HF->AddFrame(IntegrateFitToPearson_RB = new TGRadioButton(PearsonIntegrationType_HF, "Integrate fit", IntegrateFitToPearson_RB_ID),
				      new TGLayoutHints(kLHintsLeft, 10,5,5,0));
  IntegrateFitToPearson_RB->Connect("Clicked()", "AAInterface", this, "HandleRadioButtons()");
  IntegrateFitToPearson_RB->SetState(kButtonDisabled);
  
  PearsonAnalysis_GF->AddFrame(PearsonLowerLimit_NEL = new ADAQNumberEntryWithLabel(PearsonAnalysis_GF, "Lower limit (sample)", PearsonLowerLimit_NEL_ID),
			       new TGLayoutHints(kLHintsNormal, 0,5,0,0));
  PearsonLowerLimit_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  PearsonLowerLimit_NEL->GetEntry()->SetNumLimits(TGNumberFormat::kNELLimitMinMax);
  PearsonLowerLimit_NEL->GetEntry()->SetLimitValues(0,1); // Updated when ADAQ ROOT file loaded
  PearsonLowerLimit_NEL->GetEntry()->SetNumber(0); // Updated when ADAQ ROOT file loaded
  PearsonLowerLimit_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");
  PearsonLowerLimit_NEL->GetEntry()->SetState(false);
  
  PearsonAnalysis_GF->AddFrame(PearsonMiddleLimit_NEL = new ADAQNumberEntryWithLabel(PearsonAnalysis_GF, "Middle limit (sample)", PearsonMiddleLimit_NEL_ID),
			       new TGLayoutHints(kLHintsNormal, 0,5,0,0));
  PearsonMiddleLimit_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  PearsonMiddleLimit_NEL->GetEntry()->SetNumLimits(TGNumberFormat::kNELLimitMinMax);
  PearsonMiddleLimit_NEL->GetEntry()->SetLimitValues(0,1); // Updated when ADAQ ROOT file loaded
  PearsonMiddleLimit_NEL->GetEntry()->SetNumber(0); // Updated when ADAQ ROOT file loaded
  PearsonMiddleLimit_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");
  PearsonMiddleLimit_NEL->GetEntry()->SetState(false);
  
  PearsonAnalysis_GF->AddFrame(PearsonUpperLimit_NEL = new ADAQNumberEntryWithLabel(PearsonAnalysis_GF, "Upper limit (sample))", PearsonUpperLimit_NEL_ID),
			       new TGLayoutHints(kLHintsNormal, 0,5,0,0));
  PearsonUpperLimit_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  PearsonUpperLimit_NEL->GetEntry()->SetNumLimits(TGNumberFormat::kNELLimitMinMax);
  PearsonUpperLimit_NEL->GetEntry()->SetLimitValues(0,1); // Updated when ADAQ ROOT file loaded
  PearsonUpperLimit_NEL->GetEntry()->SetNumber(0); // Updated when ADAQ ROOT file loaded
  PearsonUpperLimit_NEL->GetEntry()->Connect("ValueSet(long)", "AAInterface", this, "HandleNumberEntries()");
  PearsonUpperLimit_NEL->GetEntry()->SetState(false);
  
  PearsonAnalysis_GF->AddFrame(PlotPearsonIntegration_CB = new TGCheckButton(PearsonAnalysis_GF, "Plot integration", PlotPearsonIntegration_CB_ID),
			       new TGLayoutHints(kLHintsNormal, 0,5,5,0));
  PlotPearsonIntegration_CB->SetState(kButtonDisabled);
  PlotPearsonIntegration_CB->Connect("Clicked()", "AAInterface", this, "HandleCheckButtons()");

  PearsonAnalysis_GF->AddFrame(DeuteronsInWaveform_NEFL = new ADAQNumberEntryFieldWithLabel(PearsonAnalysis_GF, "D+ in waveform", -1),
			       new TGLayoutHints(kLHintsNormal, 0,5,0,0));
  DeuteronsInWaveform_NEFL->GetEntry()->SetFormat(TGNumberFormat::kNESReal, TGNumberFormat::kNEANonNegative);
  DeuteronsInWaveform_NEFL->GetEntry()->Resize(100,20);
  DeuteronsInWaveform_NEFL->GetEntry()->SetState(false);
  
  PearsonAnalysis_GF->AddFrame(DeuteronsInTotal_NEFL = new ADAQNumberEntryFieldWithLabel(PearsonAnalysis_GF, "D+ in total", -1),
			       new TGLayoutHints(kLHintsNormal, 0,5,0,5));
  DeuteronsInTotal_NEFL->GetEntry()->SetFormat(TGNumberFormat::kNESReal, TGNumberFormat::kNEANonNegative);
  DeuteronsInTotal_NEFL->GetEntry()->Resize(100,20);
  DeuteronsInTotal_NEFL->GetEntry()->SetState(false);
  
  // Despliced file creation options
  
  TGGroupFrame *WaveformDesplicer_GF = new TGGroupFrame(ProcessingFrame_VF, "Despliced file creation", kVerticalFrame);
  ProcessingFrame_VF->AddFrame(WaveformDesplicer_GF, new TGLayoutHints(kLHintsCenterX, 5,5,5,5));
  
  WaveformDesplicer_GF->AddFrame(DesplicedWaveformNumber_NEL = new ADAQNumberEntryWithLabel(WaveformDesplicer_GF, "Number (Waveforms)", -1),
				 new TGLayoutHints(kLHintsLeft, 0,5,5,0));
  DesplicedWaveformNumber_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  DesplicedWaveformNumber_NEL->GetEntry()->SetNumLimits(TGNumberFormat::kNELLimitMinMax);
  DesplicedWaveformNumber_NEL->GetEntry()->SetLimitValues(0,1); // Updated when ADAQ ROOT file loaded
  DesplicedWaveformNumber_NEL->GetEntry()->SetNumber(0); // Updated when ADAQ ROOT file loaded
  
  WaveformDesplicer_GF->AddFrame(DesplicedWaveformBuffer_NEL = new ADAQNumberEntryWithLabel(WaveformDesplicer_GF, "Buffer size (sample)", -1),
				 new TGLayoutHints(kLHintsLeft, 0,5,0,0));
  DesplicedWaveformBuffer_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  DesplicedWaveformBuffer_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  DesplicedWaveformBuffer_NEL->GetEntry()->SetNumber(100);
  
  WaveformDesplicer_GF->AddFrame(DesplicedWaveformLength_NEL = new ADAQNumberEntryWithLabel(WaveformDesplicer_GF, "Despliced length (sample)", -1),
				 new TGLayoutHints(kLHintsLeft, 0,5,0,0));
  DesplicedWaveformLength_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  DesplicedWaveformLength_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  DesplicedWaveformLength_NEL->GetEntry()->SetNumber(512);
  
  TGHorizontalFrame *WaveformDesplicerName_HF = new TGHorizontalFrame(WaveformDesplicer_GF);
  WaveformDesplicer_GF->AddFrame(WaveformDesplicerName_HF, new TGLayoutHints(kLHintsLeft, 0,0,0,0));
  
  WaveformDesplicerName_HF->AddFrame(DesplicedFileSelection_TB = new TGTextButton(WaveformDesplicerName_HF, "File ... ", DesplicedFileSelection_TB_ID),
				     new TGLayoutHints(kLHintsLeft, 0,5,5,0));
  DesplicedFileSelection_TB->Resize(60,25);
  DesplicedFileSelection_TB->SetBackgroundColor(ThemeForegroundColor);
  DesplicedFileSelection_TB->ChangeOptions(DesplicedFileSelection_TB->GetOptions() | kFixedSize);
  DesplicedFileSelection_TB->Connect("Clicked()", "AAInterface", this, "HandleTextButtons()");
  
  WaveformDesplicerName_HF->AddFrame(DesplicedFileName_TE = new TGTextEntry(WaveformDesplicerName_HF, "<No file currently selected>", -1),
				     new TGLayoutHints(kLHintsLeft, 5,0,5,5));
  DesplicedFileName_TE->Resize(180,25);
  DesplicedFileName_TE->SetAlignment(kTextRight);
  DesplicedFileName_TE->SetBackgroundColor(ThemeForegroundColor);
  DesplicedFileName_TE->ChangeOptions(DesplicedFileName_TE->GetOptions() | kFixedSize);
  
  WaveformDesplicer_GF->AddFrame(DesplicedFileCreation_TB = new TGTextButton(WaveformDesplicer_GF, "Create despliced file", DesplicedFileCreation_TB_ID),
				 new TGLayoutHints(kLHintsLeft, 20,0,10,5));
  DesplicedFileCreation_TB->Resize(200, 30);
  DesplicedFileCreation_TB->SetBackgroundColor(ColorMgr->Number2Pixel(36));
  DesplicedFileCreation_TB->SetForegroundColor(ColorMgr->Number2Pixel(0));
  DesplicedFileCreation_TB->ChangeOptions(DesplicedFileCreation_TB->GetOptions() | kFixedSize);
  DesplicedFileCreation_TB->Connect("Clicked()", "AAInterface", this, "HandleTextButtons()");


  // Count rate

  TGGroupFrame *CountRate_GF = new TGGroupFrame(ProcessingFrame_VF, "Count rate", kVerticalFrame);
  ProcessingFrame_VF->AddFrame(CountRate_GF, new TGLayoutHints(kLHintsNormal, 15,5,5,5));

  CountRate_GF->AddFrame(RFQPulseWidth_NEL = new ADAQNumberEntryWithLabel(CountRate_GF, "RFQ pulse width [us]", -1),
			 new TGLayoutHints(kLHintsNormal, 5,5,5,0));
  RFQPulseWidth_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESReal);
  RFQPulseWidth_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  RFQPulseWidth_NEL->GetEntry()->SetNumber(50);
  RFQPulseWidth_NEL->GetEntry()->Resize(70,20);

  CountRate_GF->AddFrame(RFQRepRate_NEL = new ADAQNumberEntryWithLabel(CountRate_GF, "RFQ rep. rate [Hz]", -1),
			 new TGLayoutHints(kLHintsNormal, 5,5,0,0));
  RFQRepRate_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  RFQRepRate_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  RFQRepRate_NEL->GetEntry()->SetNumber(30);
  RFQRepRate_NEL->GetEntry()->Resize(70,20);

  CountRate_GF->AddFrame(CountRateWaveforms_NEL = new ADAQNumberEntryWithLabel(CountRate_GF, "Waveforms to use", -1),
                         new TGLayoutHints(kLHintsNormal, 5,5,0,0));
  CountRateWaveforms_NEL->GetEntry()->SetNumStyle(TGNumberFormat::kNESInteger);
  CountRateWaveforms_NEL->GetEntry()->SetNumAttr(TGNumberFormat::kNEAPositive);
  CountRateWaveforms_NEL->GetEntry()->SetNumber(1000);
  CountRateWaveforms_NEL->GetEntry()->Resize(70,20);

  CountRate_GF->AddFrame(CalculateCountRate_TB = new TGTextButton(CountRate_GF, "Calculate count rate", CountRate_TB_ID),
                         new TGLayoutHints(kLHintsNormal, 20,5,5,15));
  CalculateCountRate_TB->SetBackgroundColor(ColorMgr->Number2Pixel(36));
  CalculateCountRate_TB->SetForegroundColor(ColorMgr->Number2Pixel(0));
  CalculateCountRate_TB->Resize(200,30);
  CalculateCountRate_TB->ChangeOptions(CalculateCountRate_TB->GetOptions() | kFixedSize);
  CalculateCountRate_TB->Connect("Clicked()", "AAInterface", this, "HandleTextButtons()");

  CountRate_GF->AddFrame(InstCountRate_NEFL = new ADAQNumberEntryFieldWithLabel(CountRate_GF, "Inst. count rate [1/s]", -1),
                         new TGLayoutHints(kLHintsNormal, 5,5,5,0));
  InstCountRate_NEFL->GetEntry()->Resize(100,20);
  InstCountRate_NEFL->GetEntry()->SetState(kButtonDisabled);
  InstCountRate_NEFL->GetEntry()->SetAlignment(kTextCenterX);
  InstCountRate_NEFL->GetEntry()->SetBackgroundColor(ColorMgr->Number2Pixel(19));
  InstCountRate_NEFL->GetEntry()->ChangeOptions(InstCountRate_NEFL->GetEntry()->GetOptions() | kFixedSize);

  CountRate_GF->AddFrame(AvgCountRate_NEFL = new ADAQNumberEntryFieldWithLabel(CountRate_GF, "Avg. count rate [1/s]", -1),
                         new TGLayoutHints(kLHintsNormal, 5,5,0,5));
  AvgCountRate_NEFL->GetEntry()->Resize(100,20);
  AvgCountRate_NEFL->GetEntry()->SetState(kButtonDisabled);
  AvgCountRate_NEFL->GetEntry()->SetAlignment(kTextCenterX);
  AvgCountRate_NEFL->GetEntry()->SetBackgroundColor(ColorMgr->Number2Pixel(19));
  AvgCountRate_NEFL->GetEntry()->ChangeOptions(AvgCountRate_NEFL->GetEntry()->GetOptions() | kFixedSize);
}


void AAInterface::FillCanvasFrame()
{
  /////////////////////////////////////
  // Canvas and associated widget frame

  TGVerticalFrame *Canvas_VF = new TGVerticalFrame(OptionsAndCanvas_HF);
  Canvas_VF->SetBackgroundColor(ThemeBackgroundColor);
  OptionsAndCanvas_HF->AddFrame(Canvas_VF, new TGLayoutHints(kLHintsNormal, 5,5,5,5));

  // ADAQ ROOT file name text entry
  Canvas_VF->AddFrame(FileName_TE = new TGTextEntry(Canvas_VF, "<No file currently selected>", -1),
		      new TGLayoutHints(kLHintsTop | kLHintsCenterX, 30,5,0,5));
  FileName_TE->Resize(CanvasFrameWidth,20);
  FileName_TE->SetState(false);
  FileName_TE->SetAlignment(kTextCenterX);
  FileName_TE->SetBackgroundColor(ThemeForegroundColor);
  FileName_TE->ChangeOptions(FileName_TE->GetOptions() | kFixedSize);

  // File statistics horizontal frame

  TGHorizontalFrame *FileStats_HF = new TGHorizontalFrame(Canvas_VF,CanvasFrameWidth,30);
  FileStats_HF->SetBackgroundColor(ThemeBackgroundColor);
  Canvas_VF->AddFrame(FileStats_HF, new TGLayoutHints(kLHintsTop | kLHintsCenterX, 5,5,0,5));

  // Waveforms

  TGHorizontalFrame *WaveformNELAndLabel_HF = new TGHorizontalFrame(FileStats_HF);
  WaveformNELAndLabel_HF->SetBackgroundColor(ThemeBackgroundColor);
  FileStats_HF->AddFrame(WaveformNELAndLabel_HF, new TGLayoutHints(kLHintsLeft, 5,35,0,0));

  WaveformNELAndLabel_HF->AddFrame(Waveforms_NEL = new TGNumberEntryField(WaveformNELAndLabel_HF,-1),
				  new TGLayoutHints(kLHintsLeft,5,5,0,0));
  Waveforms_NEL->SetBackgroundColor(ThemeForegroundColor);
  Waveforms_NEL->SetAlignment(kTextCenterX);
  Waveforms_NEL->SetState(false);
  
  WaveformNELAndLabel_HF->AddFrame(WaveformLabel_L = new TGLabel(WaveformNELAndLabel_HF, " Waveforms "),
				   new TGLayoutHints(kLHintsLeft,0,5,3,0));
  WaveformLabel_L->SetBackgroundColor(ThemeForegroundColor);

  // Record length

  TGHorizontalFrame *RecordLengthNELAndLabel_HF = new TGHorizontalFrame(FileStats_HF);
  RecordLengthNELAndLabel_HF->SetBackgroundColor(ThemeBackgroundColor);
  FileStats_HF->AddFrame(RecordLengthNELAndLabel_HF, new TGLayoutHints(kLHintsLeft, 5,5,0,0));

  RecordLengthNELAndLabel_HF->AddFrame(RecordLength_NEL = new TGNumberEntryField(RecordLengthNELAndLabel_HF,-1),
				      new TGLayoutHints(kLHintsLeft,5,5,0,0));
  RecordLength_NEL->SetBackgroundColor(ThemeForegroundColor);
  RecordLength_NEL->SetAlignment(kTextCenterX);
  RecordLength_NEL->SetState(false);

  RecordLengthNELAndLabel_HF->AddFrame(RecordLengthLabel_L = new TGLabel(RecordLengthNELAndLabel_HF, " Record length "),
				       new TGLayoutHints(kLHintsLeft,0,5,3,0));
  RecordLengthLabel_L->SetBackgroundColor(ThemeForegroundColor);

  // The embedded canvas

  TGHorizontalFrame *Canvas_HF = new TGHorizontalFrame(Canvas_VF);
  Canvas_VF->AddFrame(Canvas_HF, new TGLayoutHints(kLHintsCenterX));

  Canvas_HF->AddFrame(YAxisLimits_DVS = new TGDoubleVSlider(Canvas_HF, CanvasY, kDoubleScaleBoth, YAxisLimits_DVS_ID),
		      new TGLayoutHints(kLHintsCenterY));
  YAxisLimits_DVS->SetRange(0,1);
  YAxisLimits_DVS->SetPosition(0,1);
  YAxisLimits_DVS->Connect("PositionChanged()", "AAInterface", this, "HandleDoubleSliders()");
  
  Canvas_HF->AddFrame(Canvas_EC = new TRootEmbeddedCanvas("TheCanvas_EC", Canvas_HF, CanvasX, CanvasY),
		      new TGLayoutHints(kLHintsCenterX, 5,5,5,5));
  Canvas_EC->GetCanvas()->SetFillColor(0);
  Canvas_EC->GetCanvas()->SetFrameFillColor(0);
  Canvas_EC->GetCanvas()->SetGrid();
  Canvas_EC->GetCanvas()->SetBorderMode(0);
  Canvas_EC->GetCanvas()->SetLeftMargin(0.13);
  Canvas_EC->GetCanvas()->SetBottomMargin(0.12);
  Canvas_EC->GetCanvas()->SetRightMargin(0.05);
  Canvas_EC->GetCanvas()->Connect("ProcessedEvent(int, int, int, TObject *)", "AAInterface", this, "HandleCanvas(int, int, int, TObject *)");

  Canvas_VF->AddFrame(XAxisLimits_THS = new TGTripleHSlider(Canvas_VF, CanvasX, kDoubleScaleBoth, XAxisLimits_THS_ID),
		      new TGLayoutHints(kLHintsNormal, SliderBuffer,5,5,0));
  XAxisLimits_THS->SetRange(0,1);
  XAxisLimits_THS->SetPosition(0,1);
  XAxisLimits_THS->SetPointerPosition(0.5);
  XAxisLimits_THS->Connect("PositionChanged()", "AAInterface", this, "HandleDoubleSliders()");
  XAxisLimits_THS->Connect("PointerPositionChanged()", "AAInterface", this, "HandleTripleSliderPointer()");
    
  Canvas_VF->AddFrame(new TGLabel(Canvas_VF, "  Waveform selector  "),
		      new TGLayoutHints(kLHintsCenterX, 5,5,10,0));

  Canvas_VF->AddFrame(WaveformSelector_HS = new TGHSlider(Canvas_VF, CanvasX, kSlider1),
		      new TGLayoutHints(kLHintsNormal, SliderBuffer,5,0,0));
  WaveformSelector_HS->SetRange(1,100);
  WaveformSelector_HS->SetPosition(1);
  WaveformSelector_HS->Connect("PositionChanged(int)", "AAInterface", this, "HandleSliders(int)");

  Canvas_VF->AddFrame(new TGLabel(Canvas_VF, "  Spectrum integration limits  "),
		      new TGLayoutHints(kLHintsCenterX, 5,5,10,0));

  Canvas_VF->AddFrame(SpectrumIntegrationLimits_DHS = new TGDoubleHSlider(Canvas_VF, CanvasX, kDoubleScaleBoth, SpectrumIntegrationLimits_DHS_ID),
		      new TGLayoutHints(kLHintsNormal, SliderBuffer,5,0,5));
  SpectrumIntegrationLimits_DHS->SetRange(0,1);
  SpectrumIntegrationLimits_DHS->SetPosition(0,1);
  SpectrumIntegrationLimits_DHS->Connect("PositionChanged()", "AAInterface", this, "HandleDoubleSliders()");
		      
  TGHorizontalFrame *SubCanvas_HF = new TGHorizontalFrame(Canvas_VF);
  SubCanvas_HF->SetBackgroundColor(ThemeBackgroundColor);
  Canvas_VF->AddFrame(SubCanvas_HF, new TGLayoutHints(kLHintsRight | kLHintsBottom, 5,30,20,5));
  
  SubCanvas_HF->AddFrame(ProcessingProgress_PB = new TGHProgressBar(SubCanvas_HF, TGProgressBar::kFancy, CanvasX-300),
			 new TGLayoutHints(kLHintsLeft, 5,55,7,5));
  ProcessingProgress_PB->SetBarColor(ColorMgr->Number2Pixel(29));
  ProcessingProgress_PB->ShowPosition(kTRUE, kFALSE, "%0.f% waveforms processed");
  
  SubCanvas_HF->AddFrame(Quit_TB = new TGTextButton(SubCanvas_HF, "Access standby", Quit_TB_ID),
			 new TGLayoutHints(kLHintsRight, 5,5,0,5));
  Quit_TB->Resize(200, 40);
  Quit_TB->SetBackgroundColor(ColorMgr->Number2Pixel(36));
  Quit_TB->SetForegroundColor(ColorMgr->Number2Pixel(0));
  Quit_TB->ChangeOptions(Quit_TB->GetOptions() | kFixedSize);
  Quit_TB->Connect("Clicked()", "AAInterface", this, "HandleTerminate()");
}


void AAInterface::HandleMenu(int MenuID)
{
  switch(MenuID){
    
    // Action that enables the user to select a ROOT file with
    // waveforms using the nicely prebuilt ROOT interface for file
    // selection. Only ROOT files are displayed in the dialog.
  case MenuFileOpenADAQ_ID:
  case MenuFileOpenACRONYM_ID:{

    string Desc[2], Type[2];
    if(MenuFileOpenADAQ_ID == MenuID){
      Desc[0] = "ADAQ ROOT file";
      Type[0] = "*.adaq";

      Desc[1] = "ADAQ ROOT file";
      Type[1] = "*.root";

    }
    else if(MenuFileOpenACRONYM_ID == MenuID){
      Desc[0] = "ACRONYM ROOT file";
      Type[0] = "*.acro";
      
      Desc[1] = "ACRONYM ROOT file";
      Type[1] = "*.root";
    }
    
    const char *FileTypes[] = {Desc[0].c_str(), Type[0].c_str(),
			       Desc[1].c_str(), Type[1].c_str(),
			       "All files",                    "*",
			       0,                              0};
    
    // Use the ROOT prebuilt file dialog machinery
    TGFileInfo FileInformation;
    FileInformation.fFileTypes = FileTypes;
    FileInformation.fIniDir = StrDup(DataDirectory.c_str());
    new TGFileDialog(gClient->GetRoot(), this, kFDOpen, &FileInformation);
    
    // If the selected file is not found...
    if(FileInformation.fFilename==NULL)
      CreateMessageBox("No valid ROOT file was selected so there's nothing to load!\nPlease select a valid file!","Stop");
    else{
      // Note that the FileInformation.fFilename variable storeds the
      // absolute path to the data file selected by the user
      string FileName = FileInformation.fFilename;
      
      // Strip the data file name off the absolute file path and set
      // the path to the DataDirectory variable. Thus, the "current"
      // directory from which a file was selected will become the new
      // default directory that automatically open
      size_t pos = FileName.find_last_of("/");
      if(pos != string::npos)
	DataDirectory  = FileName.substr(0,pos);
      
      // Load the ROOT file and set the bool depending on outcome
      if(MenuID == MenuFileOpenADAQ_ID){
	ADAQFileName = FileName;
	ADAQFileLoaded = ComputationMgr->LoadADAQRootFile(FileName);
	
	if(ADAQFileLoaded)
	  UpdateForADAQFile();
	else
	  CreateMessageBox("The ADAQ ROOT file that you specified fail to load for some reason!\n","Stop");
	
	ACRONYMFileLoaded = false;
      }
      else if(MenuID == MenuFileOpenACRONYM_ID){
	ACRONYMFileName = FileName;
	ACRONYMFileLoaded = ComputationMgr->LoadACRONYMRootFile(FileName);
	
	if(ACRONYMFileLoaded)
	  UpdateForACRONYMFile();
	else
	  CreateMessageBox("The ACRONYM ROOT file that you specified fail to load for some reason!\n","Stop");
	
	ADAQFileLoaded = false;
      }
    }
    break;
  }

  case MenuFileSaveWaveform_ID:
  case MenuFileSaveSpectrum_ID:
  case MenuFileSaveSpectrumBackground_ID:
  case MenuFileSaveSpectrumDerivative_ID:
  case MenuFileSavePSDHistogram_ID:
  case MenuFileSavePSDHistogramSlice_ID:{

    // Create character arrays that enable file type selection (.dat
    // files have data columns separated by spaces and .csv have data
    // columns separated by commas)
    const char *FileTypes[] = {"ASCII file",  "*.dat",
			       "CSV file",    "*.csv",
			       "ROOT file",   "*.root",
			       0,             0};
  
    TGFileInfo FileInformation;
    FileInformation.fFileTypeIdx = 4;
    FileInformation.fFileTypes = FileTypes;
    FileInformation.fIniDir = StrDup(HistogramDirectory.c_str());
    
    new TGFileDialog(gClient->GetRoot(), this, kFDSave, &FileInformation);
    
    if(FileInformation.fFilename==NULL)
      CreateMessageBox("No file was selected! Nothing will be saved to file!","Stop");
    else{
      string FileName, FileExtension;
      size_t Found = string::npos;
      
      // Get the file name for the output histogram data. Note that
      // FileInformation.fFilename is the absolute path to the file.
      FileName = FileInformation.fFilename;

      // Strip the data file name off the absolute file path and set
      // the path to the DataDirectory variable. Thus, the "current"
      // directory from which a file was selected will become the new
      // default directory that automically opens
      size_t pos = FileName.find_last_of("/");
      if(pos != string::npos)
	HistogramDirectory  = FileName.substr(0,pos);

      // Strip the file extension (the start of the file extension is
      // assumed here to begin with the final period) to extract just
      // the save file name.
      Found = FileName.find_last_of(".");
      if(Found != string::npos)
	FileName = FileName.substr(0, Found);

      // Extract only the "." with the file extension. Note that anove
      // the "*" character precedes the file extension string when
      // passed to the FileInformation class in order for files
      // containing that expression to be displaced to the
      // user. However, I strip the "*" such that the "." plus file
      // extension can be used by the SaveSpectrumData() function to
      // determine the format of spectrum save file.
      FileExtension = FileInformation.fFileTypes[FileInformation.fFileTypeIdx+1];
      Found = FileExtension.find_last_of("*");
      
      if(Found != string::npos)
	FileExtension = FileExtension.substr(Found+1, FileExtension.size());
      
      bool Success = false;
      
      if(MenuID == MenuFileSaveWaveform_ID){
	Success = ComputationMgr->SaveHistogramData("Waveform", FileName, FileExtension);
      }
      
      else if(MenuID == MenuFileSaveSpectrum_ID){
	if(!ComputationMgr->GetSpectrumExists()){
	  CreateMessageBox("No spectra have been created yet and, therefore, there is nothing to save!","Stop");
	  break;
	}
	else
	  Success = ComputationMgr->SaveHistogramData("Spectrum", FileName, FileExtension);
      }
      
      else if(MenuID == MenuFileSaveSpectrumBackground_ID){
	if(!ComputationMgr->GetSpectrumBackgroundExists()){
	  CreateMessageBox("No spectrum derivatives have been created yet and, therefore, there is nothing to save!","Stop");
	  break;
	}
	else
	  Success = ComputationMgr->SaveHistogramData("SpectrumBackground", FileName, FileExtension);
      }

      else if(MenuID == MenuFileSaveSpectrumDerivative_ID){
	if(!ComputationMgr->GetSpectrumDerivativeExists()){
	  CreateMessageBox("No spectrum derivatives have been created yet and, therefore, there is nothing to save!","Stop");
	  break;
	}
	else
	  Success = ComputationMgr->SaveHistogramData("SpectrumDerivative", FileName, FileExtension);
      }

      else if(MenuID == MenuFileSavePSDHistogram_ID){
	if(!ComputationMgr->GetPSDHistogramExists()){
	  CreateMessageBox("A PSD histogram has not been created yet and, therefore, there is nothing to save!","Stop");
	  break;
	}

	if(FileExtension != ".root"){
	  CreateMessageBox("PSD histograms may only be saved to a ROOT TFile (*.root). Please reselect file type!","Stop");
	  break;
	}

	Success = ComputationMgr->SaveHistogramData("PSDHistogram", FileName, FileExtension);
      }
      
      else if(MenuID == MenuFileSavePSDHistogramSlice_ID){
	if(!ComputationMgr->GetPSDHistogramSliceExists()){
	  CreateMessageBox("A PSD histogram slice has not been created yet and, therefore, there is nothing to save!","Stop");
	  break;
	}
	else
	  Success = ComputationMgr->SaveHistogramData("PSDHistogramSlice", FileName, FileExtension);
      }
      
      if(Success){
	if(FileExtension == ".dat")
	  CreateMessageBox("The histogram was successfully saved to the .dat file","Asterisk");
	else if(FileExtension == ".csv")
	  CreateMessageBox("The histogram was successfully saved to the .csv file","Asterisk");
	else if(FileExtension == ".root")
	  CreateMessageBox("The histogram (named 'Waveform','Spectrum',or 'PSDHistogram') \nwas successfully saved to the .root file!\n","Asterisk");
      }
      else
	CreateMessageBox("The histogram failed to save to the file for unknown reasons!","Stop");
    }
    break;
  }

  case MenuFileSaveSpectrumCalibration_ID:{
    
    const char *FileTypes[] = {"ADAQ calibration file", "*.acal",
			       "All files"            , "*.*",
			       0, 0};
    
    TGFileInfo FileInformation;
    FileInformation.fFileTypes = FileTypes;
    FileInformation.fIniDir = StrDup(getenv("PWD"));

    new TGFileDialog(gClient->GetRoot(), this, kFDSave, &FileInformation);

    if(FileInformation.fFilename==NULL)
      CreateMessageBox("No file was selected and, therefore, nothing will be saved!","Stop");
    else{
      
      string FName = FileInformation.fFilename;

      size_t Found = FName.find_last_of(".acal");
      if(Found == string::npos)
	FName += ".acal";

      const int Channel = ChannelSelector_CBL->GetComboBox()->GetSelected();
      bool Success = ComputationMgr->WriteCalibrationFile(Channel, FName);
      if(Success)
	CreateMessageBox("The calibration file was successfully written to file.","Asterisk");
      else
	CreateMessageBox("There was an unknown error in writing the calibration file!","Stop");
    }
    break;
  }
    
    // Acition that enables the user to print the currently displayed
    // canvas to a file of the user's choice. But really, it's not a
    // choice. Vector-based graphics are the only way to go. Do
    // everyone a favor, use .eps or .ps or .pdf if you want to be a
    // serious scientist.
  case MenuFilePrint_ID:{
    
    // List the available graphical file options
    const char *FileTypes[] = {"EPS file",          "*.eps",
			       "JPG file",          "*.jpeg",
			       "PS file",           "*.ps",
			       "PDF file",          "*.pdf",
			       "PNG file",          "*.png",
			       0,                  0 };

    // Use the ROOT prebuilt file dialog machinery
    TGFileInfo FileInformation;
    FileInformation.fFileTypes = FileTypes;
    FileInformation.fIniDir = StrDup(PrintDirectory.c_str());

    new TGFileDialog(gClient->GetRoot(), this, kFDSave, &FileInformation);
    
    if(FileInformation.fFilename==NULL)
      CreateMessageBox("No file was selected to the canvas graphics will not be saved!\nSelect a valid file to save the canvas graphis!","Stop");
    else{

      string GraphicFileName, GraphicFileExtension;

      size_t Found = string::npos;
      
      GraphicFileName = FileInformation.fFilename;

      // Strip the data file name off the absolute file path and set
      // the path to the DataDirectory variable. Thus, the "current"
      // directory from which a file was selected will become the new
      // default directory that automically opens
      Found = GraphicFileName.find_last_of("/");
      if(Found != string::npos)
	PrintDirectory  = GraphicFileName.substr(0,Found);
      
      Found = GraphicFileName.find_last_of(".");
      if(Found != string::npos)
	GraphicFileName = GraphicFileName.substr(0, Found);
      
      GraphicFileExtension = FileInformation.fFileTypes[FileInformation.fFileTypeIdx+1];
      Found = GraphicFileExtension.find_last_of("*");
      if(Found != string::npos)
	GraphicFileExtension = GraphicFileExtension.substr(Found+1, GraphicFileExtension.size());
      
      string GraphicFile = GraphicFileName+GraphicFileExtension;
      
      Canvas_EC->GetCanvas()->Update();
      Canvas_EC->GetCanvas()->Print(GraphicFile.c_str(), GraphicFileExtension.c_str());
      
      string SuccessMessage = "The canvas graphics have been successfully saved to the following file:\n" + GraphicFileName + GraphicFileExtension;
      CreateMessageBox(SuccessMessage,"Asterisk");
    }
    break;
  }
    
    // Action that enables the Quit_TB and File->Exit selections to
    // quit the ROOT application
  case MenuFileExit_ID:
    HandleTerminate();
    
  default:
    break;
  }
}


void AAInterface::HandleTextButtons()
{
  if(!ADAQFileLoaded and !ACRONYMFileLoaded)
    return;

  TGTextButton *ActiveTextButton = (TGTextButton *) gTQSender;
  int ActiveTextButtonID  = ActiveTextButton->WidgetId();
  
  SaveSettings();

  switch(ActiveTextButtonID){

    ////////////////////
    // Analysis frame //
    ////////////////////

    /////////////////////////////
    // Pulse shape discrimination
    
  case PSDCalculate_TB_ID:

    if(ADAQFileLoaded){
      // Sequential processing
      if(ProcessingSeq_RB->IsDown())
	ComputationMgr->CreatePSDHistogram();
      
      // Parallel processing
      else{
	SaveSettings(true);
	ComputationMgr->ProcessWaveformsInParallel("discriminating");
      }
      GraphicsMgr->PlotPSDHistogram();
    }
    else if(ACRONYMFileLoaded)
      CreateMessageBox("ACRONYM files cannot be processed for pulse shape at this time!","Stop");
    
    break;

  case PSDClearFilter_TB_ID:

    if(ADAQFileLoaded){

      ComputationMgr->ClearPSDFilter(ChannelSelector_CBL->GetComboBox()->GetSelected());
      PSDEnableFilter_CB->SetState(kButtonUp);
      
      switch(GraphicsMgr->GetCanvasContentType()){
      case zWaveform:
	GraphicsMgr->PlotWaveform();
	break;
	
      case zSpectrum:
	GraphicsMgr->PlotSpectrum();
	break;
	
      case zPSDHistogram:
	GraphicsMgr->PlotPSDHistogram();
	break;
      }
    }
    
    break;


    //////////////
    // Count rate

  case CountRate_TB_ID:
    if(ADAQFileLoaded)
      ComputationMgr->CalculateCountRate();
    break;



    ////////////////////
    // Graphics frame //
    ////////////////////
    
  case ReplotWaveform_TB_ID:
    GraphicsMgr->PlotWaveform();
    break;
    
  case ReplotSpectrum_TB_ID:
    if(ComputationMgr->GetSpectrumExists())
      GraphicsMgr->PlotSpectrum();
    else
      CreateMessageBox("A valid spectrum does not yet exist; therefore, it is difficult to replot it!","Stop");
    break;
    
  case ReplotSpectrumDerivative_TB_ID:
    if(ComputationMgr->GetSpectrumExists()){
      //SpectrumOverplotDerivative_CB->SetState(kButtonUp);
      GraphicsMgr->PlotSpectrumDerivative();
    }
    else
      CreateMessageBox("A valid spectrum does not yet exist; therefore, the spectrum derivative cannot be plotted!","Stop");
    break;
    
  case ReplotPSDHistogram_TB_ID:
    if(ADAQFileLoaded){
      if(ComputationMgr->GetPSDHistogramExists())
	GraphicsMgr->PlotPSDHistogram();
      else
CreateMessageBox("A valid PSD histogram does not yet exist; therefore, replotting cannot be achieved!","Stop");
    }
    break;

    
    //////////////////////
    // Processing frame //
    //////////////////////
    
  case DesplicedFileSelection_TB_ID:{

    const char *FileTypes[] = {"ADAQ-formatted ROOT file", "*.root",
			       "All files",                "*",
			       0, 0};
    
    // Use the presently open ADAQ ROOT file name as the basis for the
    // default despliced ADAQ ROOT file name presented to the user in
    // the TGFileDialog. I have begun to denote standard ADAQ ROOT
    // files with ".root" extension and despliced ADAQ ROOT files with
    // ".ds.root" extension. 
    string InitialFileName;
    size_t Pos = ADAQFileName.find_last_of("/");
    if(Pos != string::npos){
      string RawFileName = ADAQFileName.substr(Pos+1, ADAQFileName.size());

      Pos = RawFileName.find_last_of(".");
      if(Pos != string::npos)
	InitialFileName = RawFileName.substr(0,Pos) + ".ds.root";
    }

    TGFileInfo FileInformation;
    FileInformation.fFileTypes = FileTypes;
    FileInformation.fFilename = StrDup(InitialFileName.c_str());
    FileInformation.fIniDir = StrDup(DesplicedDirectory.c_str());
    new TGFileDialog(gClient->GetRoot(), this, kFDOpen, &FileInformation);
    
    if(FileInformation.fFilename==NULL)
      CreateMessageBox("A file was not selected so the data will not be saved!\nSelect a valid file to save the despliced waveforms","Stop");
    else{
      string DesplicedFileName, DesplicedFileExtension;
      size_t Found = string::npos;
      
      // Get the name for the despliced file. Note that
      // FileInformation.fFilename contains the absolute path to the
      // despliced file location
      DesplicedFileName = FileInformation.fFilename;
      
      // Strip the data file name off the absolute file path and set
      // the path to the DataDirectory variable. Thus, the "current"
      // directory from which a file was selected will become the new
      // default directory that automically opens
      Found = DesplicedFileName.find_last_of("/");
      if(Found != string::npos)
	DesplicedDirectory = DesplicedFileName.substr(0, Found);
      
      Found = DesplicedFileName.find_last_of(".");
      if(Found != string::npos)
	DesplicedFileName = DesplicedFileName.substr(0, Found);
     
      DesplicedFileExtension = FileInformation.fFileTypes[FileInformation.fFileTypeIdx+1];
      Found = DesplicedFileExtension.find_last_of("*");
      if(Found != string::npos)
	DesplicedFileExtension = DesplicedFileExtension.substr(Found+1, DesplicedFileExtension.size());

      string DesplicedFile = DesplicedFileName + DesplicedFileExtension;

      DesplicedFileName_TE->SetText(DesplicedFile.c_str());
    }
    break;
  }
    
  case DesplicedFileCreation_TB_ID:
    // Alert the user the filtering particles by PSD into the spectra
    // requires integration type peak finder to be used
    if(ComputationMgr->GetUsePSDFilters()[ADAQSettings->PSDChannel] and ADAQSettings->ADAQSpectrumIntTypeWW)
      CreateMessageBox("Warning! Use of the PSD filter with spectra creation requires peak finding integration","Asterisk");

    if(ACRONYMFileLoaded){
      CreateMessageBox("Error! ACRONYM files cannot be despliced!","Stop");
      break;
    }
    
    // Sequential processing
    if(ProcessingSeq_RB->IsDown())
      ComputationMgr->CreateDesplicedFile();
    
    // Parallel processing
    else{
      if(ADAQFileLoaded){
	SaveSettings(true);
	ComputationMgr->ProcessWaveformsInParallel("desplicing");
      }
    }
    break;
    
    
    //////////////////
    // Canvas frame //
    //////////////////

  case Quit_TB_ID:
    HandleTerminate();
    break;
  }
}


void AAInterface::HandleCheckButtons()
{
  if(!ADAQFileLoaded and !ACRONYMFileLoaded)
    return;
  
  TGCheckButton *ActiveCheckButton = (TGCheckButton *) gTQSender;
  int CheckButtonID = ActiveCheckButton->WidgetId();

  SaveSettings();
  
  switch(CheckButtonID){
    
  case OverrideTitles_CB_ID:
  case HistogramStats_CB_ID:
  case CanvasGrid_CB_ID:
  case CanvasXAxisLog_CB_ID:
  case CanvasYAxisLog_CB_ID:
  case CanvasZAxisLog_CB_ID:

    switch(GraphicsMgr->GetCanvasContentType()){
    case zEmpty:
      break;

    case zWaveform:
      GraphicsMgr->PlotWaveform();
      break;
      
    case zSpectrum:{
      GraphicsMgr->PlotSpectrum();
      break;
    }
      
    case zSpectrumDerivative:
      GraphicsMgr->PlotSpectrumDerivative();
      break;
      
    case zPSDHistogram:
      GraphicsMgr->PlotPSDHistogram();
      break;
    }
    break;
    
  case SpectrumCalibration_CB_ID:{
    
    if(SpectrumCalibration_CB->IsDown()){
      SetCalibrationWidgetState(true, kButtonUp);
      
      HandleTripleSliderPointer();
    }
    else{
      SetCalibrationWidgetState(false, kButtonDisabled);
    }
    break;
  }
    
  case SpectrumFindBackground_CB_ID:{
    if(!ComputationMgr->GetSpectrumExists())
      break;
    
    EButtonState ButtonState = kButtonDisabled;
    bool WidgetState = false;
    
    if(SpectrumFindBackground_CB->IsDown()){
      ButtonState = kButtonUp;
      WidgetState = true;

      ComputationMgr->CalculateSpectrumBackground();
      GraphicsMgr->PlotSpectrum();
    }
    else{
      SpectrumWithBackground_RB->SetState(kButtonDown, true);

      GraphicsMgr->PlotSpectrum();
    }

    SpectrumBackgroundIterations_NEL->GetEntry()->SetState(WidgetState);
    SpectrumBackgroundCompton_CB->SetState(ButtonState);
    SpectrumBackgroundSmoothing_CB->SetState(ButtonState);
    SpectrumRangeMin_NEL->GetEntry()->SetState(WidgetState);
    SpectrumRangeMax_NEL->GetEntry()->SetState(WidgetState);
    SpectrumBackgroundDirection_CBL->GetComboBox()->SetEnabled(WidgetState);
    SpectrumBackgroundFilterOrder_CBL->GetComboBox()->SetEnabled(WidgetState);
    SpectrumBackgroundSmoothingWidth_CBL->GetComboBox()->SetEnabled(WidgetState);
    SpectrumWithBackground_RB->SetState(ButtonState);
    SpectrumLessBackground_RB->SetState(ButtonState);

    break;
  }

    
  case SpectrumBackgroundCompton_CB_ID:
  case SpectrumBackgroundSmoothing_CB_ID:
    ComputationMgr->CalculateSpectrumBackground();
    GraphicsMgr->PlotSpectrum();
    break;
    

  case SpectrumFindIntegral_CB_ID:
  case SpectrumIntegralInCounts_CB_ID:

    if(SpectrumFindIntegral_CB->IsDown())
      SpectrumIntegrationLimits_DHS->PositionChanged();
    else
      GraphicsMgr->PlotSpectrum();
    break;
    
  case SpectrumUseGaussianFit_CB_ID:
    if(SpectrumUseGaussianFit_CB->IsDown())
      SpectrumIntegrationLimits_DHS->PositionChanged();
    else{
      if(SpectrumFindIntegral_CB->IsDown())
	SpectrumIntegrationLimits_DHS->PositionChanged();
      else
	GraphicsMgr->PlotSpectrum();
    }
    break;

  case IntegratePearson_CB_ID:{
    EButtonState ButtonState = kButtonDisabled;
    bool WidgetState = false;
    
    if(IntegratePearson_CB->IsDown()){
      // Set states to activate buttons
      ButtonState = kButtonUp;
      WidgetState = true;
    }
    
    PlotPearsonIntegration_CB->SetState(ButtonState);
    PearsonChannel_CBL->GetComboBox()->SetEnabled(WidgetState);
    PearsonPolarityPositive_RB->SetState(ButtonState);
    PearsonPolarityNegative_RB->SetState(ButtonState);
    IntegrateRawPearson_RB->SetState(ButtonState);
    IntegrateFitToPearson_RB->SetState(ButtonState);
    PearsonLowerLimit_NEL->GetEntry()->SetState(WidgetState);
    if(WidgetState==true and IntegrateFitToPearson_RB->IsDown())
      PearsonMiddleLimit_NEL->GetEntry()->SetState(WidgetState);
    PearsonUpperLimit_NEL->GetEntry()->SetState(WidgetState);

    if(IntegratePearson_CB->IsDown() and PlotPearsonIntegration_CB->IsDown()){

      GraphicsMgr->PlotWaveform();
      
      double DeuteronsInWaveform = ComputationMgr->GetDeuteronsInWaveform();
      DeuteronsInWaveform_NEFL->GetEntry()->SetNumber(DeuteronsInWaveform);
      
      double DeuteronsInTotal = ComputationMgr->GetDeuteronsInTotal();
      DeuteronsInTotal_NEFL->GetEntry()->SetNumber(DeuteronsInTotal);
    }

    break;
  }
    
  case PSDEnable_CB_ID:{
    EButtonState ButtonState = kButtonDisabled;
    bool WidgetState = false;
    
    if(PSDEnable_CB->IsDown()){
      ButtonState = kButtonUp;
      WidgetState = true;
    }

    // Be sure to turn off PSD filtering if the user does not want to
    // discriminate by pulse shape
    else{
      PSDEnableFilter_CB->SetState(kButtonUp);
      if(GraphicsMgr->GetCanvasContentType() == zWaveform)
	GraphicsMgr->PlotWaveform();
    }

    SetPSDWidgetState(WidgetState, ButtonState);

    break;
  }

  case PSDEnableFilterCreation_CB_ID:{
    
    if(PSDEnableFilterCreation_CB->IsDown() and GraphicsMgr->GetCanvasContentType() != zPSDHistogram){
      CreateMessageBox("The canvas does not presently contain a PSD histogram! PSD filter creation is not possible!","Stop");
      PSDEnableFilterCreation_CB->SetState(kButtonUp);
      break;
    }
    break;
  }

  case PSDEnableFilter_CB_ID:{
    if(PSDEnableFilter_CB->IsDown()){
      ComputationMgr->SetUsePSDFilter(PSDChannel_CBL->GetComboBox()->GetSelected(), true);
      FindPeaks_CB->SetState(kButtonDown);
    }
    else
      ComputationMgr->SetUsePSDFilter(PSDChannel_CBL->GetComboBox()->GetSelected(), false);
    break;
  }
    
  case PSDPlotTailIntegration_CB_ID:{
    if(!FindPeaks_CB->IsDown())
      FindPeaks_CB->SetState(kButtonDown);
    GraphicsMgr->PlotWaveform();
    break;
  }

  case PSDEnableHistogramSlicing_CB_ID:{
    if(PSDEnableHistogramSlicing_CB->IsDown()){
      PSDHistogramSliceX_RB->SetState(kButtonUp);
      PSDHistogramSliceY_RB->SetState(kButtonUp);
      
      // Temporary hack ZSH 12 Feb 13
      PSDHistogramSliceX_RB->SetState(kButtonDown);
    }
    else{
      // Disable histogram buttons
      PSDHistogramSliceX_RB->SetState(kButtonDisabled);
      PSDHistogramSliceY_RB->SetState(kButtonDisabled);

      // Replot the PSD histogram
      GraphicsMgr->PlotPSDHistogram();
      
      // Delete the canvas containing the PSD slice histogram and
      // close the window (formerly) containing the canvas
      TCanvas *PSDSlice_C = (TCanvas *)gROOT->GetListOfCanvases()->FindObject("PSDSlice_C");
      if(PSDSlice_C)
	PSDSlice_C->Close();
    }
    break;
  }

  case EAEnable_CB_ID:{
    
    int Channel = ChannelSelector_CBL->GetComboBox()->GetSelected();
    
    bool SpectrumIsCalibrated = ComputationMgr->GetUseSpectraCalibrations()[Channel];
    
    int Type = EASpectrumType_CBL->GetComboBox()->GetSelected();
    
    if(EAEnable_CB->IsDown() and SpectrumIsCalibrated){
      SpectrumCalibration_CB->SetState(kButtonUp, true);
      SetCalibrationWidgetState(false, kButtonDisabled);
      
      EASpectrumType_CBL->GetComboBox()->SetEnabled(true);
      
      if(Type == 0){
	SetEAGammaWidgetState(true, kButtonUp);
	SetEANeutronWidgetState(false, kButtonDisabled);
      }
      else if(Type == 1){
	SetEAGammaWidgetState(false, kButtonDisabled);
	SetEANeutronWidgetState(true, kButtonUp);
      }
      HandleTripleSliderPointer();
    }
    else{
      EASpectrumType_CBL->GetComboBox()->SetEnabled(false);

      SetEAGammaWidgetState(false, kButtonDisabled);
      SetEANeutronWidgetState(false, kButtonDisabled);
      
      if(SpectrumIsCalibrated)
	GraphicsMgr->PlotSpectrum();
    }
    break;
  }

  case EAEscapePeaks_CB_ID:{
    HandleTripleSliderPointer();
    break;
  }

  case PlotSpectrumDerivativeError_CB_ID:
  case PlotAbsValueSpectrumDerivative_CB_ID:
  case SpectrumOverplotDerivative_CB_ID:{
    if(!ComputationMgr->GetSpectrumExists()){
      CreateMessageBox("A valid spectrum does not yet exists! The calculation of a spectrum derivative is, therefore, moot!", "Stop");
      break;
    }
    else{
      //      if(SpectrumOverplotDerivative_CB->IsDown()){
      //	GraphicsMgr->PlotSpectrum();
      //	GraphicsMgr->PlotSpectrumDerivative();
      //      }
      if(PlotSpectrumDerivativeError_CB->IsDown()){
	GraphicsMgr->PlotSpectrumDerivative();
      }
      else{
	GraphicsMgr->PlotSpectrum();
      }
      break;
    }
  }

  default:
    break;
  }
}


void AAInterface::HandleSliders(int SliderPosition)
{
  if(!ADAQFileLoaded or ACRONYMFileLoaded)
    return;
  
  SaveSettings();
  
  WaveformSelector_NEL->GetEntry()->SetNumber(SliderPosition);
  
  GraphicsMgr->PlotWaveform();

  // Update the deuteron/Pearson integration widgets
  if(IntegratePearson_CB->IsDown()){
    double DeuteronsInWaveform = ComputationMgr->GetDeuteronsInWaveform();
    DeuteronsInWaveform_NEFL->GetEntry()->SetNumber(DeuteronsInWaveform);
    
    double DeuteronsInTotal = ComputationMgr->GetDeuteronsInTotal();
    DeuteronsInTotal_NEFL->GetEntry()->SetNumber(DeuteronsInTotal);
  }

  // Update the waveform analysis widgets
  if(WaveformAnalysis_CB->IsDown()){
    double Height = ComputationMgr->GetWaveformAnalysisHeight();
    WaveformHeight_NEL->GetEntry()->SetNumber(Height);
    
    double Area = ComputationMgr->GetWaveformAnalysisArea();
    WaveformIntegral_NEL->GetEntry()->SetNumber(Area);
  }
}


void AAInterface::HandleDoubleSliders()
{
  if(!ADAQFileLoaded and !ACRONYMFileLoaded)
    return;
  
  SaveSettings();
  
  // Get the currently active widget and its ID, i.e. the widget from
  // which the user has just sent a signal...
  TGDoubleSlider *ActiveDoubleSlider = (TGDoubleSlider *) gTQSender;
  int SliderID = ActiveDoubleSlider->WidgetId();

  switch(SliderID){
    
  case XAxisLimits_THS_ID:
  case YAxisLimits_DVS_ID:
    if(GraphicsMgr->GetCanvasContentType() == zWaveform)
      GraphicsMgr->PlotWaveform();

    else if(GraphicsMgr->GetCanvasContentType() == zSpectrum and ComputationMgr->GetSpectrumExists())
      GraphicsMgr->PlotSpectrum();
    
    else if(GraphicsMgr->GetCanvasContentType() == zSpectrumDerivative and ComputationMgr->GetSpectrumExists())
      GraphicsMgr->PlotSpectrumDerivative();
    
    else if(GraphicsMgr->GetCanvasContentType() == zPSDHistogram and ComputationMgr->GetSpectrumExists())
      GraphicsMgr->PlotPSDHistogram();
    
    break;

  case SpectrumIntegrationLimits_DHS_ID:
    ComputationMgr->IntegrateSpectrum();
    GraphicsMgr->PlotSpectrum();
    
    if(SpectrumFindIntegral_CB->IsDown()){
      SpectrumIntegral_NEFL->GetEntry()->SetNumber( ComputationMgr->GetSpectrumIntegralValue() );
      SpectrumIntegralError_NEFL->GetEntry()->SetNumber (ComputationMgr->GetSpectrumIntegralError () );
    }
    else{
      SpectrumIntegral_NEFL->GetEntry()->SetNumber(0.);
      SpectrumIntegralError_NEFL->GetEntry()->SetNumber(0.);
    }
    
    if(SpectrumUseGaussianFit_CB->IsDown()){
      double Const = ComputationMgr->GetSpectrumFit()->GetParameter(0);
      double Mean = ComputationMgr->GetSpectrumFit()->GetParameter(1);
      double Sigma = ComputationMgr->GetSpectrumFit()->GetParameter(2);
      
      SpectrumFitHeight_NEFL->GetEntry()->SetNumber(Const);
      SpectrumFitMean_NEFL->GetEntry()->SetNumber(Mean);
      SpectrumFitSigma_NEFL->GetEntry()->SetNumber(Sigma);
      SpectrumFitRes_NEFL->GetEntry()->SetNumber(2.35 * Sigma / Mean * 100);
    }
    break;
  }
}


void AAInterface::HandleTripleSliderPointer()
{
  if(!ADAQFileLoaded and !ACRONYMFileLoaded)
    return;

  if(ComputationMgr->GetSpectrumExists() and
     GraphicsMgr->GetCanvasContentType() == zSpectrum){

    // Get the current spectrum
    TH1F *Spectrum_H = ComputationMgr->GetSpectrum();

    // Calculate the position along the X-axis of the pulse spectrum
    // (the "area" or "height" in ADC units) based on the current
    // X-axis maximum and the triple slider's pointer position
    double XPos = XAxisLimits_THS->GetPointerPosition() * Spectrum_H->GetXaxis()->GetXmax();

    // Calculate min. and max. on the Y-axis for plotting
    float Min, Max;
    YAxisLimits_DVS->GetPosition(Min, Max);
    double YMin = Spectrum_H->GetBinContent(Spectrum_H->GetMaximumBin()) * (1-Max);
    double YMax = Spectrum_H->GetBinContent(Spectrum_H->GetMaximumBin()) * (1-Min);

    // To enable easy spectra calibration, the user has the options of
    // dragging the pointer of the X-axis horizontal slider just below
    // the canvas, which results in a "calibration line" drawn over
    // the plotted pulse spectrum. As the user drags the line, the
    // pulse unit number entry widget in the calibration group frame
    // is update, allowing the user to easily set the value of pulse
    // units (in ADC) of a feature that appears in the spectrum with a
    // known calibration energy, entered in the number entry widget
    // above.

    // If the pulse spectrum object (Spectrum_H) exists and the user has
    // selected calibration mode via the appropriate buttons ...
    if(SpectrumCalibration_CB->IsDown() and SpectrumCalibrationStandard_RB->IsDown()){
      
      // Plot the calibration line
      GraphicsMgr->PlotVCalibrationLine(XPos);
      
      // Set the calibration pulse units for the current calibration
      // point based on the X position of the calibration line
      SpectrumCalibrationPulseUnit_NEL->GetEntry()->SetNumber(XPos);
    }
    
    // The user can drag the triple slider point in order to calculate
    // the energy deposition from other particle to produce the
    // equivalent amount of light as electrons. This feature is only
    // intended for use for EJ301/EJ309 liqoid organic scintillators.
    // Note that the spectra must be calibrated in MeVee.
    
    const int Channel = ChannelSelector_CBL->GetComboBox()->GetSelected();
    bool SpectrumIsCalibrated = ComputationMgr->GetUseSpectraCalibrations()[Channel];
    
    if(EAEnable_CB->IsDown() and SpectrumIsCalibrated){
      
      int Type = EASpectrumType_CBL->GetComboBox()->GetSelected();

      double Error = 0.;
      bool ErrorBox = false;
      bool EscapePeaks = EAEscapePeaks_CB->IsDown();
      
      if(Type == 0){
	GraphicsMgr->PlotEALine(XPos, Error, ErrorBox, EscapePeaks);
	
	EAGammaEDep_NEL->GetEntry()->SetNumber(XPos);
      }
      else if(Type == 1){

	Error = EAErrorWidth_NEL->GetEntry()->GetNumber();
	ErrorBox = true;
	EscapePeaks = false;
	
	GraphicsMgr->PlotEALine(XPos, Error, ErrorBox, EscapePeaks);
	
	EAElectronEnergy_NEL->GetEntry()->SetNumber(XPos);
	
	double GE = InterpolationMgr->GetGammaEnergy(XPos);
	EAGammaEnergy_NEL->GetEntry()->SetNumber(GE);
	
	double PE = InterpolationMgr->GetProtonEnergy(XPos);
	EAProtonEnergy_NEL->GetEntry()->SetNumber(PE);
	
	double AE = InterpolationMgr->GetAlphaEnergy(XPos);
	EAAlphaEnergy_NEL->GetEntry()->SetNumber(AE);
	
	double CE = InterpolationMgr->GetCarbonEnergy(XPos);
	EACarbonEnergy_NEL->GetEntry()->SetNumber(CE);
      }
    }
  }
}


void AAInterface::HandleNumberEntries()
{ 
  if(!ADAQFileLoaded and !ACRONYMFileLoaded)
    return;

  // Get the "active" widget object/ID from which a signal has been sent
  TGNumberEntry *ActiveNumberEntry = (TGNumberEntry *) gTQSender;
  int NumberEntryID = ActiveNumberEntry->WidgetId();
  
  SaveSettings();

  switch(NumberEntryID){
    
  case SpectrumBackgroundIterations_NEL_ID:
    ComputationMgr->CalculateSpectrumBackground();
    GraphicsMgr->PlotSpectrum();
    break;

  case SpectrumCalibrationEnergy_NEL_ID:
  case SpectrumCalibrationPulseUnit_NEL_ID:{
    // Get the current spectrum
    TH1F *Spectrum_H = ComputationMgr->GetSpectrum();
    
    // Get the value set in the PulseUnit number entry
    double Value = 0.;
    if(SpectrumCalibrationEnergy_NEL_ID == NumberEntryID)
      Value = SpectrumCalibrationEnergy_NEL->GetEntry()->GetNumber();
    else
      Value = SpectrumCalibrationPulseUnit_NEL->GetEntry()->GetNumber();
    
    // Draw the new calibration line value ontop of the spectrum
    GraphicsMgr->PlotVCalibrationLine(Value);
    break;
  }

  case EALightConversionFactor_NEL_ID:{
    double CF = EALightConversionFactor_NEL->GetEntry()->GetNumber();
    InterpolationMgr->SetConversionFactor(CF);
    InterpolationMgr->ConstructResponses();
    HandleTripleSliderPointer();
    break;
  }
    
  case EAErrorWidth_NEL_ID:
    HandleTripleSliderPointer();
    break;

  case EAElectronEnergy_NEL_ID:
  case EAGammaEnergy_NEL_ID:
  case EAProtonEnergy_NEL_ID:
  case EAAlphaEnergy_NEL_ID:
  case EACarbonEnergy_NEL_ID:
    break;
    
  case XAxisSize_NEL_ID:
  case XAxisOffset_NEL_ID:
  case XAxisDivs_NEL_ID:
  case YAxisSize_NEL_ID:
  case YAxisOffset_NEL_ID:
  case YAxisDivs_NEL_ID:
  case ZAxisSize_NEL_ID:
  case ZAxisOffset_NEL_ID:
  case ZAxisDivs_NEL_ID:
  case PaletteAxisSize_NEL_ID:
  case PaletteAxisOffset_NEL_ID:
  case PaletteAxisDivs_NEL_ID:  
    
    switch(GraphicsMgr->GetCanvasContentType()){
    case zWaveform:
      GraphicsMgr->PlotWaveform();
      break;
      
    case zSpectrum:
      GraphicsMgr->PlotSpectrum();
      break;

    case zSpectrumDerivative:
      GraphicsMgr->PlotSpectrumDerivative();
      break;
      
    case zPSDHistogram:
      GraphicsMgr->PlotPSDHistogram();
      break;
    }
    break;

  default:
    break;
  }
}


void AAInterface::HandleRadioButtons()
{
  if(!ADAQFileLoaded and !ACRONYMFileLoaded)
    return;
  
  TGRadioButton *ActiveRadioButton = (TGRadioButton *) gTQSender;
  int RadioButtonID = ActiveRadioButton->WidgetId();
  
  SaveSettings();

  switch(RadioButtonID){
  
  case SpectrumWithBackground_RB_ID:
    SpectrumLessBackground_RB->SetState(kButtonUp);
    SaveSettings();
    ComputationMgr->CalculateSpectrumBackground();
    GraphicsMgr->PlotSpectrum();

    if(SpectrumFindIntegral_CB->IsDown()){
      SpectrumFindIntegral_CB->SetState(kButtonUp, true);
      SpectrumFindIntegral_CB->SetState(kButtonDown, true);
    }
    break;

  case SpectrumLessBackground_RB_ID:
    SpectrumWithBackground_RB->SetState(kButtonUp);
    SaveSettings();
    ComputationMgr->CalculateSpectrumBackground();
    GraphicsMgr->PlotSpectrum();

    if(SpectrumFindIntegral_CB->IsDown()){
      SpectrumFindIntegral_CB->SetState(kButtonUp, true);
      SpectrumFindIntegral_CB->SetState(kButtonDown, true);
    }
    break;
    
  case IntegrateRawPearson_RB_ID:
    IntegrateFitToPearson_RB->SetState(kButtonUp);
    PearsonMiddleLimit_NEL->GetEntry()->SetState(false);
    GraphicsMgr->PlotWaveform();
    break;

  case IntegrateFitToPearson_RB_ID:
    IntegrateRawPearson_RB->SetState(kButtonUp);
    PearsonMiddleLimit_NEL->GetEntry()->SetState(true);
    GraphicsMgr->PlotWaveform();
    break;

  case PearsonPolarityPositive_RB_ID:
    PearsonPolarityNegative_RB->SetState(kButtonUp);
    GraphicsMgr->PlotWaveform();
    break;

  case PearsonPolarityNegative_RB_ID:
    PearsonPolarityPositive_RB->SetState(kButtonUp);
    GraphicsMgr->PlotWaveform();
    break;

  case ProcessingSeq_RB_ID:
    if(ProcessingSeq_RB->IsDown())
      ProcessingPar_RB->SetState(kButtonUp);

    NumProcessors_NEL->GetEntry()->SetState(false);
    NumProcessors_NEL->GetEntry()->SetNumber(1);
    break;
    
  case ProcessingPar_RB_ID:
    if(ProcessingPar_RB->IsDown())
      ProcessingSeq_RB->SetState(kButtonUp);

    NumProcessors_NEL->GetEntry()->SetState(true);
    NumProcessors_NEL->GetEntry()->SetNumber(NumProcessors);
    break;

  case PSDPositiveFilter_RB_ID:
    if(PSDPositiveFilter_RB->IsDown()){
      PSDNegativeFilter_RB->SetState(kButtonUp);
    }
    break;

  case PSDNegativeFilter_RB_ID:
    if(PSDNegativeFilter_RB->IsDown()){
      PSDPositiveFilter_RB->SetState(kButtonUp);
    }
    break;

  case PSDHistogramSliceX_RB_ID:
    if(PSDHistogramSliceX_RB->IsDown())
      PSDHistogramSliceY_RB->SetState(kButtonUp);
    
    if(GraphicsMgr->GetCanvasContentType() == zPSDHistogram)
      {}//PlotPSDHistogram();
    break;
    
  case PSDHistogramSliceY_RB_ID:
    if(PSDHistogramSliceY_RB->IsDown())
      PSDHistogramSliceX_RB->SetState(kButtonUp);
    if(GraphicsMgr->GetCanvasContentType() == zPSDHistogram)
      {}//PlotPSDHistogram();
    break;
    
  case DrawWaveformWithCurve_RB_ID:
  case DrawWaveformWithMarkers_RB_ID:
  case DrawWaveformWithBoth_RB_ID:
    GraphicsMgr->PlotWaveform();
    break;

  case DrawSpectrumWithBars_RB_ID:
  case DrawSpectrumWithCurve_RB_ID:
  case DrawSpectrumWithError_RB_ID:
  case DrawSpectrumWithMarkers_RB_ID:
    if(ComputationMgr->GetSpectrumExists())
      GraphicsMgr->PlotSpectrum();
    break;
  }
}


void AAInterface::HandleComboBox(int ComboBoxID, int SelectedID)
{
  if(!ADAQFileLoaded and !ACRONYMFileLoaded)
    return;

  SaveSettings();

  switch(ComboBoxID){

  case EASpectrumType_CBL_ID:
    if(SelectedID == 0){
      SetEAGammaWidgetState(true, kButtonUp);
      SetEANeutronWidgetState(false, kButtonDisabled);
    }
    else if(SelectedID == 1){
      SetEAGammaWidgetState(false, kButtonDisabled);
      SetEANeutronWidgetState(true, kButtonUp);
    }
    break;
    
  case PSDPlotType_CBL_ID:
    if(ComputationMgr->GetPSDHistogramExists())
      {}//PlotPSDHistogram();
    break;

  case SpectrumBackgroundDirection_CBL_ID:
  case SpectrumBackgroundFilterOrder_CBL_ID:
  case SpectrumBackgroundSmoothingWidth_CBL_ID:
    ComputationMgr->CalculateSpectrumBackground();
    GraphicsMgr->PlotSpectrum();
    break;
  }
}


void AAInterface::HandleCanvas(int EventID, int XPixel, int YPixel, TObject *Selected)
{
  if(!ADAQFileLoaded and !ACRONYMFileLoaded)
    return;

  // For an unknown reason, the XPixel value appears (erroneously) to
  // be two pixels too low for a given cursor selection, I have
  // examined this issue in detail but cannot determine the reason nor
  // is it treated in the ROOT forums. At present, the fix is simply
  // to add two pixels for a given XPixel cursor selection.
  XPixel += 2;

  // If the user has enabled the creation of a PSD filter and the
  // canvas event is equal to "1" (which represents a down-click
  // somewhere on the canvas pad) then send the pixel coordinates of
  // the down-click to the PSD filter creation function
  if(PSDEnableFilterCreation_CB->IsDown() and EventID == 1){
    ComputationMgr->CreatePSDFilter(XPixel, YPixel);
    GraphicsMgr->PlotPSDFilter();
  }
  
  if(PSDEnableHistogramSlicing_CB->IsDown()){
    
    // The user may click the canvas to "freeze" the PSD histogram
    // slice position, which ensures the PSD histogram slice at that
    // point remains plotted in the standalone canvas
    if(EventID == 1){
      PSDEnableHistogramSlicing_CB->SetState(kButtonUp);
      return;
    }
    else{
      ComputationMgr->CreatePSDHistogramSlice(XPixel, YPixel);
      GraphicsMgr->PlotPSDHistogramSlice(XPixel, YPixel);
    }
  }

  // The user has the option of using an automated edge location
  // finder for setting the calibration of EJ301/9 liq. organic
  // scintillators. The user set two points that must "bound" the
  // spectral edge:
  // point 0 : top height of edge; leftmost pulse unit
  // point 1 : bottom height of edge; rightmost pulse unit
  if(SpectrumCalibrationEdgeFinder_RB->IsDown()){

    if(ComputationMgr->GetSpectrumExists() and
       GraphicsMgr->GetCanvasContentType() == zSpectrum){

      // Get the current spectrum
      TH1F *Spectrum_H = ComputationMgr->GetSpectrum();

      double X = gPad->AbsPixeltoX(XPixel);
      double Y = gPad->AbsPixeltoY(YPixel);

      // Enables the a semi-transparent box to be drawn over the edge
      // to be calibrated once the user has clicked for the first point
      if(NumEdgeBoundingPoints == 1)
	GraphicsMgr->PlotEdgeBoundingBox(EdgeBoundX0, EdgeBoundY0, X, Y);

      // The bound point is set once the user clicks the canvas at the
      // desired (X,Y) == (Pulse unit, Counts) location. Note that
      // AAComputation class automatically keep track of which point
      // is set and when to calculate the edge
      if(EventID == 1){
	ComputationMgr->SetEdgeBound(X, Y);

	// Keep track of the number of times the user has clicked
	if(NumEdgeBoundingPoints == 0){
	  EdgeBoundX0 = X;
	  EdgeBoundY0 = Y;
	}
	
	NumEdgeBoundingPoints++;
	
	// Once the edge position is found (after two points are set)
	// then update the number entry so the user may set a
	// calibration points and draw the point on screen for
	// verification. Reset number of edge bounding points.
	if(ComputationMgr->GetEdgePositionFound()){
	  double HalfHeight = ComputationMgr->GetHalfHeight();
	  double EdgePos = ComputationMgr->GetEdgePosition();
	  SpectrumCalibrationPulseUnit_NEL->GetEntry()->SetNumber(EdgePos);
	  
	  GraphicsMgr->PlotCalibrationCross(EdgePos, HalfHeight);
	  
	  NumEdgeBoundingPoints = 0;
	}
      }
    }
  }
}


void AAInterface::HandleTerminate()
{ gApplication->Terminate(); }


void AAInterface::SaveSettings(bool SaveToFile)
{
  if(!ADAQSettings)
    ADAQSettings = new AASettings;
  
  TFile *ADAQSettingsFile = 0;
  if(SaveToFile)
    ADAQSettingsFile = new TFile(ADAQSettingsFileName.c_str(), "recreate");
  
  //////////////////////////////////////////
  // Values from the "Waveform" tabbed frame 

  ADAQSettings->WaveformChannel = ChannelSelector_CBL->GetComboBox()->GetSelected();
  ADAQSettings->WaveformToPlot = WaveformSelector_NEL->GetEntry()->GetIntNumber();

  ADAQSettings->RawWaveform = RawWaveform_RB->IsDown();
  ADAQSettings->BSWaveform = BaselineSubtractedWaveform_RB->IsDown();
  ADAQSettings->ZSWaveform = ZeroSuppressionWaveform_RB->IsDown();

  ADAQSettings->PlotZeroSuppressionCeiling = PlotZeroSuppressionCeiling_CB->IsDown();
  ADAQSettings->ZeroSuppressionCeiling = ZeroSuppressionCeiling_NEL->GetEntry()->GetIntNumber();
  
  if(PositiveWaveform_RB->IsDown())
    ADAQSettings->WaveformPolarity = 1.0;
  else
    ADAQSettings->WaveformPolarity = -1.0;
  
  ADAQSettings->FindPeaks = FindPeaks_CB->IsDown();
  ADAQSettings->UseMarkovSmoothing = UseMarkovSmoothing_CB->IsDown();
  ADAQSettings->MaxPeaks = MaxPeaks_NEL->GetEntry()->GetIntNumber();
  ADAQSettings->Sigma = Sigma_NEL->GetEntry()->GetNumber();
  ADAQSettings->Resolution = Resolution_NEL->GetEntry()->GetNumber();
  ADAQSettings->Floor = Floor_NEL->GetEntry()->GetIntNumber();

  ADAQSettings->PlotFloor = PlotFloor_CB->IsDown();
  ADAQSettings->PlotCrossings = PlotCrossings_CB->IsDown();
  ADAQSettings->PlotPeakIntegrationRegion = PlotPeakIntegratingRegion_CB->IsDown();

  ADAQSettings->UsePileupRejection = UsePileupRejection_CB->IsDown();

  ADAQSettings->PlotBaselineCalcRegion = PlotBaseline_CB->IsDown();
  ADAQSettings->BaselineCalcMin = BaselineCalcMin_NEL->GetEntry()->GetIntNumber();
  ADAQSettings->BaselineCalcMax = BaselineCalcMax_NEL->GetEntry()->GetIntNumber();

  ADAQSettings->PlotTrigger = PlotTrigger_CB->IsDown();
  
  ADAQSettings->WaveformAnalysis = WaveformAnalysis_CB->IsDown();

  //////////////////////////////////////
  // Values from "Spectrum" tabbed frame

  ADAQSettings->WaveformsToHistogram = WaveformsToHistogram_NEL->GetEntry()->GetIntNumber();
  ADAQSettings->SpectrumNumBins = SpectrumNumBins_NEL->GetEntry()->GetIntNumber();
  ADAQSettings->SpectrumMinBin = SpectrumMinBin_NEL->GetEntry()->GetNumber();
  ADAQSettings->SpectrumMaxBin = SpectrumMaxBin_NEL->GetEntry()->GetNumber();

  ADAQSettings->ADAQSpectrumTypePAS = ADAQSpectrumTypePAS_RB->IsDown();
  ADAQSettings->ADAQSpectrumTypePHS = ADAQSpectrumTypePHS_RB->IsDown();
  ADAQSettings->ADAQSpectrumIntTypeWW = ADAQSpectrumIntTypeWW_RB->IsDown();
  ADAQSettings->ADAQSpectrumIntTypePF = ADAQSpectrumIntTypePF_RB->IsDown();

  ADAQSettings->ACROSpectrumTypeEnergy = ACROSpectrumTypeEnergy_RB->IsDown();
  ADAQSettings->ACROSpectrumTypeScintCreated = ACROSpectrumTypeScintCreated_RB->IsDown();
  ADAQSettings->ACROSpectrumTypeScintCounted= ACROSpectrumTypeScintCounted_RB->IsDown();
  ADAQSettings->ACROSpectrumLS = ACROSpectrumLS_RB->IsDown();
  ADAQSettings->ACROSpectrumES = ACROSpectrumES_RB->IsDown();

  ADAQSettings->EnergyUnit = SpectrumCalibrationUnit_CBL->GetComboBox()->GetSelected();

  
  //////////////////////////////////////////
  // Values from the "Analysis" tabbed frame 

  ADAQSettings->FindBackground = SpectrumFindBackground_CB->IsDown();
  ADAQSettings->BackgroundIterations = SpectrumBackgroundIterations_NEL->GetEntry()->GetIntNumber();
  ADAQSettings->BackgroundCompton = SpectrumBackgroundCompton_CB->IsDown();
  ADAQSettings->BackgroundSmoothing = SpectrumBackgroundSmoothing_CB->IsDown();
  ADAQSettings->BackgroundMinBin = SpectrumRangeMin_NEL->GetEntry()->GetNumber();
  ADAQSettings->BackgroundMaxBin = SpectrumRangeMax_NEL->GetEntry()->GetNumber();
  ADAQSettings->BackgroundDirection = SpectrumBackgroundDirection_CBL->GetComboBox()->GetSelected();
  ADAQSettings->BackgroundFilterOrder = SpectrumBackgroundFilterOrder_CBL->GetComboBox()->GetSelected();
  ADAQSettings->BackgroundSmoothingWidth = SpectrumBackgroundSmoothingWidth_CBL->GetComboBox()->GetSelected();

  ADAQSettings->PlotWithBackground = SpectrumWithBackground_RB->IsDown();
  ADAQSettings->PlotLessBackground = SpectrumLessBackground_RB->IsDown();
  
  ADAQSettings->SpectrumFindIntegral = SpectrumFindIntegral_CB->IsDown();
  ADAQSettings->SpectrumIntegralInCounts = SpectrumIntegralInCounts_CB->IsDown();
  ADAQSettings->SpectrumUseGaussianFit = SpectrumUseGaussianFit_CB->IsDown();


  //////////////////////////////////////////
  // Values from the "Graphics" tabbed frame

  ADAQSettings->WaveformCurve = DrawWaveformWithCurve_RB->IsDown();
  ADAQSettings->WaveformMarkers = DrawWaveformWithMarkers_RB->IsDown();
  ADAQSettings->WaveformBoth = DrawWaveformWithBoth_RB->IsDown();
  
  ADAQSettings->SpectrumCurve = DrawSpectrumWithCurve_RB->IsDown();
  ADAQSettings->SpectrumMarkers = DrawSpectrumWithMarkers_RB->IsDown();
  ADAQSettings->SpectrumError = DrawSpectrumWithError_RB->IsDown();
  ADAQSettings->SpectrumBars = DrawSpectrumWithBars_RB->IsDown();
  
  ADAQSettings->HistogramStats = HistogramStats_CB->IsDown();
  ADAQSettings->CanvasGrid = CanvasGrid_CB->IsDown();
  ADAQSettings->CanvasXAxisLog = CanvasXAxisLog_CB->IsDown();
  ADAQSettings->CanvasYAxisLog = CanvasYAxisLog_CB->IsDown();
  ADAQSettings->CanvasZAxisLog = CanvasZAxisLog_CB->IsDown();

  ADAQSettings->PlotSpectrumDerivativeError = PlotSpectrumDerivativeError_CB->IsDown();
  ADAQSettings->PlotAbsValueSpectrumDerivative = PlotAbsValueSpectrumDerivative_CB->IsDown();
  ADAQSettings->PlotYAxisWithAutoRange = AutoYAxisRange_CB->IsDown();
  
  ADAQSettings->OverrideGraphicalDefault = OverrideTitles_CB->IsDown();

  ADAQSettings->PlotTitle = Title_TEL->GetEntry()->GetText();
  ADAQSettings->XAxisTitle = XAxisTitle_TEL->GetEntry()->GetText();
  ADAQSettings->YAxisTitle = YAxisTitle_TEL->GetEntry()->GetText();
  ADAQSettings->ZAxisTitle = ZAxisTitle_TEL->GetEntry()->GetText();
  ADAQSettings->PaletteTitle = PaletteAxisTitle_TEL->GetEntry()->GetText();

  ADAQSettings->XSize = XAxisSize_NEL->GetEntry()->GetNumber();
  ADAQSettings->YSize = YAxisSize_NEL->GetEntry()->GetNumber();
  ADAQSettings->ZSize = ZAxisSize_NEL->GetEntry()->GetNumber();
  ADAQSettings->PaletteSize = PaletteAxisSize_NEL->GetEntry()->GetNumber();

  ADAQSettings->XOffset = XAxisOffset_NEL->GetEntry()->GetNumber();
  ADAQSettings->YOffset = YAxisOffset_NEL->GetEntry()->GetNumber();
  ADAQSettings->ZOffset = ZAxisOffset_NEL->GetEntry()->GetNumber();
  ADAQSettings->PaletteOffset = PaletteAxisOffset_NEL->GetEntry()->GetNumber();

  ADAQSettings->XDivs = XAxisDivs_NEL->GetEntry()->GetNumber();
  ADAQSettings->YDivs = YAxisDivs_NEL->GetEntry()->GetNumber();
  ADAQSettings->ZDivs = ZAxisDivs_NEL->GetEntry()->GetNumber();

  ADAQSettings->PaletteX1 = PaletteX1_NEL->GetEntry()->GetNumber();
  ADAQSettings->PaletteX2 = PaletteX2_NEL->GetEntry()->GetNumber();
  ADAQSettings->PaletteY1 = PaletteY1_NEL->GetEntry()->GetNumber();
  ADAQSettings->PaletteY2 = PaletteY2_NEL->GetEntry()->GetNumber();


  ////////////////////////////////////////
  // Values from "Processing" tabbed frame

  ADAQSettings->SeqProcessing = ProcessingSeq_RB->IsDown();
  ADAQSettings->ParProcessing = ProcessingSeq_RB->IsDown();

  ADAQSettings->NumProcessors = NumProcessors_NEL->GetEntry()->GetIntNumber();
  ADAQSettings->UpdateFreq = UpdateFreq_NEL->GetEntry()->GetIntNumber();

  ADAQSettings->PSDEnable = PSDEnable_CB->IsDown();
  ADAQSettings->PSDChannel = PSDChannel_CBL->GetComboBox()->GetSelected();
  ADAQSettings->PSDWaveformsToDiscriminate = PSDWaveforms_NEL->GetEntry()->GetIntNumber();
  
  ADAQSettings->PSDNumTailBins = PSDNumTailBins_NEL->GetEntry()->GetIntNumber();
  ADAQSettings->PSDMinTailBin = PSDMinTailBin_NEL->GetEntry()->GetIntNumber();
  ADAQSettings->PSDMaxTailBin = PSDMaxTailBin_NEL->GetEntry()->GetIntNumber();

  ADAQSettings->PSDNumTotalBins = PSDNumTotalBins_NEL->GetEntry()->GetIntNumber();
  ADAQSettings->PSDMinTotalBin = PSDMinTotalBin_NEL->GetEntry()->GetIntNumber();
  ADAQSettings->PSDMaxTotalBin = PSDMaxTotalBin_NEL->GetEntry()->GetIntNumber();

  ADAQSettings->PSDThreshold = PSDThreshold_NEL->GetEntry()->GetIntNumber();
  ADAQSettings->PSDPeakOffset = PSDPeakOffset_NEL->GetEntry()->GetIntNumber();
  ADAQSettings->PSDTailOffset = PSDTailOffset_NEL->GetEntry()->GetIntNumber();

  ADAQSettings->PSDPlotType = PSDPlotType_CBL->GetComboBox()->GetSelected();
  ADAQSettings->PSDPlotTailIntegrationRegion = PSDPlotTailIntegration_CB->IsDown();

  if(PSDPositiveFilter_RB->IsDown())
    ADAQSettings->PSDFilterPolarity = 1.0;
  else
    ADAQSettings->PSDFilterPolarity = -1.0;

  ADAQSettings->PSDXSlice = PSDHistogramSliceX_RB->IsDown();
  ADAQSettings->PSDYSlice = PSDHistogramSliceY_RB->IsDown();

  ADAQSettings->IntegratePearson = IntegratePearson_CB->IsDown();
  ADAQSettings->PearsonChannel = PearsonChannel_CBL->GetComboBox()->GetSelected();  

  if(PearsonPolarityPositive_RB->IsDown())
    ADAQSettings->PearsonPolarity = 1.0;
  else
    ADAQSettings->PearsonPolarity = -1.0;
  
  ADAQSettings->IntegrateRawPearson = IntegrateRawPearson_RB->IsDown();
  ADAQSettings->IntegrateFitToPearson = IntegrateFitToPearson_RB->IsDown();

  ADAQSettings->PearsonLowerLimit = PearsonLowerLimit_NEL->GetEntry()->GetIntNumber();
  ADAQSettings->PearsonMiddleLimit = PearsonMiddleLimit_NEL->GetEntry()->GetIntNumber();
  ADAQSettings->PearsonUpperLimit = PearsonUpperLimit_NEL->GetEntry()->GetIntNumber();

  ADAQSettings->PlotPearsonIntegration = PlotPearsonIntegration_CB->IsDown();  

  ADAQSettings->RFQPulseWidth = RFQPulseWidth_NEL->GetEntry()->GetNumber();
  ADAQSettings->RFQRepRate = RFQRepRate_NEL->GetEntry()->GetIntNumber();
  ADAQSettings->RFQCountRateWaveforms = CountRateWaveforms_NEL->GetEntry()->GetIntNumber();

  ADAQSettings->WaveformsToDesplice = DesplicedWaveformNumber_NEL->GetEntry()->GetIntNumber();
  ADAQSettings->DesplicedWaveformBuffer = DesplicedWaveformBuffer_NEL->GetEntry()->GetIntNumber();
  ADAQSettings->DesplicedWaveformLength = DesplicedWaveformLength_NEL->GetEntry()->GetIntNumber();
  ADAQSettings->DesplicedFileName = DesplicedFileName_TE->GetText();

  
  /////////////////////////////////
  // Values from the "Canvas" frame

  float Min,Max;

  XAxisLimits_THS->GetPosition(Min, Max);
  ADAQSettings->XAxisMin = Min;
  ADAQSettings->XAxisMax = Max;

  YAxisLimits_DVS->GetPosition(Min, Max);
  ADAQSettings->YAxisMin = Min;
  ADAQSettings->YAxisMax = Max;

  SpectrumIntegrationLimits_DHS->GetPosition(Min, Max);
  ADAQSettings->SpectrumIntegrationMin = Min;
  ADAQSettings->SpectrumIntegrationMax = Max;


  ////////////////////////////////////////////////////////
  // Miscellaneous values required for parallel processing
  
  ADAQSettings->ADAQFileName = ADAQFileName;
  
  // Spectrum calibration objects
  ADAQSettings->UseSpectraCalibrations = ComputationMgr->GetUseSpectraCalibrations();
  ADAQSettings->SpectraCalibrations = ComputationMgr->GetSpectraCalibrations();
  
  // PSD filter objects
  ADAQSettings->UsePSDFilters = ComputationMgr->GetUsePSDFilters();
  ADAQSettings->PSDFilters = ComputationMgr->GetPSDFilters();

  // Update the settings object pointer in the manager classes
  ComputationMgr->SetADAQSettings(ADAQSettings);
  GraphicsMgr->SetADAQSettings(ADAQSettings);

  // Write the ADAQSettings object to a ROOT file for parallel access
  if(SaveToFile){
    ADAQSettings->Write("ADAQSettings");
    ADAQSettingsFile->Write();
    ADAQSettingsFile->Close();
  }
  
  delete ADAQSettingsFile;
}


// Creates a separate pop-up box with a message for the user. Function
// is modular to allow flexibility in use.
void AAInterface::CreateMessageBox(string Message, string IconName)
{
  // Choose either a "stop" icon or an "asterisk" icon
  EMsgBoxIcon IconType = kMBIconStop;
  if(IconName == "Asterisk")
    IconType = kMBIconAsterisk;
  
  const int NumTitles = 8;
  
  string BoxTitlesAsterisk[NumTitles] = {"ADAQAnalysis says 'good job!", 
					 "Oh, so you are competent!",
					 "This is a triumph of science!",
					 "Excellent work. You're practically a PhD now.",
					 "For you ARE the Kwisatz Haderach!",
					 "There will be a parade in your honor.",
					 "Oh, well, bra-VO!",
					 "Top notch."};
  
  string BoxTitlesStop[NumTitles] = {"ADAQAnalysis is disappointed in you...", 
				     "Seriously? I'd like another operator, please.",
				     "Unacceptable. Just totally unacceptable.",
				     "That was about as successful as the Hindenburg...",
				     "You blew it!",
				     "Abominable! Off with your head!",
				     "Do, or do not. There is no try.",
				     "You fucked it up, Walter! You always fuck it up!"};
  
  // Surprise the user!
  int RndmInt = RndmMgr->Integer(NumTitles);
  
  string Title = BoxTitlesStop[RndmInt];
  if(IconType==kMBIconAsterisk)
    Title = BoxTitlesAsterisk[RndmInt];

  
  // Create the message box with the desired message and icon
  new TGMsgBox(gClient->GetRoot(), this, Title.c_str(), Message.c_str(), IconType, kMBOk);

}


// Method to update all the relevant widget settings for operating on
// an ADAQ-formatted ROOT file
void AAInterface::UpdateForADAQFile()
{
  // Update widgets with settings from the ADAQ file
  
  int WaveformsInFile = ComputationMgr->GetADAQNumberOfWaveforms();
  ADAQRootMeasParams *AMP = ComputationMgr->GetADAQMeasurementParameters();
  int RecordLength = AMP->RecordLength;
  
  WaveformSelector_HS->SetRange(1, WaveformsInFile);

  WaveformsToHistogram_NEL->GetEntry()->SetLimitValues(1, WaveformsInFile);
  WaveformsToHistogram_NEL->GetEntry()->SetNumber(WaveformsInFile);

  FileName_TE->SetText(ADAQFileName.c_str());
  Waveforms_NEL->SetNumber(WaveformsInFile);
  RecordLength_NEL->SetNumber(RecordLength);
  
  BaselineCalcMin_NEL->GetEntry()->SetLimitValues(0,RecordLength-1);
  BaselineCalcMax_NEL->GetEntry()->SetLimitValues(1,RecordLength);

  // Set the baseline calculation region to the values used during the
  // data acquisition as the default; user may update afterwards
  int Channel = ChannelSelector_CBL->GetComboBox()->GetSelected();

  // Update the waveform trigger level display
  double TriggerLevel = ComputationMgr->GetADAQMeasurementParameters()->TriggerThreshold[Channel];
  TriggerLevel_NEFL->GetEntry()->SetNumber(TriggerLevel);

  BaselineCalcMin_NEL->GetEntry()->SetNumber(AMP->BaselineCalcMin[Channel]);
  BaselineCalcMax_NEL->GetEntry()->SetNumber(AMP->BaselineCalcMax[Channel]);
  
  PearsonLowerLimit_NEL->GetEntry()->SetLimitValues(0, RecordLength-1);
  PearsonLowerLimit_NEL->GetEntry()->SetNumber(0);

  PearsonMiddleLimit_NEL->GetEntry()->SetLimitValues(1, RecordLength-1);
  PearsonMiddleLimit_NEL->GetEntry()->SetNumber(RecordLength/2);

  PearsonUpperLimit_NEL->GetEntry()->SetLimitValues(1, RecordLength);
  PearsonUpperLimit_NEL->GetEntry()->SetNumber(RecordLength);
  
  DesplicedWaveformNumber_NEL->GetEntry()->SetLimitValues(1, WaveformsInFile);
  DesplicedWaveformNumber_NEL->GetEntry()->SetNumber(WaveformsInFile);
  
  PSDWaveforms_NEL->GetEntry()->SetLimitValues(1, WaveformsInFile);
  PSDWaveforms_NEL->GetEntry()->SetNumber(WaveformsInFile);

  IntegratePearson_CB->SetState(kButtonUp);

  // Resetting radio buttons must account for enabling/disabling
  
  if(ADAQSpectrumTypePAS_RB->IsDown()){
    ADAQSpectrumTypePAS_RB->SetEnabled(true);
    ADAQSpectrumTypePAS_RB->SetState(kButtonDown);
  }
  else
    ADAQSpectrumTypePAS_RB->SetEnabled(true);
  
  if(ADAQSpectrumTypePHS_RB->IsDown()){
    ADAQSpectrumTypePHS_RB->SetEnabled(true);
    ADAQSpectrumTypePHS_RB->SetState(kButtonDown);
  }
  else
    ADAQSpectrumTypePHS_RB->SetEnabled(true);
    
   if(ADAQSpectrumIntTypeWW_RB->IsDown()){
    ADAQSpectrumIntTypeWW_RB->SetEnabled(true);
    ADAQSpectrumIntTypeWW_RB->SetState(kButtonDown);
  }
  else
    ADAQSpectrumIntTypeWW_RB->SetEnabled(true);
  
  if(ADAQSpectrumIntTypePF_RB->IsDown()){
    ADAQSpectrumIntTypePF_RB->SetEnabled(true);
    ADAQSpectrumIntTypePF_RB->SetState(kButtonDown);
  }
  else
    ADAQSpectrumIntTypePF_RB->SetEnabled(true);
  
  // Disable all ACRONYM-specific analysis widgets

  ACROSpectrumTypeEnergy_RB->SetEnabled(false);
  ACROSpectrumTypeScintCounted_RB->SetEnabled(false);
  ACROSpectrumTypeScintCreated_RB->SetEnabled(false);

  ACROSpectrumLS_RB->SetState(kButtonDisabled);
  ACROSpectrumES_RB->SetState(kButtonDisabled);
  
  PSDEnable_CB->SetState(kButtonUp);

  // Reenable all ADAQ-specific tab frames

  WaveformOptionsTab_CF->ShowFrame(WaveformOptions_CF);
}


// Method to update all the relevant widget settings for operating on
// an ACRONYM-formatted ROOT file
void AAInterface::UpdateForACRONYMFile()
{
  FileName_TE->SetText(ACRONYMFileName.c_str());
  Waveforms_NEL->SetNumber(0);
  WaveformsToHistogram_NEL->GetEntry()->SetNumber(0);
  RecordLength_NEL->SetNumber(0);
 
  // Waveform frame (disabled)
  WaveformOptionsTab_CF->HideFrame(WaveformOptions_CF);

  // Disable all ADAQ-specific analysis widgets
  ADAQSpectrumTypePAS_RB->SetState(kButtonDisabled);
  ADAQSpectrumTypePHS_RB->SetState(kButtonDisabled);
  ADAQSpectrumIntTypeWW_RB->SetState(kButtonDisabled);
  ADAQSpectrumIntTypePF_RB->SetState(kButtonDisabled);

  if(ACROSpectrumTypeEnergy_RB->IsDown()){
    ACROSpectrumTypeEnergy_RB->SetEnabled(true);
    ACROSpectrumTypeEnergy_RB->SetState(kButtonDown);
  }
  else
    ACROSpectrumTypeEnergy_RB->SetEnabled(true);
    
  if(ACROSpectrumTypeScintCounted_RB->IsDown()){
    ACROSpectrumTypeScintCounted_RB->SetEnabled(true);
    ACROSpectrumTypeScintCounted_RB->SetState(kButtonDown);
  }
  else
    ACROSpectrumTypeScintCounted_RB->SetEnabled(true);
   
  if(ACROSpectrumTypeScintCreated_RB->IsDown()){
    ACROSpectrumTypeScintCreated_RB->SetEnabled(true);
    ACROSpectrumTypeScintCreated_RB->SetState(kButtonDown);
  }
  else
    ACROSpectrumTypeScintCreated_RB->SetEnabled(true);

  if(ACROSpectrumLS_RB->IsDown()){
    ACROSpectrumLS_RB->SetEnabled(true);
    ACROSpectrumLS_RB->SetState(kButtonDown);
  }
  else
    ACROSpectrumLS_RB->SetEnabled(true);

  if(ACROSpectrumES_RB->IsDown()){
    ACROSpectrumES_RB->SetEnabled(true);
    ACROSpectrumES_RB->SetState(kButtonDown);
  }
  else
    ACROSpectrumES_RB->SetEnabled(true);

  if(ACROSpectrumLS_RB->IsDown()){
    WaveformsToHistogram_NEL->GetEntry()->SetLimitValues(1, ComputationMgr->GetACRONYMLSEntries() );
    WaveformsToHistogram_NEL->GetEntry()->SetNumber( ComputationMgr->GetACRONYMLSEntries() );
  }
  else if (ACROSpectrumES_RB->IsDown()){
    WaveformsToHistogram_NEL->GetEntry()->SetLimitValues(1, ComputationMgr->GetACRONYMESEntries() );
    WaveformsToHistogram_NEL->GetEntry()->SetNumber( ComputationMgr->GetACRONYMESEntries() );
  }

  PSDEnable_CB->SetState(kButtonDisabled);
  SetPearsonWidgetState(false, kButtonDisabled);

  // Processing frame
  NumProcessors_NEL->GetEntry()->SetState(false);      
  UpdateFreq_NEL->GetEntry()->SetState(false);
  
  OptionsTabs_T->SetTab("Spectrum");
}


void AAInterface::SetPeakFindingWidgetState(bool WidgetState, EButtonState ButtonState)
{
  UseMarkovSmoothing_CB->SetState(ButtonState);
  MaxPeaks_NEL->GetEntry()->SetState(WidgetState);
  Sigma_NEL->GetEntry()->SetState(WidgetState);
  Resolution_NEL->GetEntry()->SetState(WidgetState);
  Floor_NEL->GetEntry()->SetState(WidgetState);
  PlotFloor_CB->SetState(ButtonState);
  PlotCrossings_CB->SetState(ButtonState);
  PlotPeakIntegratingRegion_CB->SetState(ButtonState);
}


void AAInterface::SetPSDWidgetState(bool WidgetState, EButtonState ButtonState)
{
  PSDChannel_CBL->GetComboBox()->SetEnabled(WidgetState);
  PSDWaveforms_NEL->GetEntry()->SetState(WidgetState);
  PSDNumTailBins_NEL->GetEntry()->SetState(WidgetState);
  PSDMinTailBin_NEL->GetEntry()->SetState(WidgetState);
  PSDMaxTailBin_NEL->GetEntry()->SetState(WidgetState);
  PSDNumTotalBins_NEL->GetEntry()->SetState(WidgetState);
  PSDMinTotalBin_NEL->GetEntry()->SetState(WidgetState);
  PSDMaxTotalBin_NEL->GetEntry()->SetState(WidgetState);
  PSDThreshold_NEL->GetEntry()->SetState(WidgetState);
  PSDPeakOffset_NEL->GetEntry()->SetState(WidgetState);
  PSDTailOffset_NEL->GetEntry()->SetState(WidgetState);
  PSDPlotType_CBL->GetComboBox()->SetEnabled(WidgetState);
  PSDPlotTailIntegration_CB->SetState(ButtonState);
  PSDEnableHistogramSlicing_CB->SetState(ButtonState);
  PSDEnableFilterCreation_CB->SetState(ButtonState);
  PSDEnableFilter_CB->SetState(ButtonState);
  PSDPositiveFilter_RB->SetState(ButtonState);
  PSDNegativeFilter_RB->SetState(ButtonState);
  PSDClearFilter_TB->SetState(ButtonState);
  PSDCalculate_TB->SetState(ButtonState);
}


void AAInterface::SetPearsonWidgetState(bool WidgetState, EButtonState ButtonState)
{
  IntegratePearson_CB->SetState(ButtonState);
  PearsonChannel_CBL->GetComboBox()->SetEnabled(WidgetState);
  PearsonPolarityPositive_RB->SetState(ButtonState);
  PearsonPolarityNegative_RB->SetState(ButtonState);
  PlotPearsonIntegration_CB->SetState(ButtonState);
  IntegrateRawPearson_RB->SetState(ButtonState);
  IntegrateFitToPearson_RB->SetState(ButtonState);
  PearsonLowerLimit_NEL->GetEntry()->SetState(WidgetState);
  PearsonMiddleLimit_NEL->GetEntry()->SetState(WidgetState);
  PearsonUpperLimit_NEL->GetEntry()->SetState(WidgetState);
}


void AAInterface::SetCalibrationWidgetState(bool WidgetState, EButtonState ButtonState)
{
  SpectrumCalibrationLoad_TB->SetState(ButtonState);
  SpectrumCalibrationStandard_RB->SetState(ButtonState);
  SpectrumCalibrationEdgeFinder_RB->SetState(ButtonState);
  SpectrumCalibrationPoint_CBL->GetComboBox()->SetEnabled(WidgetState);
  SpectrumCalibrationUnit_CBL->GetComboBox()->SetEnabled(WidgetState);
  SpectrumCalibrationEnergy_NEL->GetEntry()->SetState(WidgetState);
  SpectrumCalibrationPulseUnit_NEL->GetEntry()->SetState(WidgetState);
  SpectrumCalibrationSetPoint_TB->SetState(ButtonState);
  SpectrumCalibrationCalibrate_TB->SetState(ButtonState);
  SpectrumCalibrationPlot_TB->SetState(ButtonState);
  SpectrumCalibrationReset_TB->SetState(ButtonState);
}


void AAInterface::SetEAGammaWidgetState(bool WidgetState, EButtonState ButtonState)
{
  EAGammaEDep_NEL->GetEntry()->SetState(WidgetState);
  EAEscapePeaks_CB->SetState(ButtonState);
}


void AAInterface::SetEANeutronWidgetState(bool WidgetState, EButtonState ButtonState)
{
  EALightConversionFactor_NEL->GetEntry()->SetState(WidgetState);
  EAErrorWidth_NEL->GetEntry()->SetState(WidgetState);
  EAElectronEnergy_NEL->GetEntry()->SetState(WidgetState);
  EAGammaEnergy_NEL->GetEntry()->SetState(WidgetState);
  EAProtonEnergy_NEL->GetEntry()->SetState(WidgetState);
  EAAlphaEnergy_NEL->GetEntry()->SetState(WidgetState);
  EACarbonEnergy_NEL->GetEntry()->SetState(WidgetState);
}

