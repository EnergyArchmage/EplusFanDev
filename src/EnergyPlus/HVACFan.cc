// EnergyPlus, Copyright (c) 1996-2016, The Board of Trustees of the University of Illinois and
// The Regents of the University of California, through Lawrence Berkeley National Laboratory
// (subject to receipt of any required approvals from the U.S. Dept. of Energy). All rights
// reserved.
//
// If you have questions about your rights to use or distribute this software, please contact
// Berkeley Lab's Innovation & Partnerships Office at IPO@lbl.gov.
//
// NOTICE: This Software was developed under funding from the U.S. Department of Energy and the
// U.S. Government consequently retains certain rights. As such, the U.S. Government has been
// granted for itself and others acting on its behalf a paid-up, nonexclusive, irrevocable,
// worldwide license in the Software to reproduce, distribute copies to the public, prepare
// derivative works, and perform publicly and display publicly, and to permit others to do so.
//
// Redistribution and use in source and binary forms, with or without modification, are permitted
// provided that the following conditions are met:
//
// (1) Redistributions of source code must retain the above copyright notice, this list of
//     conditions and the following disclaimer.
//
// (2) Redistributions in binary form must reproduce the above copyright notice, this list of
//     conditions and the following disclaimer in the documentation and/or other materials
//     provided with the distribution.
//
// (3) Neither the name of the University of California, Lawrence Berkeley National Laboratory,
//     the University of Illinois, U.S. Dept. of Energy nor the names of its contributors may be
//     used to endorse or promote products derived from this software without specific prior
//     written permission.
//
// (4) Use of EnergyPlus(TM) Name. If Licensee (i) distributes the software in stand-alone form
//     without changes from the version obtained under this License, or (ii) Licensee makes a
//     reference solely to the software portion of its product, Licensee must refer to the
//     software as "EnergyPlus version X" software, where "X" is the version number Licensee
//     obtained under this License and may not use a different name for the software. Except as
//     specifically required in this Section (4), Licensee shall not use in a company name, a
//     product name, in advertising, publicity, or other promotional activities any name, trade
//     name, trademark, logo, or other designation of "EnergyPlus", "E+", "e+" or confusingly
//     similar designation, without Lawrence Berkeley National Laboratory's prior written consent.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// You are under no obligation whatsoever to provide any bug fixes, patches, or upgrades to the
// features, functionality or performance of the source code ("Enhancements") to anyone; however,
// if you choose to make your Enhancements available either publicly, or directly to Lawrence
// Berkeley National Laboratory, without imposing a separate written license agreement for such
// Enhancements, then you hereby grant the following license: a non-exclusive, royalty-free
// perpetual license to install, use, modify, prepare derivative works, incorporate into other
// computer software, distribute, and sublicense such enhancements or derivative works thereof,
// in binary and source code form.

// EnergyPlus Headers
#include <HVACFan.hh>
#include <EnergyPlus.hh>
#include <DataGlobals.hh>
#include <DataHVACGlobals.hh>
#include <InputProcessor.hh>
#include <DataIPShortCuts.hh>
#include <ScheduleManager.hh>
#include <NodeInputManager.hh>
#include <DataLoopNode.hh>
#include <DataSizing.hh>
#include <CurveManager.hh>
#include <DataHeatBalance.hh>
#include <OutputProcessor.hh>
#include <General.hh>
#include <EMSManager.hh>
#include <ObjexxFCL/Optional.hh>
#include <DataAirLoop.hh>
#include <DataEnvironment.hh>
#include <ReportSizingManager.hh>
#include <OutputReportPredefined.hh>
#include <ReportSizingManager.hh>
#include <Psychrometrics.hh>
#include <DataContaminantBalance.hh>
#include <DataPrecisionGlobals.hh>
#include <BranchNodeConnections.hh>

namespace EnergyPlus {

namespace HVACFan {

	std::vector < std::unique_ptr <FanSystem> > fanObjs;

	int
	getFanObjectVectorIndex(  // lookup vector index for fan object name in object array EnergyPlus::HVACFan::fanObjs
		std::string const objectName  // IDF name in input
	)
	{
		int index = -1;
		bool found = false;
		for ( std::size_t loop = 0; loop < fanObjs.size(); ++loop ) {
			if ( objectName == fanObjs[ loop ]->name() ) {
				if ( ! found ) {
					index = loop;
					found = true;
				} else { // found duplicate
					//TODO throw warning?  
					index = -1;
				}
			}
		}
		return index;
	}

	bool
	checkIfFanNameIsAFanSystem( // look up to see if input contains a Fan:SystemModel with the name (for use before object construction
		std::string const objectName
	) {
	
		int testNum = InputProcessor::GetObjectItemNum("Fan:SystemModel", objectName );
		if ( testNum > 0 ) {
			return true;
		} else {
			return false;
		}
	
	}

	void
	FanSystem::simulate(
//		bool const firstHVACIteration,
		Optional< Real64 const > flowFraction,
		Optional_bool_const zoneCompTurnFansOn, // Turn fans ON signal from ZoneHVAC component
		Optional_bool_const zoneCompTurnFansOff, // Turn Fans OFF signal from ZoneHVAC component
		Optional< Real64 const > pressureRise // Pressure difference to use for DeltaPress, for rating DX coils without entire duct system
	)
	{

		this->objTurnFansOn_ = false;
		this->objTurnFansOff_ = false;

		this->init( );

		if ( present( zoneCompTurnFansOn ) && present( zoneCompTurnFansOff ) ) {
			// Set module-level logic flags equal to ZoneCompTurnFansOn and ZoneCompTurnFansOff values passed into this routine
			// for ZoneHVAC components with system availability managers defined.
			// The module-level flags get used in the other subroutines (e.g., SimSimpleFan,SimVariableVolumeFan and SimOnOffFan)
			this->objTurnFansOn_ = zoneCompTurnFansOn;
			this->objTurnFansOff_ = zoneCompTurnFansOff;
		} else {
			// Set module-level logic flags equal to the global LocalTurnFansOn and LocalTurnFansOff variables for all other cases.
			this->objTurnFansOn_ = DataHVACGlobals::TurnFansOn;
			this->objTurnFansOff_ = DataHVACGlobals::TurnFansOff;
		}
		if ( present( pressureRise ) &&  present( flowFraction ) ) {
			this->calcSimpleSystemFan( flowFraction, pressureRise );
		} else if ( present( pressureRise ) && ! present( flowFraction ) ){
			this->calcSimpleSystemFan( _, pressureRise );
		} else if ( ! present( pressureRise ) &&  present( flowFraction ) ) {
			this->calcSimpleSystemFan( flowFraction, _ );
		} else {
			this->calcSimpleSystemFan( _ , _ );
		}

		this->update();

		this->report();

	}

