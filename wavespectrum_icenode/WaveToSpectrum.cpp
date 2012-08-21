// WaveToSpectrum Plugin
// Takes a Wave File (stereo 44khz) and do a spectrum decomposition.
//
// Initial code generated by Softimage SDK Wizard
// 
// Claude Vervoort, http://claudeonthe.net 
// 
//s#define _HAS_ITERATOR_DEBUGGING 1

#include <xsi_application.h>
#include <xsi_context.h>
#include <xsi_pluginregistrar.h>
#include <xsi_status.h>

#include <xsi_icenodecontext.h>
#include <xsi_icenodedef.h>
#include <xsi_command.h>
#include <xsi_factory.h>
#include <xsi_longarray.h>
#include <xsi_doublearray.h>
#include <xsi_math.h>
#include <xsi_vector2f.h>
#include <xsi_vector3f.h>
#include <xsi_vector4f.h>
#include <xsi_matrix3f.h>
#include <xsi_matrix4f.h>
#include <xsi_rotationf.h>
#include <xsi_quaternionf.h>
#include <xsi_color4f.h>
#include <xsi_shape.h>
#include <xsi_icegeometry.h>
#include <xsi_iceportstate.h>
#include <xsi_indexset.h>
#include <xsi_dataarray.h>
#include <xsi_dataarray2D.h>

#include "WaveSpectrumAnalyzer.h"

// For storing CSampleData user data objects
#include <vector>

using namespace XSI; 
using namespace FFTWave;

// Simple user data struct

struct SpectrumUserData
{
  SpectrumUserData( ): waveLoader( NULL ), logAnalyzer( NULL ), linearAnalyzer( NULL ) {};

  ~SpectrumUserData( )
  {
    reset( );
  }

  void reset( )
  {
    if ( NULL != waveLoader )    delete waveLoader;
    if ( NULL != logAnalyzer )   delete logAnalyzer;
    if ( NULL != linearAnalyzer) delete linearAnalyzer;
    waveLoader = NULL;
    logAnalyzer = NULL;
    linearAnalyzer = NULL;
  }

  BOOL init( CString filepath )
  {
    if ( NULL == waveLoader || !waveLoader->isLoadedFile( filepath.GetWideString( ) ) )
    {
      reset( );
      waveLoader = new WaveLoader( );
      if ( !waveLoader->load( filepath.GetWideString( ) ) )
      {
        reset( );
        return false;
      }
    }
    return true;
  }

  WaveLoader* waveLoader; // shared wav data, loaded only once during the simulation (unless modified during the sim flow)
  WaveSpectrumAnalyzer* logAnalyzer; // used when the log output is connected
  WaveSpectrumAnalyzer* linearAnalyzer; // used when the linear output is connected
};

// Defines port, group and map identifiers used for registering the ICENode
enum IDs
{
	ID_IN_filepath = 0,
	ID_IN_channel = 1,
	ID_IN_time = 2,
	ID_IN_frequency = 3,
	ID_G_100 = 100,
	ID_G_200 = 200,
	ID_OUT_linearBands = 200,
	ID_OUT_logBands = 201,
	ID_OUT_linear = 202,
	ID_OUT_log = 203,
	ID_TYPE_CNS = 400,
	ID_STRUCT_CNS,
	ID_CTXT_CNS=800,
  ID_CTXT_CNS_ANY=801,
	ID_UNDEF = ULONG_MAX
};

XSI::CStatus RegisterWaveToSpectrum( XSI::PluginRegistrar& in_reg );



SICALLBACK XSILoadPlugin( PluginRegistrar& in_reg )
{
	in_reg.PutAuthor(L"Claude Vervoort");
	in_reg.PutName(L"WaveToSpectrum Plugin");
	in_reg.PutVersion(1,0);

	RegisterWaveToSpectrum( in_reg );

	//RegistrationInsertionPoint - do not remove this line

	return CStatus::OK;
}

