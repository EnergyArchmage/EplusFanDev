
// EnergyPlus Headers
#include <FanObject.hh>
#include <EnergyPlus.hh>
#include <DataGlobals.hh>
#include <DataHVACGlobals.hh>

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
	
	}


} //FanModel namespace

} // EnergyPlus namespace