	void
	FanSystem::init()
	{ 
		if ( ! DataGlobals::SysSizingCalc && this->objSizingFlag_ ) {
			this->set_size();
			this->objSizingFlag_ = false;
			if ( DataSizing::CurSysNum > 0 ) {
				DataAirLoop::AirLoopControlInfo( DataSizing::CurSysNum ).CyclingFan = true;
			}
		}

		if ( DataGlobals::BeginEnvrnFlag && this->objEnvrnFlag_ ) {
			this->rhoAirStdInit_ = DataEnvironment::StdRhoAir;
			this->maxAirMassFlowRate_ = this->designAirVolFlowRate_ * this->rhoAirStdInit_;
			this->minAirFlowRate_ = this->designAirVolFlowRate_ * this->minPowerFlowFrac_;
			this->minAirMassFlowRate_ = this->minAirFlowRate_ * this->rhoAirStdInit_;

//			if ( Fan( FanNum ).NVPerfNum > 0 ) {
//				NightVentPerf( Fan( FanNum ).NVPerfNum ).MaxAirMassFlowRate = NightVentPerf( Fan( FanNum ).NVPerfNum ).MaxAirFlowRate * Fan( FanNum ).RhoAirStdInit;
//			}

			//Init the Node Control variables
			DataLoopNode::Node( this->outletNodeNum_ ).MassFlowRateMax = this->maxAirMassFlowRate_;
			DataLoopNode::Node( this->outletNodeNum_ ).MassFlowRateMin =this->minAirMassFlowRate_;

			//Initialize all report variables to a known state at beginning of simulation
			this->fanPower_ = 0.0;
			this->deltaTemp_ = 0.0;
			this->fanEnergy_ = 0.0;
			for ( auto loop = 0; loop < this->numSpeeds_; ++loop ) {
				this->fanRunTimeFractionAtSpeed_[ loop ] = 0.0;
			}
			this->objEnvrnFlag_ =  false;
		}

		if ( ! DataGlobals::BeginEnvrnFlag ) {
			this->objEnvrnFlag_ = true;
		}

		this->massFlowRateMaxAvail_ = min( DataLoopNode::Node( this->outletNodeNum_ ).MassFlowRateMax, DataLoopNode::Node( this->inletNodeNum_ ).MassFlowRateMaxAvail );
		this->massFlowRateMinAvail_ = min( max( DataLoopNode::Node( this->outletNodeNum_ ).MassFlowRateMin, DataLoopNode::Node( this->inletNodeNum_ ).MassFlowRateMinAvail ), DataLoopNode::Node( this->inletNodeNum_ ).MassFlowRateMaxAvail );

		// Load the node data in this section for the component simulation
		//First need to make sure that the MassFlowRate is between the max and min avail.
		this->inletAirMassFlowRate_ = min( DataLoopNode::Node( this->inletNodeNum_ ).MassFlowRate, this->massFlowRateMaxAvail_ );
		this->inletAirMassFlowRate_ = max( this->inletAirMassFlowRate_, this->massFlowRateMinAvail_ );

		//Then set the other conditions
		this->inletAirTemp_     = DataLoopNode::Node( this->inletNodeNum_ ).Temp;
		this->inletAirHumRat_   = DataLoopNode::Node( this->inletNodeNum_ ).HumRat;
		this->inletAirEnthalpy_ = DataLoopNode::Node( this->inletNodeNum_ ).Enthalpy;
	}