SICALLBACK XSIUnloadPlugin( const PluginRegistrar& in_reg )
{
	CString strPluginName;
	strPluginName = in_reg.GetName();
	Application().LogMessage(strPluginName + L" has been unloaded.",siVerboseMsg);
	return CStatus::OK;
}

CStatus RegisterWaveToSpectrum( PluginRegistrar& in_reg )
{
	ICENodeDef nodeDef;
	nodeDef = Application().GetFactory().CreateICENodeDef(L"WaveToSpectrum",L"WaveToSpectrum");

	CStatus st;
	st = nodeDef.PutColor(154,188,102);
	st.AssertSucceeded( ) ;

	st = nodeDef.PutThreadingModel(XSI::siICENodeMultiEvaluationPhase);
	st.AssertSucceeded( ) ;

	// Add input ports and groups.
	st = nodeDef.AddPortGroup(ID_G_100);
	st.AssertSucceeded( ) ;

	st = nodeDef.AddInputPort(ID_IN_filepath,ID_G_100,siICENodeDataString,siICENodeStructureSingle,siICENodeContextSingleton,L"filepath",L"filepath",L"default string",CValue(),CValue(),ID_UNDEF,ID_UNDEF,ID_CTXT_CNS);
	st.AssertSucceeded( ) ;

	st = nodeDef.AddInputPort(ID_IN_channel,ID_G_100,siICENodeDataLong,siICENodeStructureSingle,siICENodeContextSingleton,L"channel",L"channel",0,CValue(),CValue(),ID_UNDEF,ID_UNDEF,ID_CTXT_CNS);
	st.AssertSucceeded( ) ;

	st = nodeDef.AddInputPort(ID_IN_time,ID_G_100,siICENodeDataFloat,siICENodeStructureSingle,siICENodeContextSingleton,L"time",L"time",0,CValue(),CValue(),ID_UNDEF,ID_UNDEF,ID_CTXT_CNS);
	st.AssertSucceeded( ) ;

  st = nodeDef.AddInputPort(ID_IN_frequency,ID_G_100,siICENodeDataFloat,siICENodeStructureSingle,siICENodeContextAny,L"frequency",L"frequency",0,CValue(),CValue(),ID_UNDEF,ID_UNDEF,ID_CTXT_CNS_ANY);
	st.AssertSucceeded( ) ;

	// Add output ports.
	st = nodeDef.AddOutputPort(ID_OUT_linearBands,siICENodeDataFloat,siICENodeStructureArray,siICENodeContextSingleton,L"linearBands",L"linearBands",ID_UNDEF,ID_UNDEF,ID_CTXT_CNS);
	st.AssertSucceeded( ) ;

	st = nodeDef.AddOutputPort(ID_OUT_logBands,siICENodeDataFloat,siICENodeStructureArray,siICENodeContextSingleton,L"logBands",L"logBands",ID_UNDEF,ID_UNDEF,ID_CTXT_CNS);
	st.AssertSucceeded( ) ;

  st = nodeDef.AddOutputPort(ID_OUT_linear,siICENodeDataFloat,siICENodeStructureSingle,siICENodeContextAny,L"linear",L"linear",ID_UNDEF,ID_UNDEF,ID_CTXT_CNS_ANY);
	st.AssertSucceeded( ) ;

  st = nodeDef.AddOutputPort(ID_OUT_log,siICENodeDataFloat,siICENodeStructureSingle,siICENodeContextAny,L"log",L"log",ID_UNDEF,ID_UNDEF,ID_CTXT_CNS_ANY);
	st.AssertSucceeded( ) ;

	PluginItem nodeItem = in_reg.RegisterICENode(nodeDef);
	nodeItem.PutCategories(L"Custom ICENode");

	return CStatus::OK;
}

/*
 * Called when the node instance is initiated at the beginning of the
 * implementation. We initiate a sharead variable here so that the Wave
 * data is only loaded once unless the file path is changed
 */
