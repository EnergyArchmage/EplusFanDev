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
		int const objectNum
	);

	void
	simulate(
	
	);

public: //methods

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


}; //class FanObject 


} // Fan namespace

} // EnergyPlus namespace
#endif //FanObject_hh_INCLUDED_hh_INCLUDED
