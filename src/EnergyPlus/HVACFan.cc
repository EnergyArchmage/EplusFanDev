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
			if ( objectName == fanObjs[ loop ]->getFanName() ) {
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


	void
	FanSystem::simulate(
//		bool const firstHVACIteration,
		Optional< Real64 const > flowFraction,
		Optional_bool_const zoneCompTurnFansOn, // Turn fans ON signal from ZoneHVAC component
		Optional_bool_const zoneCompTurnFansOff, // Turn Fans OFF signal from ZoneHVAC component
		Optional< Real64 const > pressureRise // Pressure difference to use for DeltaPress, for rating DX coils without entire duct system
	)
	{

		this->objTurnFansOn = false;
		this->objTurnFansOff = false;

		this->init( );

		if ( present( zoneCompTurnFansOn ) && present( zoneCompTurnFansOff ) ) {
			// Set module-level logic flags equal to ZoneCompTurnFansOn and ZoneCompTurnFansOff values passed into this routine
			// for ZoneHVAC components with system availability managers defined.
			// The module-level flags get used in the other subroutines (e.g., SimSimpleFan,SimVariableVolumeFan and SimOnOffFan)
			this->objTurnFansOn = zoneCompTurnFansOn;
			this->objTurnFansOff = zoneCompTurnFansOff;
		} else {
			// Set module-level logic flags equal to the global LocalTurnFansOn and LocalTurnFansOff variables for all other cases.
			this->objTurnFansOn = DataHVACGlobals::TurnFansOn;
			this->objTurnFansOff = DataHVACGlobals::TurnFansOff;
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
	
		if ( ! DataGlobals::SysSizingCalc && this->objSizingFlag ) {
			this->set_size();
			this->objSizingFlag = false;
			if ( DataSizing::CurSysNum > 0 ) {
				DataAirLoop::AirLoopControlInfo( DataSizing::CurSysNum ).CyclingFan = true;
			}
		}

		if ( DataGlobals::BeginEnvrnFlag && this->objEnvrnFlag ) {
			this->rhoAirStdInit = DataEnvironment::StdRhoAir;
			this->maxAirMassFlowRate = this->designAirVolFlowRate * this->rhoAirStdInit;
			this->minAirFlowRate = this->designAirVolFlowRate * this->minPowerFlowFrac;
			this->minAirMassFlowRate = this->minAirFlowRate * this->rhoAirStdInit;

//			if ( Fan( FanNum ).NVPerfNum > 0 ) {
//				NightVentPerf( Fan( FanNum ).NVPerfNum ).MaxAirMassFlowRate = NightVentPerf( Fan( FanNum ).NVPerfNum ).MaxAirFlowRate * Fan( FanNum ).RhoAirStdInit;
//			}

			//Init the Node Control variables
			DataLoopNode::Node( this->outletNodeNum ).MassFlowRateMax = this->maxAirMassFlowRate;
			DataLoopNode::Node( this->outletNodeNum  ).MassFlowRateMin =this->minAirMassFlowRate;

			//Initialize all report variables to a known state at beginning of simulation
			this->fanPower = 0.0;
			this->deltaTemp = 0.0;
			this->fanEnergy = 0.0;
			for ( auto loop = 0; loop < this->numSpeeds; ++loop ) {
				this->fanRunTimeFractionAtSpeed[ loop ] = 0.0;
			}
			this->objEnvrnFlag =  false;
		}

		if ( ! DataGlobals::BeginEnvrnFlag ) {
			this->objEnvrnFlag = true;
		}

		this->massFlowRateMaxAvail = min( DataLoopNode::Node( this->outletNodeNum ).MassFlowRateMax, DataLoopNode::Node( this->inletNodeNum ).MassFlowRateMaxAvail );
		this->massFlowRateMinAvail = min( max( DataLoopNode::Node( this->outletNodeNum ).MassFlowRateMin, DataLoopNode::Node( this->inletNodeNum ).MassFlowRateMinAvail ), DataLoopNode::Node( this->inletNodeNum ).MassFlowRateMaxAvail );

		// Load the node data in this section for the component simulation
		//First need to make sure that the MassFlowRate is between the max and min avail.
		this->inletAirMassFlowRate = min( DataLoopNode::Node( this->inletNodeNum ).MassFlowRate, this->massFlowRateMaxAvail );
		this->inletAirMassFlowRate = max( this->inletAirMassFlowRate, this->massFlowRateMinAvail );

		//Then set the other conditions
		this->inletAirTemp     = DataLoopNode::Node( this->inletNodeNum ).Temp;
		this->inletAirHumRat   = DataLoopNode::Node( this->inletNodeNum ).HumRat;
		this->inletAirEnthalpy = DataLoopNode::Node( this->inletNodeNum ).Enthalpy;

	}

	void
	FanSystem::set_size()
	{
		std::string const routineName = "HVACFan::set_size ";
	
		Real64 tempFlow = this->designAirVolFlowRate;
		bool bPRINT = true;
		DataSizing::DataAutosizable = true;
		DataSizing::DataEMSOverrideON = this->maxAirFlowRateEMSOverrideOn;
		DataSizing::DataEMSOverride   = this->maxAirFlowRateEMSOverrideValue;
		ReportSizingManager::RequestSizing(this->fanType, this->name, DataHVACGlobals::SystemAirflowSizing, "Design Maximum Air Flow Rate [m3/s]", tempFlow, bPRINT, routineName );
		this->designAirVolFlowRate    = tempFlow;
		DataSizing::DataAutosizable   = true;
		DataSizing::DataEMSOverrideON = false;
		DataSizing::DataEMSOverride   = 0.0;


		if ( this->designElecPowerWasAutosized ) {
		
			switch ( this->powerSizingMethod )
			{
		
			case powerPerFlow: {
				this->designElecPower = this->designAirVolFlowRate * this->elecPowerPerFlowRate;
				break;
			}
			case powerPerFlowPerPressure: {
				this->designElecPower = this->designAirVolFlowRate * this->deltaPress * this->elecPowerPerFlowRatePerPressure;
				break;
			}
			case totalEfficiencyAndPressure: {
				this->designElecPower = this->designAirVolFlowRate * this->deltaPress / this->fanTotalEff;
				break;
			}
		
			} // end switch

			//report design power
			ReportSizingManager::ReportSizingOutput( this->fanType, this->name, "Design Electric Power Consumption [W]", this->designElecPower );
		
		} // end if power was autosized

		//calculate total fan system efficiency at design
		this->fanTotalEff = this->designAirVolFlowRate * this->deltaPress  / this->designElecPower;

		if (this->numSpeeds > 1 ) { // set up values at speeds
			this->massFlowAtSpeed.resize( this->numSpeeds, 0.0 );
			this->totEfficAtSpeed.resize( this->numSpeeds, 0.0 );
			for ( auto loop=0; loop < this->numSpeeds; ++loop ) {
				this->massFlowAtSpeed[ loop ] = this->maxAirMassFlowRate * this->flowFractionAtSpeed[ loop ];
				if ( this->powerFractionInputAtSpeed[ loop ] ) { // use speed power fraction
					this->totEfficAtSpeed[ loop ] = this->flowFractionAtSpeed[ loop ] * this->designAirVolFlowRate * this->deltaPress  / ( this->designElecPower * this->powerFractionAtSpeed[ loop ] );
				} else { // use power curve
					this->totEfficAtSpeed[ loop ] = this->flowFractionAtSpeed[ loop ] * this->designAirVolFlowRate * this->deltaPress  / ( this->designElecPower * CurveManager::CurveValue( this->powerModFuncFlowFractionCurveIndex, this->flowFractionAtSpeed[ loop ] ) );
					this->powerFractionAtSpeed[ loop ] = CurveManager::CurveValue( this->powerModFuncFlowFractionCurveIndex, this->flowFractionAtSpeed[ loop ] );
				}
				
			}
		}

		OutputReportPredefined::PreDefTableEntry( OutputReportPredefined::pdchFanType, this->name, this->fanType );
		OutputReportPredefined::PreDefTableEntry( OutputReportPredefined::pdchFanTotEff, this->name, this->fanTotalEff );
		OutputReportPredefined::PreDefTableEntry( OutputReportPredefined::pdchFanDeltaP, this->name, this->deltaPress );
		OutputReportPredefined::PreDefTableEntry( OutputReportPredefined::pdchFanVolFlow, this->name, this->designAirVolFlowRate );

		OutputReportPredefined::PreDefTableEntry( OutputReportPredefined::pdchFanPwr, this->name, this->designElecPower );
		if ( this->designAirVolFlowRate != 0.0 ) {
			OutputReportPredefined::PreDefTableEntry( OutputReportPredefined::pdchFanPwrPerFlow, this->name, this->designElecPower / this->designAirVolFlowRate );
		}
		OutputReportPredefined::PreDefTableEntry( OutputReportPredefined::pdchFanMotorIn, this->name, this->motorInAirFrac );
		OutputReportPredefined::PreDefTableEntry( OutputReportPredefined::pdchFanEndUse, this->name, this->endUseSubcategoryName );

		this->objSizingFlag = false;
	}

	FanSystem::FanSystem( // constructor
		std::string const objectName
	)
	{
		//initialize all data
		name                      = ""; 
		fanType                   = ""; 
		fanType_Num               = 0; 
		availSchedIndex           = 0; 
		inletNodeNum              = 0;
		outletNodeNum             = 0; 
		designAirVolFlowRate      = 0.0; 
		designAirVolFlowRateWasAutosized = false; 
		speedControl = speedControlNotSet; 
		minPowerFlowFrac          = 0.0; 
		deltaPress                = 0.0; 
		motorEff                  = 0.0; 
		motorInAirFrac            = 0.0; 
		designElecPower           = 0.0; 
		designElecPowerWasAutosized = false;
		powerSizingMethod = powerSizingMethodNotSet; 
		elecPowerPerFlowRate      = 0.0; 
		elecPowerPerFlowRatePerPressure = 0.0; 
		fanTotalEff               = 0.0; 
		powerModFuncFlowFractionCurveIndex = 0; 
		nightVentPressureDelta    = 0.0; 
		nightVentFlowFraction     = 0.0;
		zoneNum                   = 0; 
		zoneRadFract              = 0.0;
		heatLossesDestination     = heatLossNotDetermined;
		endUseSubcategoryName     = "";
		numSpeeds                 = 0; 
//TODO, how to initialize vectors here? 
		//std::vector < Real64 > flowFractionAtSpeed; //array of flow fractions for speed levels
		//std::vector < Real64 > powerFractionAtSpeed; // array of power fractions for speed levels
		//std::vector < bool > powerFractionInputAtSpeed;
		//std::vector < Real64 > massFlowAtSpeed;
		//std::vector < Real64 > totEfficAtSpeed;
		inletAirMassFlowRate      = 0.0; 
		outletAirMassFlowRate     = 0.0;
		minAirFlowRate            = 0.0; 
		maxAirMassFlowRate        = 0.0; 
		minAirMassFlowRate        = 0.0; 
		inletAirTemp              = 0.0;
		outletAirTemp             = 0.0;
		inletAirHumRat            = 0.0;
		outletAirHumRat           = 0.0;
		inletAirEnthalpy          = 0.0;
		outletAirEnthalpy         = 0.0;
		objTurnFansOn             = false;
		objTurnFansOff            = false;
		objEnvrnFlag              = true; 
		objSizingFlag             = true;  
		fanPower                  = 0.0; 
		fanEnergy                 = 0.0; 
	 // std::vector < Real64 > fanRunTimeFractionAtSpeed;
		maxAirFlowRateEMSOverrideOn   = false; 
		maxAirFlowRateEMSOverrideValue = 0.0; 
		eMSFanPressureOverrideOn = false; 
		eMSFanPressureValue      = 0.0; 
		eMSFanEffOverrideOn      = false; 
		eMSFanEffValue           = 0.0; 
		eMSMaxMassFlowOverrideOn = false; 
		eMSAirMassFlowValue      = 0.0; 
		faultyFilterFlag         = false; 
		faultyFilterIndex        = 0; 
		massFlowRateMaxAvail     = 0.0;
		massFlowRateMinAvail     = 0.0;
		rhoAirStdInit            = 0.0;
		oneTimePowerCurveCheck   = true; 

		std::string const routineName = "HVACFan constructor ";
		int numAlphas; // Number of elements in the alpha array
		int numNums; // Number of elements in the numeric array
		int IOStat; // IO Status when calling get input subroutine
		bool errorsFound = false;
		DataIPShortCuts::cCurrentModuleObject = "Fan:SystemModel";

		int objectNum = InputProcessor::GetObjectItemNum( DataIPShortCuts::cCurrentModuleObject, objectName );

		InputProcessor::GetObjectItem( DataIPShortCuts::cCurrentModuleObject, objectNum, DataIPShortCuts::cAlphaArgs, numAlphas, DataIPShortCuts::rNumericArgs, numNums, IOStat, DataIPShortCuts::lNumericFieldBlanks, DataIPShortCuts::lAlphaFieldBlanks, DataIPShortCuts::cAlphaFieldNames, DataIPShortCuts::cNumericFieldNames  );

		this->name = DataIPShortCuts::cAlphaArgs( 1 );
		//TODO how to check for unique names across objects during get input?
		this->fanType = DataIPShortCuts::cCurrentModuleObject;
		this->fanType_Num = DataHVACGlobals::FanType_SystemModelObject;
		if ( DataIPShortCuts::lAlphaFieldBlanks( 2 ) ) {
			this->availSchedIndex = DataGlobals::ScheduleAlwaysOn;
		} else {
			this->availSchedIndex = ScheduleManager::GetScheduleIndex( DataIPShortCuts::cAlphaArgs( 2 ) );
			if ( this->availSchedIndex == 0 ) {
				ShowSevereError( routineName + DataIPShortCuts::cCurrentModuleObject + "=\"" + DataIPShortCuts::cAlphaArgs( 1 ) + "\", invalid entry." );
				ShowContinueError( "Invalid " + DataIPShortCuts::cAlphaFieldNames( 2 ) + " = " + DataIPShortCuts::cAlphaArgs( 2 ) );
				errorsFound = true;
			}
		}
		this->inletNodeNum = NodeInputManager::GetOnlySingleNode( DataIPShortCuts::cAlphaArgs( 3 ), errorsFound, DataIPShortCuts::cCurrentModuleObject, DataIPShortCuts::cAlphaArgs( 1 ), DataLoopNode::NodeType_Air, DataLoopNode::NodeConnectionType_Inlet, 1, DataLoopNode::ObjectIsNotParent );
		this->outletNodeNum = NodeInputManager::GetOnlySingleNode( DataIPShortCuts::cAlphaArgs( 4 ), errorsFound, DataIPShortCuts::cCurrentModuleObject, DataIPShortCuts::cAlphaArgs( 1 ), DataLoopNode::NodeType_Air, DataLoopNode::NodeConnectionType_Outlet, 1, DataLoopNode::ObjectIsNotParent );

		BranchNodeConnections::TestCompSet( DataIPShortCuts::cCurrentModuleObject, DataIPShortCuts::cAlphaArgs( 1 ),  DataIPShortCuts::cAlphaArgs( 3 ),  DataIPShortCuts::cAlphaArgs( 4 ),"Air Nodes" );

		this->designAirVolFlowRate =  DataIPShortCuts::rNumericArgs( 1 );
		if ( this->designAirVolFlowRate == DataSizing::AutoSize ) {
			this->designAirVolFlowRateWasAutosized = true;
		}

		if ( DataIPShortCuts::lAlphaFieldBlanks( 5 ) ) {
			this->speedControl = speedControlDiscrete;
		} else if ( InputProcessor::SameString( DataIPShortCuts::cAlphaArgs( 5 ), "Continuous") ) {
			this->speedControl = speedControlContinuous;
		} else if ( InputProcessor::SameString( DataIPShortCuts::cAlphaArgs( 5 ), "Discrete")  ) {
			this->speedControl = speedControlDiscrete;
		} else {
			ShowSevereError( routineName + DataIPShortCuts::cCurrentModuleObject + "=\"" + DataIPShortCuts::cAlphaArgs( 1 ) + "\", invalid entry." );
			ShowContinueError( "Invalid " + DataIPShortCuts::cAlphaFieldNames( 5 ) + " = " + DataIPShortCuts::cAlphaArgs( 5 ) );
			errorsFound = true;
		}

		this->minPowerFlowFrac = DataIPShortCuts::rNumericArgs( 2 );
		this->deltaPress       = DataIPShortCuts::rNumericArgs( 3 );
		this->motorEff         = DataIPShortCuts::rNumericArgs( 4 );
		this->motorInAirFrac   = DataIPShortCuts::rNumericArgs( 5 );
		this->designElecPower  = DataIPShortCuts::rNumericArgs( 6 );
		if ( this->designElecPower == DataSizing::AutoSize ) {
			this->designElecPowerWasAutosized = true;
		}
		if ( this->designElecPowerWasAutosized ) {
			if ( DataIPShortCuts::lAlphaFieldBlanks( 6 ) ) {
				this->powerSizingMethod = powerPerFlowPerPressure;
			} else if ( InputProcessor::SameString( DataIPShortCuts::cAlphaArgs( 6 ), "PowerPerFlow" ) ) {
				this->powerSizingMethod = powerPerFlow;
			} else if ( InputProcessor::SameString( DataIPShortCuts::cAlphaArgs( 6 ), "PowerPerFlowPerPressure" ) ) {
				this->powerSizingMethod = powerPerFlowPerPressure;
			} else if (  InputProcessor::SameString( DataIPShortCuts::cAlphaArgs( 6 ), "TotalEfficiencyAndPressure" ) ) {
				this->powerSizingMethod = totalEfficiencyAndPressure;
			} else {
				ShowSevereError( routineName + DataIPShortCuts::cCurrentModuleObject + "=\"" + DataIPShortCuts::cAlphaArgs( 1 ) + "\", invalid entry." );
				ShowContinueError( "Invalid " + DataIPShortCuts::cAlphaFieldNames( 6 ) + " = " + DataIPShortCuts::cAlphaArgs( 6 ) );
				errorsFound = true;
			}
			this->elecPowerPerFlowRate            = DataIPShortCuts::rNumericArgs( 7 );
			this->elecPowerPerFlowRatePerPressure = DataIPShortCuts::rNumericArgs( 8 );
			this->fanTotalEff                     = DataIPShortCuts::rNumericArgs( 9 );

		}
		if ( ! DataIPShortCuts::lAlphaFieldBlanks( 7 ) ) {
			this->powerModFuncFlowFractionCurveIndex = CurveManager::GetCurveIndex( DataIPShortCuts::cAlphaArgs( 7 ) );
		}
		this-> nightVentPressureDelta       = DataIPShortCuts::rNumericArgs( 10 );
		this-> nightVentFlowFraction        = DataIPShortCuts::rNumericArgs( 11 );
		this->zoneNum = InputProcessor::FindItemInList( DataIPShortCuts::cAlphaArgs( 8 ), DataHeatBalance::Zone );
		if ( this->zoneNum > 0 ) this->heatLossesDestination = zoneGains;
		if ( this->zoneNum == 0 ) {
			if ( DataIPShortCuts::lAlphaFieldBlanks( 8 ) ) {
				this->heatLossesDestination = lostToOutside;
			} else {
				this->heatLossesDestination = lostToOutside;
				ShowWarningError( routineName + DataIPShortCuts::cCurrentModuleObject + "=\"" + DataIPShortCuts::cAlphaArgs( 1 ) + "\", invalid entry." );
				ShowContinueError( "Invalid " + DataIPShortCuts::cAlphaFieldNames( 8 ) + " = " + DataIPShortCuts::cAlphaArgs( 8 ) );
				ShowContinueError( "Zone name not found. Fan motor heat losses will not be added to a zone" );
				// continue with simulation but motor losses not sent to a zone.
			}
		}
		this->zoneRadFract = DataIPShortCuts::rNumericArgs( 12 );
		if ( ! DataIPShortCuts::lAlphaFieldBlanks( 9 ) ) {
			this->endUseSubcategoryName = DataIPShortCuts::cAlphaArgs( 9 );
		} else {
			this->endUseSubcategoryName = "General";
		}
		
		if ( ! DataIPShortCuts::lNumericFieldBlanks( 13 ) ){
			this->numSpeeds =  DataIPShortCuts::rNumericArgs( 13 );
		} else {
			this->numSpeeds =  1;
		}
		this->fanRunTimeFractionAtSpeed.resize( this->numSpeeds, 0.0 );
		if ( this->speedControl == speedControlDiscrete && this->numSpeeds > 1 ) {
			//should have field sets 
			this->flowFractionAtSpeed.resize( this->numSpeeds, 0.0 );
			this->powerFractionAtSpeed.resize( this->numSpeeds, 0.0 );
			this->powerFractionInputAtSpeed.resize( this->numSpeeds, false );
			if ( this->numSpeeds == (( numNums - 13 ) / 2 ) || this->numSpeeds == (( numNums + 1 - 13 ) / 2 ) ) {
				for ( auto loopSet = 0 ; loopSet< this->numSpeeds; ++loopSet ) {
					this->flowFractionAtSpeed[ loopSet ]  = DataIPShortCuts::rNumericArgs( 13 + loopSet * 2 + 1 );
					if ( ! DataIPShortCuts::lNumericFieldBlanks( 13 + loopSet * 2 + 2  )  ) {
						this->powerFractionAtSpeed[ loopSet ] = DataIPShortCuts::rNumericArgs( 13 + loopSet * 2 + 2 );
						this->powerFractionInputAtSpeed[ loopSet ] = true;
					} else {
						this->powerFractionInputAtSpeed[ loopSet ] = false;
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
			for ( auto loop = 0; loop < (this->numSpeeds - 1); ++loop ) {
				if ( this->flowFractionAtSpeed[ loop ] >  this->flowFractionAtSpeed[ loop + 1 ]) {
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
		if ( this->speedControl == speedControlDiscrete && this->numSpeeds > 1 && this->powerModFuncFlowFractionCurveIndex == 0 ) {
			bool foundMissingPowerFraction = false;
			for ( auto loop = 0 ; loop< this->numSpeeds; ++loop ) {
				if ( ! this->powerFractionInputAtSpeed[ loop ] ) {
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

		SetupOutputVariable( "Fan Electric Power [W]", this->fanPower, "System", "Average", this->name );
		SetupOutputVariable( "Fan Rise in Air Temperature [deltaC]", this->deltaTemp, "System", "Average", this->name );
		SetupOutputVariable( "Fan Electric Energy [J]", this->fanEnergy, "System", "Sum", this->name, _, "Electric", "Fans", this->endUseSubcategoryName, "System" );
		if ( this->speedControl == speedControlDiscrete && this->numSpeeds == 1 ) {
			SetupOutputVariable( "Fan Runtime Fraction []", this->fanRunTimeFractionAtSpeed[ 0 ], "System", "Average", this->name );
		} else if ( this->speedControl == speedControlDiscrete && this->numSpeeds > 1 ) {
			for (auto speedLoop = 0; speedLoop < this->numSpeeds; ++speedLoop) {
				SetupOutputVariable( "Fan Runtime Fraction Speed " + General::TrimSigDigits( speedLoop + 1 ) + " []", this->fanRunTimeFractionAtSpeed[ speedLoop ], "System", "Average", this->name );
			}
		}

		if ( DataGlobals::AnyEnergyManagementSystemInModel ) {
			SetupEMSInternalVariable( "Fan Maximum Mass Flow Rate", this->name , "[kg/s]", this->maxAirMassFlowRate );
			SetupEMSActuator( "Fan", this->name , "Fan Air Mass Flow Rate", "[kg/s]", this->eMSMaxMassFlowOverrideOn, this->eMSAirMassFlowValue );
			SetupEMSInternalVariable( "Fan Nominal Pressure Rise", this->name , "[Pa]", this->deltaPress );
			SetupEMSActuator( "Fan", this->name , "Fan Pressure Rise", "[Pa]", this->eMSFanPressureOverrideOn, this->eMSFanPressureValue );
			SetupEMSInternalVariable( "Fan Nominal Total Efficiency", this->name, "[fraction]", this->fanTotalEff );
			SetupEMSActuator( "Fan",this->name , "Fan Total Efficiency", "[fraction]", this->eMSFanEffOverrideOn, this->eMSFanEffValue );
			SetupEMSActuator( "Fan", this->name , "Fan Autosized Air Flow Rate", "[m3/s]", this->maxAirFlowRateEMSOverrideOn, this->maxAirFlowRateEMSOverrideValue );
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
			if ( this->nightVentPressureDelta > 0.0 ) { 
				localPressureRise =  this->nightVentPressureDelta;
			}
			if ( this->nightVentFlowFraction > 0.0 ) {
				localFlowFraction = this->nightVentFlowFraction;
				localAirMassFlow = localFlowFraction * this->maxAirMassFlowRate;
			}
		} else { // not in night mode
			if ( present( pressureRise ) ) {
				localPressureRise = pressureRise;
			} else {
				localPressureRise = this->deltaPress;
			}
			if ( present( flowFraction ) ) {
				localFlowFraction = flowFraction;
				localAirMassFlow = localFlowFraction * this->maxAirMassFlowRate;
			} else {
				localFlowFraction = this->inletAirMassFlowRate / this->maxAirMassFlowRate;
				localAirMassFlow  = this->inletAirMassFlowRate;
			}
		}

// TODO Faulty fan operation


		//EMS override MassFlow, DeltaPress, and FanEff
		if ( this->eMSFanPressureOverrideOn ) localPressureRise = this->eMSFanPressureValue;
		if ( this->eMSFanEffOverrideOn ) localFanTotEff = this->eMSFanEffValue;
		if ( this->eMSMaxMassFlowOverrideOn ) {
			localAirMassFlow = this->eMSAirMassFlowValue;

		}

		localAirMassFlow = min( localAirMassFlow, this->maxAirMassFlowRate );
		localFlowFraction = localAirMassFlow / this->maxAirMassFlowRate;

		if ( ( ScheduleManager::GetCurrentScheduleValue( this->availSchedIndex ) > 0.0 || this->objTurnFansOn ) && ! this->objTurnFansOff && localAirMassFlow > 0.0 ) {
			//fan is running

			switch ( this->speedControl ) {
			
			case speedControlDiscrete: {
				if ( this->numSpeeds == 1 ) { // CV or OnOff
					localFanTotEff = this->fanTotalEff;
					this->fanRunTimeFractionAtSpeed[ 0 ] = localFlowFraction;
					this->fanPower = this->fanRunTimeFractionAtSpeed[ 0 ] * this->maxAirMassFlowRate * localPressureRise / ( localFanTotEff * this->rhoAirStdInit );
					Real64 fanShaftPower = this->motorEff * this->fanPower;
					Real64 powerLossToAir = fanShaftPower + ( this->fanPower - fanShaftPower )* this->motorInAirFrac;
					this->outletAirEnthalpy = this->inletAirEnthalpy + powerLossToAir / localAirMassFlow;
					this->outletAirHumRat = this->inletAirHumRat;
					this->outletAirMassFlowRate =  localAirMassFlow;
					this->outletAirTemp = Psychrometrics::PsyTdbFnHW( this->outletAirEnthalpy, this->outletAirHumRat );
				} else if ( this->numSpeeds > 1 ) { // multi speed

					// find which two speed levels bracket flow fraction and calculate runtimefraction
					int lowSideSpeed = -1;
					int hiSideSpeed  = -1;
					for ( auto loop = 0; loop < this->numSpeeds; ++loop ) {
						this->fanRunTimeFractionAtSpeed[ loop ] = 0.0;
					}

					if ( localFlowFraction < this->flowFractionAtSpeed[ 0 ] ) { // on/off between zero and lowest speed
						hiSideSpeed  = 0;
						this->fanRunTimeFractionAtSpeed[ 0 ] = localFlowFraction / this->flowFractionAtSpeed[ 0 ];
					} else {
						for ( auto loop = 0; loop < this->numSpeeds - 1; ++loop ) {
							if ( ( this->flowFractionAtSpeed[ loop ] <= localFlowFraction ) && ( localFlowFraction <= flowFractionAtSpeed[ loop + 1 ] ) ) {
								lowSideSpeed = loop;
								hiSideSpeed = loop +1;
								break;
							}
						}
						this->fanRunTimeFractionAtSpeed[ lowSideSpeed ] = ( this->flowFractionAtSpeed[ hiSideSpeed ] - localFlowFraction ) / ( this->flowFractionAtSpeed[ hiSideSpeed ] - this->flowFractionAtSpeed[ lowSideSpeed ] );
						this->fanRunTimeFractionAtSpeed[ hiSideSpeed ] = ( localFlowFraction - this->flowFractionAtSpeed[ lowSideSpeed ] ) / ( this->flowFractionAtSpeed[ hiSideSpeed ] - this->flowFractionAtSpeed[ lowSideSpeed ] );
					}
					this->fanPower = this->fanRunTimeFractionAtSpeed[ lowSideSpeed ] * this->massFlowAtSpeed[ lowSideSpeed ] * localPressureRise / ( this->totEfficAtSpeed[ lowSideSpeed ] * this->rhoAirStdInit ) + this->fanRunTimeFractionAtSpeed[ hiSideSpeed ] * this->massFlowAtSpeed[ hiSideSpeed ] * localPressureRise / ( this->totEfficAtSpeed[ hiSideSpeed ] * this->rhoAirStdInit );
										Real64 fanShaftPower = this->motorEff * this->fanPower;
					Real64 powerLossToAir = fanShaftPower + ( this->fanPower - fanShaftPower )* this->motorInAirFrac;
					this->outletAirEnthalpy = this->inletAirEnthalpy + powerLossToAir / localAirMassFlow;
					this->outletAirHumRat = this->inletAirHumRat;
					this->outletAirMassFlowRate =  localAirMassFlow;
					this->outletAirTemp = Psychrometrics::PsyTdbFnHW( this->outletAirEnthalpy, this->outletAirHumRat );
				}



				localFanTotEff = this->fanTotalEff;
				break;
			}
			case speedControlContinuous: {
				localFanTotEff = this->fanTotalEff;
				Real64 localFlowFractionForPower = max( this->minPowerFlowFrac, localFlowFraction );
				Real64 localPowerFraction = CurveManager::CurveValue( this->powerModFuncFlowFractionCurveIndex, localFlowFractionForPower );
				this->fanPower = localPowerFraction * this->maxAirMassFlowRate * localPressureRise / ( localFanTotEff * this->rhoAirStdInit );
				Real64 fanShaftPower = this->motorEff * this->fanPower;
				Real64 powerLossToAir = fanShaftPower + ( this->fanPower - fanShaftPower )* this->motorInAirFrac;
				this->outletAirEnthalpy = this->inletAirEnthalpy + powerLossToAir / localAirMassFlow;
				this->outletAirHumRat = this->inletAirHumRat;
				this->outletAirMassFlowRate =  localAirMassFlow;
				this->outletAirTemp = Psychrometrics::PsyTdbFnHW( this->outletAirEnthalpy, this->outletAirHumRat );
				
				// When fan air flow is less than 10%, the fan power curve is linearized between the 10% to 0% to
			//  avoid the unrealistic high temperature rise across the fan.
				Real64 deltaTAcrossFan = this->outletAirTemp - this->inletAirTemp;
				if ( deltaTAcrossFan > 20.0 ) {
					Real64 minFlowFracLimitFanHeat = 0.10;
					Real64 powerFractionAtLowMin = 0.0;
					Real64 fanPoweratLowMinimum = 0.0;
					if ( localFlowFractionForPower < minFlowFracLimitFanHeat ) {
						powerFractionAtLowMin = CurveManager::CurveValue( this->powerModFuncFlowFractionCurveIndex, minFlowFracLimitFanHeat );

						fanPoweratLowMinimum = powerFractionAtLowMin * this->maxAirMassFlowRate * localPressureRise / ( localFanTotEff * this->rhoAirStdInit );
						this->fanPower = localFlowFractionForPower * fanPoweratLowMinimum / minFlowFracLimitFanHeat;
					} else if ( localFlowFraction < minFlowFracLimitFanHeat ) {

						powerFractionAtLowMin = CurveManager::CurveValue( this->powerModFuncFlowFractionCurveIndex, minFlowFracLimitFanHeat );
						fanPoweratLowMinimum = powerFractionAtLowMin * this->maxAirMassFlowRate * localPressureRise / ( localFanTotEff * this->rhoAirStdInit );
						this->fanPower = localFlowFraction * fanPoweratLowMinimum / minFlowFracLimitFanHeat;
					}
					fanShaftPower = this->motorEff * this->fanPower; // power delivered to shaft
					powerLossToAir = fanShaftPower + ( this->fanPower - fanShaftPower ) * this->motorInAirFrac;
					this->outletAirEnthalpy = this->inletAirEnthalpy + powerLossToAir / localAirMassFlow;
					// This fan does not change the moisture or Mass Flow across the component
					this->outletAirHumRat = this->inletAirHumRat;
					this->outletAirMassFlowRate = localAirMassFlow;
					this->outletAirTemp = Psychrometrics::PsyTdbFnHW( this->outletAirEnthalpy, this->outletAirHumRat );
				}
				break;
			} // continuous speed control case
			} // end switch

		} else { // fan is off
			//Fan is off and not operating no power consumed and mass flow rate.
			this->fanPower = 0.0;

			this->outletAirMassFlowRate = 0.0;
			this->outletAirHumRat = this->inletAirHumRat;
			this->outletAirEnthalpy = this->inletAirEnthalpy;
			this->outletAirTemp = this->inletAirTemp;
			// Set the Control Flow variables to 0.0 flow when OFF.
			this->massFlowRateMaxAvail = 0.0;
			this->massFlowRateMinAvail = 0.0;
		}

	}

	void
	FanSystem::update() const // does not change state of object, only update elsewhere
	{
		// Set the outlet air node of the fan
		DataLoopNode::Node( this->outletNodeNum ).MassFlowRate = this->outletAirMassFlowRate;
		DataLoopNode::Node( this->outletNodeNum ).Temp         = this->outletAirTemp;
		DataLoopNode::Node( this->outletNodeNum ).HumRat       = this->outletAirHumRat;
		DataLoopNode::Node( this->outletNodeNum ).Enthalpy     = this->outletAirEnthalpy;
		// Set the outlet nodes for properties that just pass through & not used
		DataLoopNode::Node( this->outletNodeNum ).Quality = DataLoopNode::Node( this->inletNodeNum ).Quality;
		DataLoopNode::Node( this->outletNodeNum ).Press   = DataLoopNode::Node( this->inletNodeNum ).Press;

		// Set the Node Flow Control Variables from the Fan Control Variables
		DataLoopNode::Node( this->outletNodeNum ).MassFlowRateMaxAvail = this->massFlowRateMaxAvail;
		DataLoopNode::Node( this->outletNodeNum ).MassFlowRateMinAvail = this->massFlowRateMinAvail;

		if ( DataContaminantBalance::Contaminant.CO2Simulation ) {
			DataLoopNode::Node( this->outletNodeNum ).CO2 = DataLoopNode::Node( this->inletNodeNum ).CO2;
		}

		if ( DataContaminantBalance::Contaminant.GenericContamSimulation ) {
			DataLoopNode::Node( this->outletNodeNum ).GenContam = DataLoopNode::Node( this->inletNodeNum ).GenContam;
		}

		//ugly use of global here
		DataHVACGlobals::FanElecPower = this->fanPower;
		DataAirLoop::LoopOnOffFanRTF  = this->fanRunTimeFractionAtSpeed[ this->numSpeeds - 1 ]; //fill with RTF from highest speed level

	}

	void
	FanSystem::report()
	{
		this->fanEnergy = this->fanPower * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
		this->deltaTemp = this->outletAirTemp - this->inletAirTemp;
	}

	std::string
	FanSystem::getFanName() const
	{
		return this->name;
	}

	Real64
	FanSystem::getFanDesignVolumeFlowRate() const
	{
		return this->designAirVolFlowRate;
	}

	int
	FanSystem::getFanInletNode() const
	{
		return this->inletNodeNum;
	}

	int
	FanSystem::getFanOutletNode() const
	{
		return this->outletNodeNum;
	}

	int
	FanSystem::getFanAvailSchIndex() const
	{
		return this->availSchedIndex;
	}

	int
	FanSystem::getFanPowerCurveIndex() const
	{
		return this->powerModFuncFlowFractionCurveIndex;
	}

	Real64
	FanSystem::getFanDesignTemperatureRise() const
	{
		if ( ! this->objSizingFlag ) {
			Real64 cpAir = Psychrometrics::PsyCpAirFnWTdb( DataPrecisionGlobals::constant_zero, DataPrecisionGlobals::constant_twenty );
			Real64 designDeltaT = ( this->deltaPress / ( this->rhoAirStdInit * cpAir * this->fanTotalEff ) ) * ( this->motorEff + this->motorInAirFrac * ( 1.0 - this->motorEff ) );
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
		if ( ! this->objSizingFlag ) {
			Real64 fanPowerTot = ( FanVolFlow * this->deltaPress ) / this->fanTotalEff ;
			Real64 designHeatGain = this->motorEff * fanPowerTot + ( fanPowerTot - this->motorEff * fanPowerTot ) * this->motorInAirFrac;
			return designHeatGain;
		} else {
			this->set_size();
			Real64 fanPowerTot = ( FanVolFlow * this->deltaPress ) / this->fanTotalEff ;
			Real64 designHeatGain = this->motorEff * fanPowerTot + ( fanPowerTot - this->motorEff * fanPowerTot ) * this->motorInAirFrac;
			return designHeatGain;

		}
	}

	bool
	FanSystem::getIfContinuousSpeedControl() const 
	{
		if (this->speedControl == speedControlContinuous ) {
			return true;
		} else {
			return false;
		}
	}

} //HVACFan namespace

} // EnergyPlus namespace