	void
	FanSystem::set_size()
	{
		std::string const routineName = "HVACFan::set_size ";
	
		Real64 tempFlow = this->designAirVolFlowRate_;
		bool bPRINT = true;
		DataSizing::DataAutosizable = true;
		DataSizing::DataEMSOverrideON = this->maxAirFlowRateEMSOverrideOn_;
		DataSizing::DataEMSOverride   = this->maxAirFlowRateEMSOverrideValue_;
		ReportSizingManager::RequestSizing(this->fanType_, this->name_, DataHVACGlobals::SystemAirflowSizing, "Design Maximum Air Flow Rate [m3/s]", tempFlow, bPRINT, routineName );
		this->designAirVolFlowRate_    = tempFlow;
		DataSizing::DataAutosizable   = true;
		DataSizing::DataEMSOverrideON = false;
		DataSizing::DataEMSOverride   = 0.0;


		if ( this->designElecPowerWasAutosized_ ) {

			switch ( this->powerSizingMethod_ )
			{
		
			case powerPerFlow: {
				this->designElecPower_ = this->designAirVolFlowRate_ * this->elecPowerPerFlowRate_;
				break;
			}
			case powerPerFlowPerPressure: {
				this->designElecPower_ = this->designAirVolFlowRate_ * this->deltaPress_ * this->elecPowerPerFlowRatePerPressure_;
				break;
			}
			case totalEfficiencyAndPressure: {
				this->designElecPower_ = this->designAirVolFlowRate_ * this->deltaPress_ / this->fanTotalEff_;
				break;
			}
		
			} // end switch

			//report design power
			ReportSizingManager::ReportSizingOutput( this->fanType_, this->name_, "Design Electric Power Consumption [W]", this->designElecPower_ );
		
		} // end if power was autosized

		//calculate total fan system efficiency at design
		this->fanTotalEff_ = this->designAirVolFlowRate_ * this->deltaPress_ / this->designElecPower_;

		if (this->numSpeeds_ > 1 ) { // set up values at speeds
			this->massFlowAtSpeed_.resize( this->numSpeeds_, 0.0 );
			this->totEfficAtSpeed_.resize( this->numSpeeds_, 0.0 );
			for ( auto loop=0; loop < this->numSpeeds_; ++loop ) {
				this->massFlowAtSpeed_[ loop ] = this->maxAirMassFlowRate_ * this->flowFractionAtSpeed_[ loop ];
				if ( this->powerFractionInputAtSpeed_[ loop ] ) { // use speed power fraction
					this->totEfficAtSpeed_[ loop ] = this->flowFractionAtSpeed_[ loop ] * this->designAirVolFlowRate_ * this->deltaPress_  / ( this->designElecPower_ * this->powerFractionAtSpeed_[ loop ] );
				} else { // use power curve
					this->totEfficAtSpeed_[ loop ] = this->flowFractionAtSpeed_[ loop ] * this->designAirVolFlowRate_ * this->deltaPress_ / ( this->designElecPower_ * CurveManager::CurveValue( this->powerModFuncFlowFractionCurveIndex_, this->flowFractionAtSpeed_[ loop ] ) );
					this->powerFractionAtSpeed_[ loop ] = CurveManager::CurveValue( this->powerModFuncFlowFractionCurveIndex_, this->flowFractionAtSpeed_[ loop ] );
				}
				
			}
		}

		OutputReportPredefined::PreDefTableEntry( OutputReportPredefined::pdchFanType, this->name_, this->fanType_ );
		OutputReportPredefined::PreDefTableEntry( OutputReportPredefined::pdchFanTotEff, this->name_, this->fanTotalEff_ );
		OutputReportPredefined::PreDefTableEntry( OutputReportPredefined::pdchFanDeltaP, this->name_, this->deltaPress_ );
		OutputReportPredefined::PreDefTableEntry( OutputReportPredefined::pdchFanVolFlow, this->name_, this->designAirVolFlowRate_ );

		OutputReportPredefined::PreDefTableEntry( OutputReportPredefined::pdchFanPwr, this->name_, this->designElecPower_ );
		if ( this->designAirVolFlowRate_ != 0.0 ) {
			OutputReportPredefined::PreDefTableEntry( OutputReportPredefined::pdchFanPwrPerFlow, this->name_, this->designElecPower_ / this->designAirVolFlowRate_ );
		}
		OutputReportPredefined::PreDefTableEntry( OutputReportPredefined::pdchFanMotorIn, this->name_, this->motorInAirFrac_ );
		OutputReportPredefined::PreDefTableEntry( OutputReportPredefined::pdchFanEndUse, this->name_, this->endUseSubcategoryName_ );

		this->objSizingFlag_ = false;
	}

