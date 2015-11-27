
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

namespace EnergyPlus {

namespace HVACFan {

	std::vector < std::unique_ptr <FanSystem> > fanObjs;

	int
	getFanObjectVectorIndex(  // lookup vector index for fan object name in object array DataHVACGlobals::fanObjs
		std::string const objectName
	)
	{
		int index = -1;
		bool found = false;
		for ( auto loop = 0; loop < fanObjs.size(); ++loop ) {
			if ( objectName == fanObjs[ loop ]->getFanName() ) {
				if ( ! found ) {
					index = loop;
					found = true;
				} else { // found duplicate
					//TODO throw warning
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
		Optional< Real64 const > pressureRise // Pressure difference to use for DeltaPress
	)
	{

		this->localTurnFansOn = false;
		this->localTurnFansOff = false;

		this->init( );

		if ( present( zoneCompTurnFansOn ) && present( zoneCompTurnFansOff ) ) {
			// Set module-level logic flags equal to ZoneCompTurnFansOn and ZoneCompTurnFansOff values passed into this routine
			// for ZoneHVAC components with system availability managers defined.
			// The module-level flags get used in the other subroutines (e.g., SimSimpleFan,SimVariableVolumeFan and SimOnOffFan)
			this->localTurnFansOn = zoneCompTurnFansOn;
			this->localTurnFansOff = zoneCompTurnFansOff;
		} else {
			// Set module-level logic flags equal to the global LocalTurnFansOn and LocalTurnFansOff variables for all other cases.
			this->localTurnFansOn = DataHVACGlobals::TurnFansOn;
			this->localTurnFansOff = DataHVACGlobals::TurnFansOff;
		}

		this->calcSimpleSystemFan();

		this->update();

		this->report();

	}

	void
	FanSystem::init()
	{ 
	
		if ( ! DataGlobals::SysSizingCalc && this->localSizingFlag ) {
			this->set_size();
			this->localSizingFlag = false;
			if ( DataSizing::CurSysNum > 0 ) {
				DataAirLoop::AirLoopControlInfo( DataSizing::CurSysNum ).CyclingFan = true;
			}
		}

		if ( DataGlobals::BeginEnvrnFlag && this->localEnvrnFlag ) {
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
			this->localEnvrnFlag =  false;
		}

		if ( ! DataGlobals::BeginEnvrnFlag ) {
			this->localEnvrnFlag = true;
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
		if ( this->designAirVolFlowRateWasAutosized ) {
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
		}

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
					this->totEfficAtSpeed[ loop ] = this->flowFractionAtSpeed[ loop ] * this->designAirVolFlowRate * this->deltaPress  / ( this->designElecPower * powerFractionAtSpeed[ loop ] );
				} else { // use power curve
					this->totEfficAtSpeed[ loop ] = this->flowFractionAtSpeed[ loop ] * this->designAirVolFlowRate * this->deltaPress  / ( this->designElecPower * CurveManager::CurveValue( this->powerModFuncFlowFractionCurveIndex, this->flowFractionAtSpeed[ loop ] ) );
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


	}

	FanSystem::FanSystem( // constructor
		std::string const objectName
	)
	{
	

		std::string const routineName = "Fan constructor ";
		int numAlphas; // Number of elements in the alpha array
		int numNums; // Number of elements in the numeric array
		int IOStat; // IO Status when calling get input subroutine
		bool errorsFound = false;
		DataIPShortCuts::cCurrentModuleObject = "Fan:SystemModel";

		int objectNum = InputProcessor::GetObjectItemNum( DataIPShortCuts::cCurrentModuleObject, objectName );

		InputProcessor::GetObjectItem( DataIPShortCuts::cCurrentModuleObject, objectNum, DataIPShortCuts::cAlphaArgs, numAlphas, DataIPShortCuts::rNumericArgs, numNums, IOStat, DataIPShortCuts::lNumericFieldBlanks, DataIPShortCuts::lAlphaFieldBlanks, DataIPShortCuts::cAlphaFieldNames, DataIPShortCuts::cNumericFieldNames  );

		this->name = DataIPShortCuts::cAlphaArgs( 1 );
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

		this->designAirVolFlowRate =  DataIPShortCuts::rNumericArgs( 1 );
		if ( this->designAirVolFlowRate = DataSizing::AutoSize ) {
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
			this->designAirVolFlowRateWasAutosized = true;
		}
		if ( this->designAirVolFlowRateWasAutosized ) {
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

				this->elecPowerPerFlowRate            = DataIPShortCuts::rNumericArgs( 7 );
				this->elecPowerPerFlowRatePerPressure = DataIPShortCuts::rNumericArgs( 8 );
				this->fanTotalEff                     = DataIPShortCuts::rNumericArgs( 9 );
			}
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


} //HVACFan namespace

} // EnergyPlus namespace
