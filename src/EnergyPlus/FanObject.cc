
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

namespace EnergyPlus {

namespace FanModel {

	int
	getFanObjectVectorIndex(  // lookup vector index for fan object name in object array DataHVACGlobals::fanObjs
		std::string const objectName
	)
	{
		int index = -1;
		bool found = false;
		for ( auto loop = 0; loop < DataHVACGlobals::fanObjs.size(); ++loop ) {
			if ( objectName == DataHVACGlobals::fanObjs[ loop ]->getFanName() ) {
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
	Fan::simulate()
	{
	
	
	}

	Fan::Fan( // constructor
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


	}


} //FanModel namespace

} // EnergyPlus namespace