	FanSystem::FanSystem( // constructor
		std::string const objectName
	):
		fanType_Num_( 0 ),
		availSchedIndex_( 0 ),
		inletNodeNum_( 0 ),
		outletNodeNum_( 0 ),
		designAirVolFlowRate_( 0.0 ),
		designAirVolFlowRateWasAutosized_( false),
		speedControl_( speedControlNotSet ), 
		minPowerFlowFrac_( 0.0 ),
		deltaPress_( 0.0 ),
		motorEff_( 0.0 ),
		motorInAirFrac_( 0.0 ),
		designElecPower_( 0.0 ),
		designElecPowerWasAutosized_( false),
		powerSizingMethod_( powerSizingMethodNotSet),
		elecPowerPerFlowRate_( 0.0 ),
		elecPowerPerFlowRatePerPressure_( 0.0 ),
		fanTotalEff_( 0.0 ),
		powerModFuncFlowFractionCurveIndex_( 0 ),
		nightVentPressureDelta_( 0.0 ),
		nightVentFlowFraction_( 0.0 ),
		zoneNum_( 0 ),
		zoneRadFract_( 0.0 ),
		heatLossesDestination_( heatLossNotDetermined ),
		endUseSubcategoryName_( "" ),
		numSpeeds_( 0 ),
		inletAirMassFlowRate_( 0.0 ),
		outletAirMassFlowRate_( 0.0 ),
		minAirFlowRate_( 0.0 ),
		maxAirMassFlowRate_( 0.0 ),
		minAirMassFlowRate_( 0.0 ),
		inletAirTemp_( 0.0 ),
		outletAirTemp_( 0.0 ),
		inletAirHumRat_( 0.0 ),
		outletAirHumRat_( 0.0 ),
		inletAirEnthalpy_( 0.0 ),
		outletAirEnthalpy_( 0.0 ),
		objTurnFansOn_( false ),
		objTurnFansOff_( false ),
		objEnvrnFlag_( true ),
		objSizingFlag_( true ),
		fanPower_( 0.0 ),
		fanEnergy_( 0.0 ),
		maxAirFlowRateEMSOverrideOn_( false ),
		maxAirFlowRateEMSOverrideValue_( 0.0 ),
		eMSFanPressureOverrideOn_( false ),
		eMSFanPressureValue_( 0.0 ),
		eMSFanEffOverrideOn_( false ),
		eMSFanEffValue_( 0.0 ),
		eMSMaxMassFlowOverrideOn_( false ),
		eMSAirMassFlowValue_( 0.0 ),
		faultyFilterFlag_( false ),
		faultyFilterIndex_( 0 ),
		massFlowRateMaxAvail_( 0.0 ),
		massFlowRateMinAvail_( 0.0 ),
		rhoAirStdInit_( 0.0 ),
		oneTimePowerCurveCheck_( true ) 
	{

		std::string const routineName = "HVACFan constructor ";
		int numAlphas; // Number of elements in the alpha array
		int numNums; // Number of elements in the numeric array
		int IOStat; // IO Status when calling get input subroutine
		bool errorsFound = false;
		DataIPShortCuts::cCurrentModuleObject = "Fan:SystemModel";

		int objectNum = InputProcessor::GetObjectItemNum( DataIPShortCuts::cCurrentModuleObject, objectName );

		InputProcessor::GetObjectItem( DataIPShortCuts::cCurrentModuleObject, objectNum, DataIPShortCuts::cAlphaArgs, numAlphas, DataIPShortCuts::rNumericArgs, numNums, IOStat, DataIPShortCuts::lNumericFieldBlanks, DataIPShortCuts::lAlphaFieldBlanks, DataIPShortCuts::cAlphaFieldNames, DataIPShortCuts::cNumericFieldNames  );

		this->name_ = DataIPShortCuts::cAlphaArgs( 1 );
		//TODO how to check for unique names across objects during get input?
		this->fanType_ = DataIPShortCuts::cCurrentModuleObject;
		this->fanType_Num_ = DataHVACGlobals::FanType_SystemModelObject;
		if ( DataIPShortCuts::lAlphaFieldBlanks( 2 ) ) {
			this->availSchedIndex_ = DataGlobals::ScheduleAlwaysOn;
		} else {
			this->availSchedIndex_ = ScheduleManager::GetScheduleIndex( DataIPShortCuts::cAlphaArgs( 2 ) );
			if ( this->availSchedIndex_ == 0 ) {
				ShowSevereError( routineName + DataIPShortCuts::cCurrentModuleObject + "=\"" + DataIPShortCuts::cAlphaArgs( 1 ) + "\", invalid entry." );
				ShowContinueError( "Invalid " + DataIPShortCuts::cAlphaFieldNames( 2 ) + " = " + DataIPShortCuts::cAlphaArgs( 2 ) );
				errorsFound = true;
			}
		}
		this->inletNodeNum_ = NodeInputManager::GetOnlySingleNode( DataIPShortCuts::cAlphaArgs( 3 ), errorsFound, DataIPShortCuts::cCurrentModuleObject, DataIPShortCuts::cAlphaArgs( 1 ), DataLoopNode::NodeType_Air, DataLoopNode::NodeConnectionType_Inlet, 1, DataLoopNode::ObjectIsNotParent );
		this->outletNodeNum_ = NodeInputManager::GetOnlySingleNode( DataIPShortCuts::cAlphaArgs( 4 ), errorsFound, DataIPShortCuts::cCurrentModuleObject, DataIPShortCuts::cAlphaArgs( 1 ), DataLoopNode::NodeType_Air, DataLoopNode::NodeConnectionType_Outlet, 1, DataLoopNode::ObjectIsNotParent );

		BranchNodeConnections::TestCompSet( DataIPShortCuts::cCurrentModuleObject, DataIPShortCuts::cAlphaArgs( 1 ),  DataIPShortCuts::cAlphaArgs( 3 ),  DataIPShortCuts::cAlphaArgs( 4 ),"Air Nodes" );

		this->designAirVolFlowRate_ =  DataIPShortCuts::rNumericArgs( 1 );
		if ( this->designAirVolFlowRate_ == DataSizing::AutoSize ) {
			this->designAirVolFlowRateWasAutosized_ = true;
		}

		if ( DataIPShortCuts::lAlphaFieldBlanks( 5 ) ) {
			this->speedControl_ = speedControlDiscrete;
		} else if ( InputProcessor::SameString( DataIPShortCuts::cAlphaArgs( 5 ), "Continuous") ) {
			this->speedControl_ = speedControlContinuous;
		} else if ( InputProcessor::SameString( DataIPShortCuts::cAlphaArgs( 5 ), "Discrete")  ) {
			this->speedControl_ = speedControlDiscrete;
		} else {
			ShowSevereError( routineName + DataIPShortCuts::cCurrentModuleObject + "=\"" + DataIPShortCuts::cAlphaArgs( 1 ) + "\", invalid entry." );
			ShowContinueError( "Invalid " + DataIPShortCuts::cAlphaFieldNames( 5 ) + " = " + DataIPShortCuts::cAlphaArgs( 5 ) );
			errorsFound = true;
		}

		this->minPowerFlowFrac_ = DataIPShortCuts::rNumericArgs( 2 );
		this->deltaPress_       = DataIPShortCuts::rNumericArgs( 3 );
		this->motorEff_         = DataIPShortCuts::rNumericArgs( 4 );
		this->motorInAirFrac_   = DataIPShortCuts::rNumericArgs( 5 );
		this->designElecPower_  = DataIPShortCuts::rNumericArgs( 6 );
		if ( this->designElecPower_ == DataSizing::AutoSize ) {
			this->designElecPowerWasAutosized_ = true;
		}
		if ( this->designElecPowerWasAutosized_ ) {
			if ( DataIPShortCuts::lAlphaFieldBlanks( 6 ) ) {
				this->powerSizingMethod_ = powerPerFlowPerPressure;
			} else if ( InputProcessor::SameString( DataIPShortCuts::cAlphaArgs( 6 ), "PowerPerFlow" ) ) {
				this->powerSizingMethod_ = powerPerFlow;
			} else if ( InputProcessor::SameString( DataIPShortCuts::cAlphaArgs( 6 ), "PowerPerFlowPerPressure" ) ) {
				this->powerSizingMethod_ = powerPerFlowPerPressure;
			} else if (  InputProcessor::SameString( DataIPShortCuts::cAlphaArgs( 6 ), "TotalEfficiencyAndPressure" ) ) {
				this->powerSizingMethod_ = totalEfficiencyAndPressure;
			} else {
				ShowSevereError( routineName + DataIPShortCuts::cCurrentModuleObject + "=\"" + DataIPShortCuts::cAlphaArgs( 1 ) + "\", invalid entry." );
				ShowContinueError( "Invalid " + DataIPShortCuts::cAlphaFieldNames( 6 ) + " = " + DataIPShortCuts::cAlphaArgs( 6 ) );
				errorsFound = true;
			}
			this->elecPowerPerFlowRate_            = DataIPShortCuts::rNumericArgs( 7 );
			this->elecPowerPerFlowRatePerPressure_ = DataIPShortCuts::rNumericArgs( 8 );
			this->fanTotalEff_                     = DataIPShortCuts::rNumericArgs( 9 );
		}
		if ( ! DataIPShortCuts::lAlphaFieldBlanks( 7 ) ) {
			this->powerModFuncFlowFractionCurveIndex_ = CurveManager::GetCurveIndex( DataIPShortCuts::cAlphaArgs( 7 ) );
		}
		this-> nightVentPressureDelta_       = DataIPShortCuts::rNumericArgs( 10 );
		this-> nightVentFlowFraction_        = DataIPShortCuts::rNumericArgs( 11 );
		this->zoneNum_ = InputProcessor::FindItemInList( DataIPShortCuts::cAlphaArgs( 8 ), DataHeatBalance::Zone );
		if ( this->zoneNum_ > 0 ) this->heatLossesDestination_ = zoneGains;
		if ( this->zoneNum_ == 0 ) {
			if ( DataIPShortCuts::lAlphaFieldBlanks( 8 ) ) {
				this->heatLossesDestination_ = lostToOutside;
			} else {
				this->heatLossesDestination_ = lostToOutside;
				ShowWarningError( routineName + DataIPShortCuts::cCurrentModuleObject + "=\"" + DataIPShortCuts::cAlphaArgs( 1 ) + "\", invalid entry." );
				ShowContinueError( "Invalid " + DataIPShortCuts::cAlphaFieldNames( 8 ) + " = " + DataIPShortCuts::cAlphaArgs( 8 ) );
				ShowContinueError( "Zone name not found. Fan motor heat losses will not be added to a zone" );
				// continue with simulation but motor losses not sent to a zone.
			}
		}
		this->zoneRadFract_ = DataIPShortCuts::rNumericArgs( 12 );
		if ( ! DataIPShortCuts::lAlphaFieldBlanks( 9 ) ) {
			this->endUseSubcategoryName_ = DataIPShortCuts::cAlphaArgs( 9 );
		} else {
			this->endUseSubcategoryName_ = "General";
		}
		
		if ( ! DataIPShortCuts::lNumericFieldBlanks( 13 ) ){
			this->numSpeeds_ =  DataIPShortCuts::rNumericArgs( 13 );
		} else {
			this->numSpeeds_ =  1;
		}
		this->fanRunTimeFractionAtSpeed_.resize( this->numSpeeds_, 0.0 );
		if ( this->speedControl_ == speedControlDiscrete && this->numSpeeds_ > 1 ) {
			//should have field sets 
			this->flowFractionAtSpeed_.resize( this->numSpeeds_, 0.0 );
			this->powerFractionAtSpeed_.resize( this->numSpeeds_, 0.0 );
			this->powerFractionInputAtSpeed_.resize( this->numSpeeds_, false );
			if ( this->numSpeeds_ == (( numNums - 13 ) / 2 ) || this->numSpeeds_ == (( numNums + 1 - 13 ) / 2 ) ) {
				for ( auto loopSet = 0 ; loopSet< this->numSpeeds_; ++loopSet ) {
					this->flowFractionAtSpeed_[ loopSet ]  = DataIPShortCuts::rNumericArgs( 13 + loopSet * 2 + 1 );
					if ( ! DataIPShortCuts::lNumericFieldBlanks( 13 + loopSet * 2 + 2  )  ) {
						this->powerFractionAtSpeed_[ loopSet ] = DataIPShortCuts::rNumericArgs( 13 + loopSet * 2 + 2 );
						this->powerFractionInputAtSpeed_[ loopSet ] = true;
					} else {
						this->powerFractionInputAtSpeed_[ loopSet ] = false;
					}
				}
			} else {
				// field set input does not match number of speeds, throw warning
				ShowSevereError( routineName + DataIPShortCuts::cCurrentModuleObject + "=\"" + DataIPShortCuts::cAlphaArgs( 1 ) + "\", invalid entry." );
				ShowContinueError( "Fan with Discrete speed control does not have input for speed data that matches the number of speeds.");
				errorsFound = true;
			}
			// check that flow fractions are increasing
			bool increasingOrderError = false;
			for ( auto loop = 0; loop < (this->numSpeeds_ - 1); ++loop ) {
				if ( this->flowFractionAtSpeed_[ loop ] >  this->flowFractionAtSpeed_[ loop + 1 ]) {
					increasingOrderError = true;
				}
			}
			if ( increasingOrderError ) {
				ShowSevereError( routineName + DataIPShortCuts::cCurrentModuleObject + "=\"" + DataIPShortCuts::cAlphaArgs( 1 ) + "\", invalid entry." );
				ShowContinueError( "Fan with Discrete speed control and multiple speed levels does not have input with flow fractions arranged in increasing order.");
				errorsFound = true;
			}
		}

		// check if power curve present when any speeds have no power fraction 
		if ( this->speedControl_ == speedControlDiscrete && this->numSpeeds_ > 1 && this->powerModFuncFlowFractionCurveIndex_ == 0 ) {
			bool foundMissingPowerFraction = false;
			for ( auto loop = 0 ; loop< this->numSpeeds_; ++loop ) {
				if ( ! this->powerFractionInputAtSpeed_[ loop ] ) {
					foundMissingPowerFraction = true;
				}
			}
			if ( foundMissingPowerFraction ) {
				// field set input does not match number of speeds, throw warning
				ShowSevereError( routineName + DataIPShortCuts::cCurrentModuleObject + "=\"" + DataIPShortCuts::cAlphaArgs( 1 ) + "\", invalid entry." );
				ShowContinueError( "Fan with Discrete speed control does not have input for power fraction at all speed levels and does not have a power curve.");
				errorsFound = true;
			}
		}

		if ( errorsFound ) {
			ShowFatalError( routineName + "Errors found in input.  Program terminates." );
		}

		SetupOutputVariable( "Fan Electric Power [W]", this->fanPower_, "System", "Average", this->name_ );
		SetupOutputVariable( "Fan Rise in Air Temperature [deltaC]", this->deltaTemp_, "System", "Average", this->name_ );
		SetupOutputVariable( "Fan Electric Energy [J]", this->fanEnergy_, "System", "Sum", this->name_, _, "Electric", "Fans", this->endUseSubcategoryName_, "System" );
		if ( this->speedControl_ == speedControlDiscrete && this->numSpeeds_ == 1 ) {
			SetupOutputVariable( "Fan Runtime Fraction []", this->fanRunTimeFractionAtSpeed_[ 0 ], "System", "Average", this->name_ );
		} else if ( this->speedControl_ == speedControlDiscrete && this->numSpeeds_ > 1 ) {
			for (auto speedLoop = 0; speedLoop < this->numSpeeds_; ++speedLoop) {
				SetupOutputVariable( "Fan Runtime Fraction Speed " + General::TrimSigDigits( speedLoop + 1 ) + " []", this->fanRunTimeFractionAtSpeed_[ speedLoop ], "System", "Average", this->name_ );
			}
		}

		if ( DataGlobals::AnyEnergyManagementSystemInModel ) {
			SetupEMSInternalVariable( "Fan Maximum Mass Flow Rate", this->name_ , "[kg/s]", this->maxAirMassFlowRate_ );
			SetupEMSActuator( "Fan", this->name_ , "Fan Air Mass Flow Rate", "[kg/s]", this->eMSMaxMassFlowOverrideOn_, this->eMSAirMassFlowValue_ );
			SetupEMSInternalVariable( "Fan Nominal Pressure Rise", this->name_ , "[Pa]", this->deltaPress_ );
			SetupEMSActuator( "Fan", this->name_ , "Fan Pressure Rise", "[Pa]", this->eMSFanPressureOverrideOn_, this->eMSFanPressureValue_ );
			SetupEMSInternalVariable( "Fan Nominal Total Efficiency", this->name_, "[fraction]", this->fanTotalEff_ );
			SetupEMSActuator( "Fan",this->name_ , "Fan Total Efficiency", "[fraction]", this->eMSFanEffOverrideOn_, this->eMSFanEffValue_ );
			SetupEMSActuator( "Fan", this->name_ , "Fan Autosized Air Flow Rate", "[m3/s]", this->maxAirFlowRateEMSOverrideOn_, this->maxAirFlowRateEMSOverrideValue_ );
		}
		EMSManager::ManageEMS( DataGlobals::emsCallFromComponentGetInput );
	}