SICALLBACK WaveToSpectrum_Init( CRef& in_ctxt )
{
	Context ctxt( in_ctxt );
  SpectrumUserData* userData = new SpectrumUserData( );
  ctxt.PutUserData( (CValue::siPtrType)userData );
	return CStatus::OK;
}

/*
 * Called when the node is discarded, so we are going to free any
 * structure we have initialized during the simulation
 */
SICALLBACK WaveToSpectrum_Term( CRef& in_ctxt )
{
	Context ctxt( in_ctxt );
  SpectrumUserData* userData = ( SpectrumUserData* ) ( CValue::siPtrType ) ctxt.GetUserData( );
  delete userData;
	return CStatus::OK;
}

XSIPLUGINCALLBACK CStatus WaveToSpectrum_SubmitEvaluationPhaseInfo( ICENodeContext& in_ctxt )
{
	ULONG nPhase = in_ctxt.GetEvaluationPhaseIndex( );
	switch( nPhase )
	{
		case 0:
		{
      // Phase 0 single thread: we set up the wave file and compute the FFT
			in_ctxt.AddEvaluationPhaseInputPort( ID_IN_filepath );
			in_ctxt.AddEvaluationPhaseInputPort( ID_IN_channel );
			in_ctxt.AddEvaluationPhaseInputPort( ID_IN_time );
		}
		break;
		
		case 1:
		{
			in_ctxt.AddEvaluationPhaseInputPort( ID_IN_frequency );
			// This phase is the last one. All ports specified for phase 1 will be evaluated in multi-threaded batches.
			in_ctxt.SetLastEvaluationPhase();
		}
		break;
	}
	return CStatus::OK;
}

// we compute the FFT in the single threaded context since the time is Object instance so all
// particles share the same FFT (however they get the value at different frequency since frequency
// is per point context)
SICALLBACK WaveToSpectrum_ComputeFFT( ICENodeContext& in_ctxt )
{
  SpectrumUserData* userData = ( SpectrumUserData* ) ( CValue::siPtrType ) in_ctxt.GetUserData( );
  CDataArrayString filepathData( in_ctxt, ID_IN_filepath );
  if ( filepathData.GetCount( ) > 0 )
  {
    CString filepath;
    filepathData.GetData( 0, filepath );
    if ( userData->init( filepath ) )// will reset loaders and analyzers if file path changes
    {
      // So we do have Wave Data, let's compute the FFT and the spectrum bands for the current time/channel
      ULONG out_portID = in_ctxt.GetEvaluatedOutputPortID( );
      bool linear = ( out_portID == ID_OUT_linear ) || ( out_portID == ID_OUT_linearBands );
      WaveSpectrumAnalyzer* spectrumAnalyzer = linear?userData->linearAnalyzer:userData->logAnalyzer;
      CDataArrayLong channelData( in_ctxt, ID_IN_channel );
      CDataArrayFloat timeData( in_ctxt, ID_IN_time );
      CIndexSet indexSet( in_ctxt );
      // Should be unique since Singleton context
      for(CIndexSet::Iterator it = indexSet.Begin(); it.HasNext(); it.Next())
      {
        int channel = static_cast<int>( channelData[ it ] );
        float time =  timeData[ it ];
        if ( time <0 || channel > 1 || channel <-1 )
        {
            Application( ).LogMessage( "Incorrect parameters time[ " + CString( time ) + "] channelData (-1,0,1) [ " + CString(channel) + "]"  );
             return CStatus::InvalidArgument;
        }
        if ( NULL == spectrumAnalyzer )
        {
            spectrumAnalyzer = new WaveSpectrumAnalyzer( userData->waveLoader );
            if ( linear )
            {
              spectrumAnalyzer->setLinear( );
              userData->linearAnalyzer = spectrumAnalyzer;
            }
            else
            {
              spectrumAnalyzer->setLog( );
              userData->logAnalyzer = spectrumAnalyzer;
            }
            // we find the peak so that output are somewhat normalized
            spectrumAnalyzer->findPeak( 1040, 2000 /* every 1000 samples */, channel  ); // find the average peak and uses it to scale values
            spectrumAnalyzer->setTransformFinalValueFunction( &FFTWave::max1Compute ); // 1 - exp ( -2 * v ) so that values never go above 1
        }
        int block =  userData->waveLoader->toBlockFromTime( timeData[ it ] );
        spectrumAnalyzer->getFFTBands(block, channel ) ;
      }
      return CStatus::OK;
    }
  }
  // Is there a better way to indicate an error?
  Application( ).LogMessage( "File could not be loaded" );
  return CStatus::InvalidArgument;
}

