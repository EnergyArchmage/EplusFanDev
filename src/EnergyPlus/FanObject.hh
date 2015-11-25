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

	std::string name; // user identifier
	std::string fanType; // Type of Fan ie. Simple, Vane axial, Centrifugal, etc.
	std::string availSchedName; // Fan Operation Schedule
	int fanType_Num; // DataHVACGlobals fan type
	int availSchedIndex; // Pointer to the availability schedule
	Real64 inletAirMassFlowRate; // MassFlow through the Fan being Simulated [kg/Sec]
	Real64 outletAirMassFlowRate;
	Real64 maxAirVolFlowRate; // Max Specified Volume Flow Rate of Fan [m3/sec]
	bool maxAirVolFlowRateWasAutosized; // true if design max volume flow rate was autosize on input

	bool maxAirFlowRateEMSOverrideOn; // if true, EMS wants to override fan size for Max Volume Flow Rate
	Real64 maxAirFlowRateEMSOverrideValue; // EMS value to use for override of  Max Volume Flow Rate
	Real64 minAirFlowRate; // Min Specified Volume Flow Rate of Fan [m3/sec]
	Real64 maxAirMassFlowRate; // Max flow rate of fan in kg/sec
	Real64 minAirMassFlowRate; // Min flow rate of fan in kg/sec
	int fanMinAirFracMethod; // parameter for what method is used for min flow fraction
	Real64 fanMinFrac; // Minimum fan air flow fraction
	Real64 fanFixedMin; // Absolute minimum fan air flow [m3/s]
	bool eMSMaxMassFlowOverrideOn; // if true, then EMS is calling to override mass flow
	Real64 eMSAirMassFlowValue; // value EMS is directing to use [kg/s]
	Real64 inletAirTemp;
	Real64 outletAirTemp;
	Real64 inletAirHumRat;
	Real64 outletAirHumRat;
	Real64 inletAirEnthalpy;
	Real64 outletAirEnthalpy;
	Real64 fanPower; // Power of the Fan being Simulated [kW]
	Real64 fanEnergy; // Fan energy in [kJ]
	Real64 fanRuntimeFraction; // Fraction of the timestep that the fan operates
	Real64 deltaTemp; // Temp Rise across the Fan [C]
	Real64 deltaPress; // Delta Pressure Across the Fan [N/m2]
	bool eMSFanPressureOverrideOn; // if true, then EMS is calling to override
	Real64 eMSFanPressureValue; // EMS value for Delta Pressure Across the Fan [Pa]

	Real64 fanEff; // Fan total system efficiency (fan*belt*motor*VFD)
	bool eMSFanEffOverrideOn; // if true, then EMS is calling to override
	Real64 eMSFanEffValue; // EMS value for total efficiency of the Fan, fraction on 0..1
	bool faultyFilterFlag; // Indicate whether there is a fouling air filter corresponding to the fan
	int faultyFilterIndex;  // Index of the fouling air filter corresponding to the fan
	Real64 motEff; // Fan motor efficiency
	Real64 motInAirFrac; // Fraction of motor heat entering air stream
	int powerModFuncFlowFractionCurveIndex; // pointer to performance curve or table

	// Mass Flow Rate Control Variables
	Real64 massFlowRateMaxAvail;
	Real64 massFlowRateMinAvail;
	Real64 rhoAirStdInit;
	int inletNodeNum;
	int outletNodeNum;
//	int NVPerfNum;
//	int FanPowerRatAtSpeedRatCurveIndex;
//	int FanEffRatioCurveIndex;
	std::string endUseSubcategoryName;
	bool oneTimePowerCurveCheck; // one time flag used for error message
//	bool OneTimeEffRatioCheck; // one time flag used for error message

}; //class FanObject 


} // Fan namespace

} // EnergyPlus namespace
#endif //FanObject_hh_INCLUDED_hh_INCLUDED
