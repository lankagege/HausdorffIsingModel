/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * IsingModel.cpp                                                              *
 * Author: Evan Coleman and Brad Marston, 2016                                 *
 *                                                                             *
 * Class structure for Ising model simulation. Key characteristics:            *
 *  - Generates lattice of a given Hausdorff dimension and depth               *
 *  - Multithreaded computation of partition function                          *
 *  - Multithreaded Monte Carlo steps                                          *
 *                                                                             *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include <pthread.h>
#include <cstdlib>
#include <vector>
#include <iostream>

class IsingModel {
    public :
        // Constructors, destructor
        IsingModel();
        virtual ~IsingModel() = {};
        
        // Settings
        void setNumThreads        (const int num    );
        void setNumMCSteps        (const int num    );
        void setLatticeDepth      (const int num    );
        void setHausdorffDimension(const double dim );
        void setHausdorffMethod   (const char* hmtd );
        void setInteractionSigma  (const double sig );    
        void setTemperature       (const double tkbT);
        void setCouplingConsts    (const double H,
                                   const double J);    

        const std::vector<int> getSpinArray();
        const std::vector<int> getLatticeDimensions();
        const int    getNumThreads()         = {return nThreads        };
        const int    getNumSpins()           = {return nSpins          };
        const int    getNumLatticePoints()   = {return nLatticePoints  };
        const int    getLatticeDepth()       = {return latticeDepth    };
        const double getHausdorffDimension() = {return hausdorffDim    };
        const double getHausdorffMethod()    = {return hausdorffMethod };
        const double getHausdorffSlices()    = {return hausdorffSlices };
        const double getHausdorffScale()     = {return hausdorffScale  };
        const double getInteractionSigma()   = {return interactionSigma};    
        const double getNumMCSteps()         = {return nMCSteps        };
        
        // Observables
        const int    getMagnetization();
        const double getFreeEnergy(
                const std::vector<int>& flips=std::vector<int>());
        const double computePartitionFunction(
                const int start=0,
                const std::vector<int>& flips=std::vector<int>());

        // Shorthand definitions
        const double K()={return J/kbT                     };
        const double h()={return H/kbT                     };
        const int    m()={return getMagnetization()        };
        const double Z()={return computePartitionFunction()};

        // Simulation
        void setup();
        void runMonteCarlo();

    private :
        // Spins
        typedef struct {
           int S;
           bool active;
           std::vector<int> coords;
        } spin;

        // Settings
        std::vector<spin> spinArray;
        std::vector<int > latticeDimensions;
        double latticeDepth=1;
        int    nThreads=1;
        int    nSpins=0;
        int    nLatticePoints=0;
        double interactionSigma=1;
        double hausdorffDim=1;
        double hausdorffSlices=2;
        double hausdorffScale=1/3;
        char*  hausdorffMethod="SCALING";
        int    nMCSteps=10000;

        // Thermodynamic variables
        double kbT=1;
        double H=1;
        double J=1;

        // Observables
        int magnetization = 0;
        
        // Simulation
        bool   hasBeenSetup=false;
        void   monteCarloStep();
        void   simpleMonteCarloStep();
        double computePartitionFunction();
         
};