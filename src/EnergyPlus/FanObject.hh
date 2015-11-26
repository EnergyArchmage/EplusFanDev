#ifndef FanObject_hh_INCLUDED
#define FanObject_hh_INCLUDED

// C++ Headers
#include <string>
#include <vector>
#include <memory>

//#include <ObjexxFCL/Optional.hh>


// EnergyPlus Headers
#include <EnergyPlus.hh>
#include <DataGlobals.hh>
#include <DataHVACGlobals.hh>


namespace EnergyPlus {

namespace FanModel {

	int
	getFanObjectVectorIndex(
		std::string const objectName
	);

class Fan {

private: // Creation
	// Default Constructor
	Fan() :
	name( "")
	{}

	// Copy Constructor
	Fan( Fan const & ) = default;

	// Move Constructor
#if !defined(_MSC_VER) || defined(__INTEL_COMPILER) || (_MSC_VER>=1900)
	Fan( Fan && ) = default;
#endif

public: // Methods
	// Destructor
	~Fan()
	{}

	// Constructor
	Fan(
		std::string const objectName
	);

	void
	simulate(
	
	);

public: //methods

	std::string
	getFanName();

	Real64
	getFanVolFlow();

	Real64
	getFanPower();

	Real64
	getFanDesignVolumeFlowRate(
		bool & errorsFound
	);

	int
	getFanInletNode(
		bool & errorsFound
	);

	int
	getFanOutletNode(
		bool & errorsFound
	);

	int
	getFanAvailSchIndex(
		bool & errorsFound
	);

	int
	getFanPowerCurveIndex();

	int
	getFanDesignTemperatureRise();

	Real64 
	getFanDesignHeatGain(
		Real64 const FanVolFlow // fan volume flow rate [m3/s]
	);


private: //methods
	void
	init();

	void
	set_size();

	void
	calcSimpleSystemFan();

	void
	update() const; 

	void
	report();


private: // data

	enum speedControlMethodEnum {
		speedControlNotSet,
		speedControlDiscrete,
		speedControlContinuous
	};
	enum powerSizingMethodEnum {
		powerSizingMethodNotSet,
		powerPerFlow,
		powerPerFlowPerPressure,
		totalEfficiencyAndPressure
	
	};
	enum thermalLossDestinationEnum {
		heatLossNotDetermined,
		zoneGains,
		lostToOutside
	};
	//input data
	std::string name; // user identifier
	std::string fanType; // Type of Fan ie. Simple, Vane axial, Centrifugal, etc.
	int fanType_Num; // DataHVACGlobals fan type
	int availSchedIndex; // Pointer to the availability schedule
	int inletNodeNum; // system air node at fan inlet
	int outletNodeNum; // system air node at fan outlet
	Real64 designAirVolFlowRate; // Max Specified Volume Flow Rate of Fan [m3/sec]
	bool designAirVolFlowRateWasAutosized; // true if design max volume flow rate was autosize on input
	speedControlMethodEnum speedControl; // Discrete or Continuous speed control method
	Real64 minPowerFlowFrac; // Minimum fan air flow fraction for power calculation
	Real64 deltaPress; // Delta Pressure Across the Fan [N/m2]
	Real64 motorEff; // Fan motor efficiency
	Real64 motorInAirFrac; // Fraction of motor heat entering air stream
	Real64 designElecPower; // design electric power consumption [W]
	bool designElecPowerWasAutosized;
	powerSizingMethodEnum powerSizingMethod; // sizing method for design electric power, three options
	Real64 elecPowerPerFlowRate; // scaling factor for powerPerFlow method
	Real64 elecPowerPerFlowRatePerPressure; // scaling factor for powerPerFlowPerPressure
	Real64 fanTotalEff; // Fan total system efficiency (fan*belt*motor*VFD)
	int powerModFuncFlowFractionCurveIndex; // pointer to performance curve or table
	Real64 nightVentPressureDelta; // fan pressure rise during night ventilation mode
	Real64 nightVentFlowFraction; // fan flow fraction during night ventilation mode
	int zoneNum; // zone index for motor heat losses as internal gains
	Real64 zoneRadFract; // thermal radiation split for motor losses
	thermalLossDestinationEnum heatLossesDestination; //enum for where motor loss go
	std::string endUseSubcategoryName;
	int numSpeeds; // input for how many speed levels for discrete fan
	std::vector < Real64 > flowFractionSpeed; //array of flow fractions for speed levels
	std::vector < Real64 > powerFractionSpeed; // array of power fractions for speed levels
	//calculation variables
	Real64 inletAirMassFlowRate; // MassFlow through the Fan being Simulated [kg/Sec]
	Real64 outletAirMassFlowRate;
	Real64 minAirFlowRate; // Min Specified Volume Flow Rate of Fan [m3/sec]
	Real64 maxAirMassFlowRate; // Max flow rate of fan in kg/sec
	Real64 minAirMassFlowRate; // Min flow rate of fan in kg/sec
//	int fanMinAirFracMethod; // parameter for what method is used for min flow fraction
//	Real64 fanFixedMin; // Absolute minimum fan air flow [m3/s]
	Real64 inletAirTemp;
	Real64 outletAirTemp;
	Real64 inletAirHumRat;
	Real64 outletAirHumRat;
	Real64 inletAirEnthalpy;
	Real64 outletAirEnthalpy;
	//report variables
	Real64 fanPower; // Power of the Fan being Simulated [kW]
	Real64 fanEnergy; // Fan energy in [kJ]
//	Real64 fanRuntimeFraction; // Fraction of the timestep that the fan operates
	Real64 deltaTemp; // Temp Rise across the Fan [C]
	std::vector < Real64 > fanRunTimeFractionSpeed;
	//EMS related variables
	bool maxAirFlowRateEMSOverrideOn; // if true, EMS wants to override fan size for Max Volume Flow Rate
	Real64 maxAirFlowRateEMSOverrideValue; // EMS value to use for override of  Max Volume Flow Rate
	bool eMSFanPressureOverrideOn; // if true, then EMS is calling to override
	Real64 eMSFanPressureValue; // EMS value for Delta Pressure Across the Fan [Pa]
	bool eMSFanEffOverrideOn; // if true, then EMS is calling to override
	Real64 eMSFanEffValue; // EMS value for total efficiency of the Fan, fraction on 0..1
	bool eMSMaxMassFlowOverrideOn; // if true, then EMS is calling to override mass flow
	Real64 eMSAirMassFlowValue; // value EMS is directing to use [kg/s]

	bool faultyFilterFlag; // Indicate whether there is a fouling air filter corresponding to the fan
	int faultyFilterIndex;  // Index of the fouling air filter corresponding to the fan



	// Mass Flow Rate Control Variables
	Real64 massFlowRateMaxAvail;
	Real64 massFlowRateMinAvail;
	Real64 rhoAirStdInit;

//	int NVPerfNum;
//	int FanPowerRatAtSpeedRatCurveIndex;
//	int FanEffRatioCurveIndex;

	bool oneTimePowerCurveCheck; // one time flag used for error message
//	bool OneTimeEffRatioCheck; // one time flag used for error message

}; //class FanObject 


} // Fan namespace

} // EnergyPlus namespace
#endif //FanObject_hh_INCLUDED_hh_INCLUDED
