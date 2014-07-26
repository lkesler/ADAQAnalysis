//////////////////////////////////////////////////////////////////////////////////////////
// 
// name: ADAQAnalysis.cc
// date: 21 May 13
// auth: Zach Hartwig
// 
// desc:
//
//////////////////////////////////////////////////////////////////////////////////////////

#include <TApplication.h>

#include <iostream>
#include <string>

#ifdef MPI_ENABLED
#include <mpi.h>
#endif

#include "AAInterface.hh"
#include "AAComputation.hh"
#include "AAParallel.hh"
#include "AAGraphics.hh"

int main(int argc, char *argv[])
{
  // Assign the binary architecture (sequential or parallel) to a bool
  // that will be used frequently in the code to determine arch. type
  bool ParallelArchitecture = false;

  // Create a manager to handle parallel processing
  AAParallel *TheParallel = new AAParallel;

#ifdef MPI_ENABLED
  // Initialize the MPI session
  TheParallel->Initialize(argc, argv);
  ParallelArchitecture = true;
#endif
  
  // A word on the use of the first cmd line arg: the first cmd line
  // arg is used in different ways depending on the binary
  // architecture. For sequential arch, the user may specify a valid
  // ADAQ ROOT file as the first cmd line arg to open automatically
  // upon launching the ADAQAnalysis binary. For parallel arch, the
  // automatic call to the parallel binaries made by ADAQAnalysisGUI
  // contains the type of processing to be performed in parallel as
  // the first cmd line arg, which, at present, is "histogramming" for
  // spectra creation and "desplicing" for despliced file creation.
  
  // Get the zeroth command line arg (the binary name)
  string CmdLineBin = argv[0];
  
  // Get the first command line argument 
  string CmdLineArg;

  // If no first cmd line arg was specified then we will start the
  // analysis with no ADAQ ROOT file loaded via "unspecified" string
  if(argc==1)
    CmdLineArg = "Unspecified";
  
  // If only 1 cmd line arg was specified then assign it to
  // corresponding string that will be passed to the
  // ADAQComputation class constructor for use
  else if(argc==2)
    CmdLineArg = argv[1];

  // Disallow any other combination of cmd line args and notify the
  // user about his/her mistake appropriately (arch. specific)
  else{
    if(CmdLineBin == "ADAQAnalysis"){
      cout << "\nError! Unspecified command line arguments to ADAQAnalysis!\n"
	   <<   "       Usage: ADAQAnalysis </path/to/filename>\n"
	   << endl;
      exit(-42);
    }
    else{
      cout << "\nError! Unspecified command line arguments to ADAQAnalysis_MPI!\n"
	   <<   "       Usage: ADAQAnalysis <WaveformAnalysisType: {histogramming, desplicing, discriminating}\n"
	   << endl;
      exit(-42);
    }
  }    
  
  // Run ROOT in standalone mode
  TApplication *TheApplication = new TApplication("ADAQAnalysis", &argc, argv);
  
  // Create the singleton analysis manager
  AAComputation *TheComputation = new AAComputation(CmdLineArg, ParallelArchitecture);
  
  if(!ParallelArchitecture){
    // Create the singleton graphics manager
    AAGraphics *TheGraphics = new AAGraphics;

    // Create the singletone interpolation manager
    AAInterpolation *TheInterpolation = new AAInterpolation;
    
    // Create the graphical user interface
    AAInterface *TheInterface = new AAInterface(CmdLineArg);
    TheInterface->Connect("CloseWindow()", "AAInterface", TheInterface, "HandleTerminate()");
    
    // Run the application
    TheApplication->Run();
    
    delete TheInterface;

    delete TheGraphics;
  }

  delete TheComputation;

  delete TheApplication;
  
  // Finalize the MPI session
#ifdef MPI_ENABLED
  TheParallel->Finalize();
#endif  

  delete TheParallel;
  
  return 0;
}