	void
	FanSystem::calcSimpleSystemFan(
		Optional< Real64 const > flowFraction,
		Optional< Real64 const > pressureRise
	)
	{
		Real64 localPressureRise;
		Real64 localFlowFraction;
		Real64 localFanTotEff;
		Real64 localAirMassFlow;

		if ( DataHVACGlobals::NightVentOn ) {
		// assume if non-zero inputs for night data then this fan is to be used with that data
			if ( this->nightVentPressureDelta_ > 0.0 ) { 
				localPressureRise =  this->nightVentPressureDelta_;
			}
			if ( this->nightVentFlowFraction_ > 0.0 ) {
				localFlowFraction = this->nightVentFlowFraction_;
				localAirMassFlow = localFlowFraction * this->maxAirMassFlowRate_;
			}
		} else { // not in night mode
			if ( present( pressureRise ) ) {
				localPressureRise = pressureRise;
			} else {
				localPressureRise = this->deltaPress_;
			}
			if ( present( flowFraction ) ) {
				localFlowFraction = flowFraction;
				localAirMassFlow = localFlowFraction * this->maxAirMassFlowRate_;
			} else {
				localFlowFraction = this->inletAirMassFlowRate_ / this->maxAirMassFlowRate_;
				localAirMassFlow  = this->inletAirMassFlowRate_;
			}
		}

// TODO Faulty fan operation


		//EMS override MassFlow, DeltaPress, and FanEff
		if ( this->eMSFanPressureOverrideOn_ ) localPressureRise = this->eMSFanPressureValue_;
		if ( this->eMSFanEffOverrideOn_ ) localFanTotEff = this->eMSFanEffValue_;
		if ( this->eMSMaxMassFlowOverrideOn_ ) {
			localAirMassFlow = this->eMSAirMassFlowValue_;
		}

		localAirMassFlow = min( localAirMassFlow, this->maxAirMassFlowRate_ );
		localFlowFraction = localAirMassFlow / this->maxAirMassFlowRate_;

		if ( ( ScheduleManager::GetCurrentScheduleValue( this->availSchedIndex_ ) > 0.0 || this->objTurnFansOn_ ) && ! this->objTurnFansOff_ && localAirMassFlow > 0.0 ) {
			//fan is running

			switch ( this->speedControl_ ) {
			
			case speedControlDiscrete: {
				if ( this->numSpeeds_ == 1 ) { // CV or OnOff
					localFanTotEff = this->fanTotalEff_;
					this->fanRunTimeFractionAtSpeed_[ 0 ] = localFlowFraction;
					this->fanPower_ = this->fanRunTimeFractionAtSpeed_[ 0 ] * this->maxAirMassFlowRate_ * localPressureRise / ( localFanTotEff * this->rhoAirStdInit_ );
					Real64 fanShaftPower = this->motorEff_ * this->fanPower_;
					Real64 powerLossToAir = fanShaftPower + ( this->fanPower_ - fanShaftPower )* this->motorInAirFrac_;
					this->outletAirEnthalpy_ = this->inletAirEnthalpy_ + powerLossToAir / localAirMassFlow;
					this->outletAirHumRat_ = this->inletAirHumRat_;
					this->outletAirMassFlowRate_ =  localAirMassFlow;
					this->outletAirTemp_ = Psychrometrics::PsyTdbFnHW( this->outletAirEnthalpy_, this->outletAirHumRat_ );
				} else if ( this->numSpeeds_ > 1 ) { // multi speed

					// find which two speed levels bracket flow fraction and calculate runtimefraction
					int lowSideSpeed = -1;
					int hiSideSpeed  = -1;
					for ( auto loop = 0; loop < this->numSpeeds_; ++loop ) {
						this->fanRunTimeFractionAtSpeed_[ loop ] = 0.0;
					}

					if ( localFlowFraction < this->flowFractionAtSpeed_[ 0 ] ) { // on/off between zero and lowest speed
						hiSideSpeed  = 0;
						this->fanRunTimeFractionAtSpeed_[ 0 ] = localFlowFraction / this->flowFractionAtSpeed_[ 0 ];
					} else {
						for ( auto loop = 0; loop < this->numSpeeds_ - 1; ++loop ) {
							if ( ( this->flowFractionAtSpeed_[ loop ] <= localFlowFraction ) && ( localFlowFraction <= flowFractionAtSpeed_[ loop + 1 ] ) ) {
								lowSideSpeed = loop;
								hiSideSpeed = loop +1;
								break;
							}
						}
						this->fanRunTimeFractionAtSpeed_[ lowSideSpeed ] = ( this->flowFractionAtSpeed_[ hiSideSpeed ] - localFlowFraction ) / ( this->flowFractionAtSpeed_[ hiSideSpeed ] - this->flowFractionAtSpeed_[ lowSideSpeed ] );
						this->fanRunTimeFractionAtSpeed_[ hiSideSpeed ] = ( localFlowFraction - this->flowFractionAtSpeed_[ lowSideSpeed ] ) / ( this->flowFractionAtSpeed_[ hiSideSpeed ] - this->flowFractionAtSpeed_[ lowSideSpeed ] );
					}
					this->fanPower_ = this->fanRunTimeFractionAtSpeed_[ lowSideSpeed ] * this->massFlowAtSpeed_[ lowSideSpeed ] * localPressureRise / ( this->totEfficAtSpeed_[ lowSideSpeed ] * this->rhoAirStdInit_ ) + this->fanRunTimeFractionAtSpeed_[ hiSideSpeed ] * this->massFlowAtSpeed_[ hiSideSpeed ] * localPressureRise / ( this->totEfficAtSpeed_[ hiSideSpeed ] * this->rhoAirStdInit_ );
										Real64 fanShaftPower = this->motorEff_ * this->fanPower_;
					Real64 powerLossToAir = fanShaftPower + ( this->fanPower_ - fanShaftPower )* this->motorInAirFrac_;
					this->outletAirEnthalpy_ = this->inletAirEnthalpy_ + powerLossToAir / localAirMassFlow;
					this->outletAirHumRat_ = this->inletAirHumRat_;
					this->outletAirMassFlowRate_ =  localAirMassFlow;
					this->outletAirTemp_ = Psychrometrics::PsyTdbFnHW( this->outletAirEnthalpy_, this->outletAirHumRat_ );
				}

				localFanTotEff = this->fanTotalEff_;
				break;
			}
			case speedControlContinuous: {
				localFanTotEff = this->fanTotalEff_;
				Real64 localFlowFractionForPower = max( this->minPowerFlowFrac_, localFlowFraction );
				Real64 localPowerFraction = CurveManager::CurveValue( this->powerModFuncFlowFractionCurveIndex_, localFlowFractionForPower );
				this->fanPower_ = localPowerFraction * this->maxAirMassFlowRate_ * localPressureRise / ( localFanTotEff * this->rhoAirStdInit_ );
				Real64 fanShaftPower = this->motorEff_ * this->fanPower_;
				Real64 powerLossToAir = fanShaftPower + ( this->fanPower_ - fanShaftPower )* this->motorInAirFrac_;
				this->outletAirEnthalpy_ = this->inletAirEnthalpy_ + powerLossToAir / localAirMassFlow;
				this->outletAirHumRat_ = this->inletAirHumRat_;
				this->outletAirMassFlowRate_ =  localAirMassFlow;
				this->outletAirTemp_ = Psychrometrics::PsyTdbFnHW( this->outletAirEnthalpy_, this->outletAirHumRat_ );
				
				// When fan air flow is less than 10%, the fan power curve is linearized between the 10% to 0% to
			//  avoid the unrealistic high temperature rise across the fan.
				Real64 deltaTAcrossFan = this->outletAirTemp_ - this->inletAirTemp_;
				if ( deltaTAcrossFan > 20.0 ) {
					Real64 minFlowFracLimitFanHeat = 0.10;
					Real64 powerFractionAtLowMin = 0.0;
					Real64 fanPoweratLowMinimum = 0.0;
					if ( localFlowFractionForPower < minFlowFracLimitFanHeat ) {
						powerFractionAtLowMin = CurveManager::CurveValue( this->powerModFuncFlowFractionCurveIndex_, minFlowFracLimitFanHeat );

						fanPoweratLowMinimum = powerFractionAtLowMin * this->maxAirMassFlowRate_ * localPressureRise / ( localFanTotEff * this->rhoAirStdInit_ );
						this->fanPower_ = localFlowFractionForPower * fanPoweratLowMinimum / minFlowFracLimitFanHeat;
					} else if ( localFlowFraction < minFlowFracLimitFanHeat ) {

						powerFractionAtLowMin = CurveManager::CurveValue( this->powerModFuncFlowFractionCurveIndex_, minFlowFracLimitFanHeat );
						fanPoweratLowMinimum = powerFractionAtLowMin * this->maxAirMassFlowRate_ * localPressureRise / ( localFanTotEff * this->rhoAirStdInit_ );
						this->fanPower_ = localFlowFraction * fanPoweratLowMinimum / minFlowFracLimitFanHeat;
					}
					fanShaftPower = this->motorEff_ * this->fanPower_; // power delivered to shaft
					powerLossToAir = fanShaftPower + ( this->fanPower_ - fanShaftPower ) * this->motorInAirFrac_;
					this->outletAirEnthalpy_ = this->inletAirEnthalpy_ + powerLossToAir / localAirMassFlow;
					// This fan does not change the moisture or Mass Flow across the component
					this->outletAirHumRat_ = this->inletAirHumRat_;
					this->outletAirMassFlowRate_ = localAirMassFlow;
					this->outletAirTemp_ = Psychrometrics::PsyTdbFnHW( this->outletAirEnthalpy_, this->outletAirHumRat_ );
				}
				break;
			} // continuous speed control case
			} // end switch

		} else { // fan is off
			//Fan is off and not operating no power consumed and mass flow rate.
			this->fanPower_ = 0.0;
			this->outletAirMassFlowRate_ = 0.0;
			this->outletAirHumRat_ = this->inletAirHumRat_;
			this->outletAirEnthalpy_ = this->inletAirEnthalpy_;
			this->outletAirTemp_ = this->inletAirTemp_;
			// Set the Control Flow variables to 0.0 flow when OFF.
			this->massFlowRateMaxAvail_ = 0.0;
			this->massFlowRateMinAvail_ = 0.0;
		}
	}