SICALLBACK WaveToSpectrum_Evaluate( ICENodeContext& in_ctxt )
{

  ULONG nPhase = in_ctxt.GetEvaluationPhaseIndex( );
  if ( 0 == nPhase )
  {
    return WaveToSpectrum_ComputeFFT( in_ctxt );
  }
	// Get the user data that we allocated in BeginEvaluate
  SpectrumUserData* userData = ( SpectrumUserData* ) ( CValue::siPtrType ) in_ctxt.GetUserData( );

	// The current output port being evaluated...
	ULONG out_portID = in_ctxt.GetEvaluatedOutputPortID( );
  bool allBands = (ID_OUT_linearBands == out_portID ) || (  ID_OUT_logBands == out_portID );
  bool linear = ( out_portID == ID_OUT_linear ) || ( out_portID == ID_OUT_linearBands );
  WaveSpectrumAnalyzer* spectrumAnalyzer = linear?userData->linearAnalyzer:userData->logAnalyzer;
  CIndexSet indexSet( in_ctxt );
  if ( allBands )
  {
    CDataArray2DFloat outData( in_ctxt );
    for(CIndexSet::Iterator it = indexSet.Begin(); it.HasNext(); it.Next())
    {
      float* bands = spectrumAnalyzer->getFFTBands( ) ;
      CDataArray2DFloat::Accessor outAccessor = outData.Resize( it, spectrumAnalyzer->getNumberOfBands( ) );
	    for (int i=0; i<spectrumAnalyzer->getNumberOfBands( ); ++i )
	    {	
		    outAccessor[i] = ( NULL == bands )?0:bands[i];
	    }
    }
  }
  else
  {
    // frequency should be a normalized value 0-1, and what is returned is a linear interpolation
    // of the value at that frequency location.
    CDataArrayFloat outData( in_ctxt );
    CDataArrayFloat frequency( in_ctxt, ID_IN_frequency );
    for(CIndexSet::Iterator it = indexSet.Begin(); it.HasNext(); it.Next())
    {
      float* bands = spectrumAnalyzer->getFFTBands( ) ;
      // find the upper and lower bands then linear interpolation
      float bandIndex = frequency[ it ] * spectrumAnalyzer->getNumberOfBands( );
      bandIndex = ( bandIndex < 0 )?0:bandIndex;
      int lowerIndex = ( int ) bandIndex;
      if ( lowerIndex + 1 >= spectrumAnalyzer->getNumberOfBands( ) )
      {
        outData[ it ] = bands[ spectrumAnalyzer->getNumberOfBands( ) - 1 ];
      }
      else
      {
        float interp = bandIndex - lowerIndex;
        outData[ it ] = bands[ lowerIndex ] * ( 1 - interp ) + bands[ lowerIndex + 1 ] * interp; 
      }
    }
  }
  return CStatus::OK;
}

/*
 * Begin Evaluate is called in a single threaded environment and is used to set
 * up the loader
 */
SICALLBACK WaveToSpectrum_BeginEvaluate( ICENodeContext& in_ctxt )
{
	return CStatus::OK;
}

SICALLBACK WaveToSpectrum_EndEvaluate( ICENodeContext& in_ctxt )
{
	// We do not release any memory here since the wave file might be needed on the next frame
	return CStatus::OK;
}

