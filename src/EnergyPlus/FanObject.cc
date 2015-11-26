
// EnergyPlus Headers
#include <FanObject.hh>
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

namespace EnergyPlus {

namespace FanModel {

	std::vector < std::unique_ptr <HVACFan> > fanObjs;

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
	HVACFan::simulate(
		bool const firstHVACIteration,
		Optional< Real64 const > speedRatio,
		Optional_bool_const zoneCompTurnFansOn, // Turn fans ON signal from ZoneHVAC component
		Optional_bool_const zoneCompTurnFansOff, // Turn Fans OFF signal from ZoneHVAC component
		Optional< Real64 const > pressureRise // Pressure difference to use for DeltaPress
	)
	{

		this->localTurnFansOn = false;
		this->localTurnFansOff = false;

		this->init( firstHVACIteration );

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
	init(
		bool const firstHVACIteration
	)
	{
	
	}

	HVACFan::HVACFan( // constructor
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
		this->fanRunTimeFractionSpeed.resize(this->numSpeeds, 0.0);
		if ( this->speedControl == speedControlDiscrete && this->numSpeeds > 1 ) {
			//should have field sets 
			this->flowFractionSpeed.resize(this->numSpeeds, 0.0);
			this->powerFractionSpeed.resize(this->numSpeeds, 0.0);

			if ( this->numSpeeds == (( numNums - 13 ) / 2 ) || this->numSpeeds == (( numNums + 1 - 13 ) / 2 ) ) {
				for ( auto loopSet = 0 ; loopSet< this->numSpeeds; ++loopSet ) {
					this->flowFractionSpeed[ loopSet ]  = DataIPShortCuts::rNumericArgs( 13 + loopSet * 2 + 1 );
					this->powerFractionSpeed[ loopSet ] = DataIPShortCuts::rNumericArgs( 13 + loopSet * 2 + 2 );
				}
			} else {
				// field set input does not match number of speeds, throw warning
				ShowSevereError( routineName + DataIPShortCuts::cCurrentModuleObject + "=\"" + DataIPShortCuts::cAlphaArgs( 1 ) + "\", invalid entry." );
				ShowContinueError( "Fan with Discrete speed control does not have input for speed data that matches the number of speeds.");
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
			SetupOutputVariable( "Fan Runtime Fraction []", this->fanRunTimeFractionSpeed[ 0 ], "System", "Average", this->name );
		} else if ( this->speedControl == speedControlDiscrete && this->numSpeeds > 1 ) {
			for (auto speedLoop = 0; speedLoop < this->numSpeeds; ++speedLoop) {
				SetupOutputVariable( "Fan Runtime Fraction Speed " + General::TrimSigDigits( speedLoop + 1 ) + " []", this->fanRunTimeFractionSpeed[ speedLoop ], "System", "Average", this->name );
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


} //FanModel namespace

} // EnergyPlus namespace