	void
	FanSystem::update() const // does not change state of object, only update elsewhere
	{
		// Set the outlet air node of the fan
		DataLoopNode::Node( this->outletNodeNum_ ).MassFlowRate = this->outletAirMassFlowRate_;
		DataLoopNode::Node( this->outletNodeNum_ ).Temp         = this->outletAirTemp_;
		DataLoopNode::Node( this->outletNodeNum_ ).HumRat       = this->outletAirHumRat_;
		DataLoopNode::Node( this->outletNodeNum_ ).Enthalpy     = this->outletAirEnthalpy_;
		// Set the outlet nodes for properties that just pass through & not used
		DataLoopNode::Node( this->outletNodeNum_ ).Quality = DataLoopNode::Node( this->inletNodeNum_ ).Quality;
		DataLoopNode::Node( this->outletNodeNum_ ).Press   = DataLoopNode::Node( this->inletNodeNum_ ).Press;

		// Set the Node Flow Control Variables from the Fan Control Variables
		DataLoopNode::Node( this->outletNodeNum_ ).MassFlowRateMaxAvail = this->massFlowRateMaxAvail_;
		DataLoopNode::Node( this->outletNodeNum_ ).MassFlowRateMinAvail = this->massFlowRateMinAvail_;

		// make sure inlet has the same mass flow
		DataLoopNode::Node( this->inletNodeNum_ ).MassFlowRate = this->outletAirMassFlowRate_;

		if ( DataContaminantBalance::Contaminant.CO2Simulation ) {
			DataLoopNode::Node( this->outletNodeNum_ ).CO2 = DataLoopNode::Node( this->inletNodeNum_ ).CO2;
		}
		if ( DataContaminantBalance::Contaminant.GenericContamSimulation ) {
			DataLoopNode::Node( this->outletNodeNum_ ).GenContam = DataLoopNode::Node( this->inletNodeNum_ ).GenContam;
		}

		if ( this->heatLossesDestination_ == zoneGains ) {
	//TODO	
		
		}

		//ugly use of global here
		DataHVACGlobals::FanElecPower = this->fanPower_;
		DataAirLoop::LoopOnOffFanRTF  = this->fanRunTimeFractionAtSpeed_[ this->numSpeeds_ - 1 ]; //fill with RTF from highest speed level

	}

	void
	FanSystem::report()
	{
		this->fanEnergy_ = this->fanPower_ * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
		this->deltaTemp_ = this->outletAirTemp_ - this->inletAirTemp_;
	}

	std::string const &
	FanSystem::name() const
	{
		return name_;
	}

	Real64
	FanSystem::fanPower() const
	{
		return fanPower_;
	}

	Real64
	FanSystem::designAirVolFlowRate() const
	{
		return this->designAirVolFlowRate_;
	}

	int
	FanSystem::inletNodeNum() const
	{
		return this->inletNodeNum_;
	}

	int
	FanSystem::outletNodeNum() const
	{
		return this->outletNodeNum_;
	}

	int
	FanSystem::availSchedIndex() const
	{
		return this->availSchedIndex_;
	}

	int
	FanSystem::getFanPowerCurveIndex() const
	{
		return this->powerModFuncFlowFractionCurveIndex_;
	}

	Real64
	FanSystem::getFanDesignTemperatureRise() const
	{
		if ( ! this->objSizingFlag_ ) {
			Real64 cpAir = Psychrometrics::PsyCpAirFnWTdb( DataPrecisionGlobals::constant_zero, DataPrecisionGlobals::constant_twenty );
			Real64 designDeltaT = ( this->deltaPress_ / ( this->rhoAirStdInit_ * cpAir * this->fanTotalEff_ ) ) * ( this->motorEff_ + this->motorInAirFrac_ * ( 1.0 - this->motorEff_ ) );
			return designDeltaT;
		} else {
			//TODO throw warning, exception, call sizing?
			ShowWarningError("FanSystem::getFanDesignTemperatureRise called before fan sizing completed ");
			return 0.0;
		}
	}

	Real64 
	FanSystem::getFanDesignHeatGain(
		Real64 const FanVolFlow // fan volume flow rate [m3/s]
	)
	{
		if ( ! this->objSizingFlag_ ) {
			Real64 fanPowerTot = ( FanVolFlow * this->deltaPress_ ) / this->fanTotalEff_ ;
			Real64 designHeatGain = this->motorEff_ * fanPowerTot + ( fanPowerTot - this->motorEff_ * fanPowerTot ) * this->motorInAirFrac_;
			return designHeatGain;
		} else {
			this->set_size();
			Real64 fanPowerTot = ( FanVolFlow * this->deltaPress_ ) / this->fanTotalEff_ ;
			Real64 designHeatGain = this->motorEff_ * fanPowerTot + ( fanPowerTot - this->motorEff_ * fanPowerTot ) * this->motorInAirFrac_;
			return designHeatGain;

		}
	}

	bool
	FanSystem::getIfContinuousSpeedControl() const 
	{
		if (this->speedControl_ == speedControlContinuous ) {
			return true;
		} else {
			return false;
		}
	}

} //HVACFan namespace

} // EnergyPlus namespace
