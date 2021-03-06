/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * IsingModel.cpp                                                              *
 * Author: Evan Coleman and Brad Marston, 2016                                 *
 *                                                                             *
 * Class definitions for Ising model simulation. Key characteristics:          *
 *  - Generates lattice of a given Hausdorff dimension and depth               *
 *  - Multithreaded computation of partition function                          *
 *  - Multithreaded Monte Carlo steps                                          *
 *                                                                             *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include "interface/IsingModel.h"
#include <iomanip>

// Constructors/destructors implemented simply
// (because of number of options)
IsingModel::IsingModel() {};
IsingModel::~IsingModel() {};


/* (void) setNumThreads
 *    | How many threads to use at a time.
 *  I | (int) number of threads
 */
void IsingModel::setNumThreads(const int num) {
    if(nThreads < 1) return;
    nThreads = num;
    hasBeenSetup=false;
}


/* (void) setNumMCSteps 
 *    | How many MC steps to perform 
 *  I | (int) number of steps 
 */
void IsingModel::setNumMCSteps(const int num) {
    if(num < 1) return;
    nMCSteps = num;
    hasBeenSetup=false;
}


/* (void) setLatticeDepth
 *    | How many steps to simulate into the fractal lattice 
 *  I | (int) depth 
 */
void IsingModel::setLatticeDepth(const int num) {
    latticeDepth=num;
    hasBeenSetup=false;
}


/* (void) setInteractionSigma
 *    | The distance coupling exponent on the J_ij 
 *  I | (int) number of spins 
 */
void IsingModel::setInteractionSigma(const double sig) {
    interactionSigma=sig;
    hasBeenSetup=false;
}


/* (void) setHausdorffDimension
 *    | Set the lattice Hausdorff dimension.
 *  I | (double) dimension to use 
 */
void IsingModel::setHausdorffDimension(const double dim) {
    if(dim <= 0) return;
    hausdorffDim=dim;
    hasBeenSetup=false;
}


/* (void) setHausdorffMethod
 *    | Set the lattice Hausdorff scaling method.
 *  I | (double) method to use:
 *    |         - SCALING   = modify separation
 *    |         - SPLITTING = modify # divisions
 */
void IsingModel::setHausdorffMethod(char* const hmtd) {
    hausdorffMethod=hmtd;
    hasBeenSetup=false;
}


/* (void) setMCMethod 
 *    | Set the MC method.
 *  I | (double) method to use:
 *    |         - METROPOLIS (no multithread) 
 *    |         - HEATBATH   (multithread)
 */
void IsingModel::setMCMethod(char* const mcmd) {
    mcMethod=mcmd;
    hasBeenSetup=false;
}


/* (void) setCouplingConsts
 *    | Set the values of H,J in the hamiltonian 
 *  I | (double) value of H, magnetic field coupling 
 *    | (double) value of J, neighbor couplings
 */
void IsingModel::setCouplingConsts(const double tH, const double tJ) {
    H=tH;
    J=tJ;
    hasBeenSetup=false;
}


/* (void) setTemperature
 *    | Set the temperature of the system
 *  I | (double) value of k_B * T (>0) to use 
 */
void IsingModel::setTemperature(const double tkbT) {
    if (tkbT < 0) return;
    kbT=tkbT;
    hasBeenSetup=false;
}


/* (vector<int>) getSpinArray 
 *    | Returns an array of the spins (+1,-1, or 0)
 */
const std::vector<int> IsingModel::getSpinArray() {
    std::vector<int> spins(0);
    for(const auto &it : spinArray) {
        spins.push_back(it.active ? it.S : 0);
    }
    return spins;
}


/* (vector<int>) getLatticeDimensions 
 *    | Returns an array of the number of spins along
 *    | each lattice edge
 */
const std::vector<int> IsingModel::getLatticeDimensions() {
    return latticeDimensions;
}


/* (int) getMagnetization() 
 *    | Returns the magnetization of the lattice 
 */
const int IsingModel::getMagnetization() {
    int mag=0;
    for(const auto &it : spinArray) {
       mag += it.S*it.active;
    }
    magnetization=mag;
    return mag;
}


/* (int) getDistanceSq() 
 *    | Returns the square of the distance between two spins
 *  I | (spin) first spin
 *    | (spin) second spin
 *  O | (double) distance between the spins, or 1 if distance=0
 */
double IsingModel::getDistanceSq(spin s1, spin s2) {
    if(interactionSigma==0) return 1;
    else {
        double distance=0;
        for(int i=0; i < s1.coords.size(); i++) {
            distance += pow(s1.coords.at(i)-s2.coords.at(i),2);
        }
        if(distance==0) return 1;
        return distance;
    }
}


/* (double) getEffHamiltonian 
 *    | Get the effective energy of the system state (no multithread)
 *  I | (int) single spin to flip 
 */
const double IsingModel::getEffHamiltonian(const int flip) {
    std::vector<int> tflips;
    tflips.push_back(flip);
    return getEffHamiltonian(tflips);
}


/* (double) getEffHamiltonian 
 *    | Get the effective energy of the system state, -(beta*Hamiltonian) (no multithread)
 *  I | (vector<int> (default: empty)) array of spin indices to flip 
 */
const double IsingModel::getEffHamiltonian(const std::vector<int>& flips) {
    double energy=0;
    for(int i=0; i<nSpins; i++) {
        spin s=spinArray.at(i);
        if (!s.active) continue;
        int spinFlip=
            (std::find(flips.begin(),flips.end(),i)!=flips.end() ? -1 : 1);

        energy -= geth()*s.S*spinFlip;


        // Nearest neighbor sum, no circular boundary conditions
        for(int j=0,p=latticeDimensions.size(); j < p; j++) {
            double indexPM = pow(latticeDimensions.at(j),p-1-j);
            
            // get to the left
            if(i > indexPM-1 && s.coords.at(j) != xmin 
               && spinArray.at(i-indexPM).coords.at(j) != xmax) {
                int newIndex=i-indexPM;

                if(spinArray.at(newIndex).active) {

                    int tspinFlip=
                        (std::find(flips.begin(),flips.end(),newIndex)!=flips.end() ? -1 : 1); 
    
                    energy -= getK()*pow(getDistanceSq(s,spinArray.at(newIndex)),interactionSigma/2)
                              *s.S*spinArray.at(newIndex).S*spinFlip*tspinFlip/2;

                }
            } 

            // get to the right
            if(i + indexPM < nSpins && s.coords.at(j) != xmax 
               && spinArray.at(i+indexPM).coords.at(j) != xmin) {
                int newIndex=i+indexPM;
                
                if(spinArray.at(newIndex).active) {
            
                    int tspinFlip=
                        (std::find(flips.begin(),flips.end(),newIndex)!=flips.end() ? -1 : 1); 
    
                    energy -= getK()*pow(getDistanceSq(s,spinArray.at(newIndex)),interactionSigma/2)
                              *s.S*spinArray.at(newIndex).S*spinFlip*tspinFlip/2;

                }
            }

        }
    }
    return energy;
}


/* (double) computePartitionFunction 
 *    | Get the partition function of the system (no multithread)
 *  I | (int (default: 0)) index to start the trace at 
 *    | (vector<int> (default: empty)) array of spin indices to flip 
 */
const double IsingModel::computePartitionFunction(const int start, const std::vector<int>& flips) {
    if(start >= nSpins) return 0;
    double Z=0;

    
    std::vector<int> newflips = flips; 
    newflips.push_back(start);

    // add e^-BH with S_i = +1 and S_i = -1
    Z+=exp(getEffHamiltonian(newflips));

    // branch into computations with history 
    // S_i = +1 and S_i = -1
    Z+=computePartitionFunction(start+1,newflips);
    Z+=computePartitionFunction(start+1,flips);
    if(start == 0) Z+=exp(getEffHamiltonian());

    return Z; 
}


/* (void) nextPermutation
 *    | For a vector of indices and some uniform maximum index,
 *    | get a new tuple of indices from the current state by 
 *    | adding one to the lowest index and "carrying" over.
 *    | Return tvN[0]=-1 when finished, for the loop end condition
 *  I | (std::vector<int>) the array of indices to permute 
 *    | (int) the maximum array index
 */
void IsingModel::nextPermutation(std::vector<int>& tvN, const int max) {
    //if(debug) std::cout<<"\tNextPermutation: "<<tvN.size()<<" "<<max<<std::endl;

    for(size_t i=0; i<tvN.size(); i++) {
        if(i == tvN.size()-1) {
            if(tvN.at(i) == max-1) tvN.at(0)=-1;
            else tvN.at(i)+=1;
            return;
        } else if(tvN.at(i) == max-1) {
            tvN.at(i)=0;
        } else {
            tvN.at(i)=tvN.at(i)+1;
            return;
        }
    }
}


/* (void) addSpins 
 *    | Adds spins to the spinArray by isolating each smallest hypercube
 *    | making up the lattice at a given depth 
 *  I | (int) depth to build to
 *    | (double) vector of coordinates to start fractal at
 *    | (double) vector of coordinates to end fractal at
 */
void IsingModel::addSpins(const int depth,
        const std::vector<double>& x0, 
        const std::vector<double>& x1) {
        
    if(debug) std::cout<<"\taddSpins:"<<std::endl;

    double delta    = abs(x1.at(0)-x0.at(0));
    //double sliceLen = hausdorffScale * delta;
    //double spaceLen = (delta - sliceLen*hausdorffSlices)/(hausdorffSlices-1);

    // Create an array of length dimension, p
    // Containing arrays of length depth, d
    // Which will contain our p*d indices 
    std::vector<int> vN(latticeDimensions.size()*depth);

    // Loop over all valid positions for a spin hypercube
    for(std::fill(vN.begin(),vN.end(),0); 
        vN.at(0) != -1; 
        nextPermutation(vN,hausdorffSlices)) {
        // Current position is lower corner of hypercube we produce 
        // (think in terms of the origin for the unit hypercube, [0,1]^p) 
        std::vector<double> cPos(latticeDimensions.size());
        for(size_t iDim=0; iDim < latticeDimensions.size(); iDim++) {
            cPos.at(iDim) += x0.at(iDim);
            
            for(int iDepth=0; iDepth < depth; iDepth++) {
                int depthVal = vN.at(iDim*depth+iDepth);
                double depthScale = pow(hausdorffScale,depth-iDepth)*delta;
                cPos.at(iDim) += (1 + (1/hausdorffScale-hausdorffSlices)/(hausdorffSlices-1))
                                 *depthScale
                                 *depthVal;
            }
        }

        // - place spins at each corner of a hypercube
        // - cube will have side length s^d and bottom corner at cPos
        // - loop will go through e.g. for p=2 (0,0) , (0,1) , (1,0) , (1,1)
        std::vector<int> cubePoints(latticeDimensions.size());
        for(std::fill(cubePoints.begin(),cubePoints.end(),0);
            cubePoints.at(0) != -1;
            nextPermutation(cubePoints,2)) {
           
            spin ts;
            ts.active=1;
            ts.S=1;
            ts.coords=std::vector<double>(latticeDimensions.size());
            for(size_t index=0; index < cubePoints.size(); index++) {
                ts.coords.at(index) = cubePoints.at(index) * pow(hausdorffScale,depth)*delta
                                        + cPos.at(index);
            }


            spinArray.push_back(ts);
            nSpins++;
        } 
    }

    if(debug) std::cout<<"\t\t- spinArray made, sorting..."<<std::endl;
    QuickSort(spinArray,0,nSpins-1);

}


/* (void) setup 
 *    | Prepare the class object for simulation 
 */
void IsingModel::setup() {
    if(debug) std::cout<<"\tSETUP:"<<std::endl;
    
    // Calculate the lattice dimensions from the input
    // Hausdorff dimension
    // (See my notes for derivation)
    if(hausdorffMethod=="SCALING") {
        hausdorffScale=exp(-ceil(hausdorffDim)*log(hausdorffSlices)/hausdorffDim);
    }

    // Check that the dimensions are reasonable
    if(hausdorffScale > 1/hausdorffSlices) {
        std::cout<<"ERROR: Invalid Hausdorff scaling"<<std::endl;
        exit(EXIT_FAILURE); 
    }

    // Keep track of the number of site coordinates along each axis
    for(int i=0; i<ceil(hausdorffDim); i++) {
        latticeDimensions.push_back(2*pow(hausdorffSlices,latticeDepth));
    }

    // Generate the lattice array
    std::vector<double> x0;
    std::vector<double> x1;
    for(size_t i=0; i<latticeDimensions.size(); i++) {
        x0.push_back(0);
        x1.push_back(1);
    }

    addSpins(latticeDepth,x0,x1);

    hasBeenSetup=true;
}


/* (void) runMonteCarlo 
 *    | Run the Monte Carlo simulation (spin-flipping) 
 */
void IsingModel::runMonteCarlo() {
    if(debug) std::cout<<"\tRunMonteCarlo:"<<std::endl;
    if(!hasBeenSetup) {
        std::cout<<"ERROR: Object has not been setup!"<<std::endl;
        exit(EXIT_FAILURE); 
    }
    
    // Various utils 
    TRandom3* rNG = new TRandom3(); 

    currentEffH=getEffHamiltonian();
    double avgAbsDeltaE=-1;
    int nSpinsPerThread = floor(nSpins/nThreads);
    int cNumThreads=nThreads;
    //std::vector<boost::thread*> threads;

    // Start performing MC steps
    for(int i=0; i < nMCSteps; i++) {
        if(debug && nMCSteps < 100) std::cout<<"\t\t At MC Step "
                                             <<i<<"/"<<nMCSteps<<std::endl;


        double newAvgAbsDeltaE=0;
        for(int iDE=0,lMCI=mcInfo.size(); iDE < lMCI; iDE++)
             newAvgAbsDeltaE+=(mcInfo.at(iDE));
        mcInfo.clear();

        //std::cout<<" - "<<newAvgAbsDeltaE<<" "<<avgAbsDeltaE<<std::endl;

             if(mcMethod=="METROPOLIS") metropolisStep(rNG);
        else if(mcMethod=="HEATBATH")   heatBathStep(rNG);
        else if(mcMethod=="HYBRID") {
            
            // Prepare threads
            //int currentNThreads=0;

            // Create a vector of spin indices
            int popVectorSize=nSpins;
            std::vector<int> popVector(nSpins);
            for(int j=0; j < nSpins; j++) popVector.at(j)=j;

            if(nSpinsPerThread < 1) nSpinsPerThread=1;
            // Based upon the previous running average of Delta E,
            // choose whether to increase the number of threads
                                        
                                                                           // Decrease spins/thread:
            if(nSpinsPerThread > 1 && (newAvgAbsDeltaE > avgAbsDeltaE*2    // if energy change is growing fast
                                        || newAvgAbsDeltaE <= avgAbsDeltaE  // if we are converging on minimum
                                        || newAvgAbsDeltaE==0)) {          // if nothing is changing (stuck)
                nSpinsPerThread /= 2;
                nSpinsPerThread=max(nSpinsPerThread,1);
                if(debug) std::cout<<"\t\tHYBRID: Increasing granularity to "
                                    <<nSpinsPerThread<<" spins / thread"<<std::endl;
            // if energy change is moderate and magnetization is low, more spins/thread
            } else if(nSpinsPerThread >= 1 && abs(magnetization) < nSpins/2) {   
                nSpinsPerThread *= 2;
                nSpinsPerThread=max(nSpinsPerThread,1);
                if(debug) std::cout<<"\t\tHYBRID: Decreasing granularity to "
                                    <<nSpinsPerThread<<" spins / thread"<<std::endl;   
            }

            
            // Loop through threads
            while(popVectorSize>0) {

                // Generate array of spins to flip for thread
                // by ripping apart vector of spin indices 
                std::vector<int> spinFlips(nSpinsPerThread);
                if(nSpinsPerThread > popVector.size()) {
                    spinFlips=popVector;
                    popVector.clear();
                    popVectorSize=0;
                } else {
                    for(int j=0; j < nSpinsPerThread; j++){ 
                        int index=rNG->Integer(popVectorSize);
                        spinFlips.push_back(popVector.at(index));
                        popVector.erase(popVector.begin()+index);
                        popVectorSize--;
                    }
                }

                // Save thread info outside of scope
                // and submit the thread
                //boost::thread * tThread
                //    = new boost::thread(&IsingModel::heatBathStep,this,rNG->Uniform(),spinFlips);
                ///threads.push_back(tThread);
                hybridStep(rNG->Uniform(),spinFlips);

                //currentNThreads++;
            }

            // Let them run in sequence
            //for(int iThread; iThread<currentNThreads; iThread++) {
            //    //threads.at(iThread)->join();
            //}
        }


        if(avgAbsDeltaE >= 0) hybridInfo.push_back(avgAbsDeltaE);
        avgAbsDeltaE=newAvgAbsDeltaE;   
            
    }

    delete rNG;
}


/* (void) metropolisStep 
 *    | Perform one run over the lattice, using Metropolis acceptance function
 *  I | (TRandom3*) pointer to random number generator
 */
double IsingModel::metropolisStep(TRandom3* rNG) {

    // loop over spins
    for(int i=0; i < nSpins; i++) {
        double tE = getEffHamiltonian(i);
        bool spinFlip=false;

        //std::cout<<"\t\t\t-F= "<<currentEffH<<", vs temp: "<<tE<<std::endl;

        if(tE-currentEffH<0) spinFlip = true;
        else spinFlip = (rNG->Uniform() < exp(currentEffH-tE));

        if(spinFlip) {
            spinArray.at(i).S=-spinArray.at(i).S;
            mcInfo.push_back(abs(tE-currentEffH));            
            currentEffH=tE;
        }
    }

    return currentEffH;
}


/* (void) heatBathStep 
 *    | Perform one run over the lattice, using the Heat Bath acceptance function
 *  I | (TRandom3*) pointer to random number generator
 */
double IsingModel::heatBathStep(TRandom3* rNG) {

    // loop over spins
    for(int i=0; i < nSpins; i++) {
        double tE = getEffHamiltonian(i);

        //std::cout<<"\t\t\t-F= "<<currentEffH<<", vs temp: "<<tE<<std::endl;

        bool spinFlip=false;
        double acceptance = exp(currentEffH-tE);

        if(std::isinf(acceptance)) {
            spinFlip=true;
        } else if(acceptance > 0) {
            acceptance/=(exp(tE-currentEffH)+exp(currentEffH-tE));
            spinFlip = (rNG->Uniform() < acceptance);
        }

        if(spinFlip) {
            spinArray.at(i).S=-spinArray.at(i).S;
            mcInfo.push_back(abs(tE-currentEffH));
            currentEffH=tE;
        }
    }


    return currentEffH;
}

/* (void) hybridStep 
 *    | Perform a single group spin flip for the HYBRID MC method
 *  I | (double) a random number between 0 and 1
 *    | (std::vector<int>) a list of indices to flip in the spingroup
 */
void IsingModel::hybridStep(const double rng, const std::vector<int>& spinFlips) {

    double tE = getEffHamiltonian(spinFlips);
    bool spinFlip=false;

    if(tE-currentEffH<0) spinFlip=true;
    else spinFlip = (rng < exp(currentEffH-tE));
        
    if(spinFlip) {
        for(size_t i=0; i<spinFlips.size(); i++) {
            spinArray.at(spinFlips.at(i)).S=-spinArray.at(spinFlips.at(i)).S;
        }
        mcInfo.push_back(abs(tE-currentEffH));
        currentEffH=tE;
    }

}


/* (void) reset 
 *    | Reset the Ising Model class
 */
void IsingModel::reset() {
    if(debug) std::cout<<"\tReset:"<<std::endl;
    spinArray.clear();
    hybridInfo.clear();
    mcInfo.clear();
    latticeDimensions.clear();

    magnetization=0;
    currentEffH=0;
    nSpins=0;

    hasBeenSetup=false;
}


/* (void) status 
 *    | Return the status of the model
 */
void IsingModel::status() {
    std::cout<<"\t\t| Magnetization:   "<<getMagnetization()     <<std::endl;
    std::cout<<"\t\t| Eff. energy:     "<<getEffHamiltonian()    <<std::endl;
    std::cout<<"\t\t| Hausdorff dim.:  "<<getHausdorffDimension()<<std::endl;
    std::cout<<"\t\t| Lattice copies:  "<<getHausdorffSlices()   <<std::endl;
    std::cout<<"\t\t| Lattice scaling: "<<getHausdorffScale()    <<std::endl;
    std::cout<<"\t\t| Number of spins: "<<getNumSpins()          <<std::endl;
    std::cout<<"\t\t| MC Method:       "<<getMCMethod()          <<std::endl;
    std::cout<<"\t\t| Number MC steps: "<<getNumMCSteps()        <<std::endl;
    std::cout<<"\t\t| Number threads:  "<<getNumThreads()        <<std::endl;
    std::cout<<"\t\t|                  "<<getNumThreads()        <<std::endl;
    std::cout<<"\t\t| Beta * Hamiltonian: "<<"-1/"<<kbT<<" * "    <<std::endl;
    std::cout<<"\t\t|                     "<<"("<<J<<"/|r_i-r_j|^"
                                           <<getInteractionSigma()<<" * S_i*S_j"<<std::endl;
    std::cout<<"\t\t|                     "<<" + "<<H<<"*S_i)"<<std::endl;

    if(!hasBeenSetup) 
        std::cout<<"\n\t\t WARNING: Model has not been setup properly!"<<std::endl;

}


/* (void) swap (for QUICKSORT)
 *    | Swap two spins in an array
 */
void IsingModel::swap(spin* a, spin* b) {
    spin t = *a;
    *a = *b;
    *b = t;
}


/* (void) QSPartition (for QUICKSORT)
 *    | Splits the sort into a sort within a small range
 */
 int IsingModel::QSPartition (std::vector<spin>& vec, int low, int high) {
    spin pivot = vec[high];
    int i = (low - 1);

    for (int j = low; j <= high - 1; j++)
    {
        if (!(vec[j] > pivot)) {
            i++;
            swap(&vec[i], &vec[j]);
        }
    }
    swap(&vec[i + 1], &vec[high]);
    return (i + 1);
}


/* (void) QuickSort
 *    | Tail-recursive quick sorting. Must be used, as other sorting methods
 *    | are either too slow or cause a stack overflow.
 */
void IsingModel::QuickSort(std::vector<spin>& vec, int left, int right) {
    while (left < right)
    {
        int pi = QSPartition(vec, left, right);

        if (pi - left < right - pi) {
            QuickSort(vec, left, pi - 1);
            left = pi + 1; 
        } else {
            QuickSort(vec, pi + 1, right);
            right = pi - 1;
        }
    }
}


/* (void) randomizeSpins
 *    | Randomly flips spins in the array (does not necessarily lead to 0 mag.)
 */
void IsingModel::randomizeSpins() {
    TRandom3 *rNG = new TRandom3(0);
    int nFlips=0;

    for(int i=0; i < spinArray.size(); i++) {
        if(rNG->Uniform() < 0.5) {
          spinArray.at(i).S = spinArray.at(i).S * -1;
          nFlips++;
        }
    }

    if(debug) std::cout<<"\tRandomizeSpins:\n\t\t- flipped "
                       <<nFlips<<"/"<<nSpins<<std::endl;

    delete rNG;
}


/* (void) setAllSpins
 *    | Sets all spins in one direction
 *  I | (int) direction (+1 or -1) to set the spins
 */
void IsingModel::setAllSpins(const int direction) {
    int allSpin = (direction > 0) ? 1: -1;
    for(int i=0; i<spinArray.size(); i++) {
        spinArray.at(i).S=allSpin;
    }
}

/* (TGraph*) getConvergenceGr
 *    | Get a graph of the convergence statistics for the MC passes
 *  O | (TGraph*) dynamically allocated graph of the convergence
 */
TGraph* IsingModel::getConvergenceGr() {
    std::vector<double> convergenceDt = hybridInfo;
    convergenceDt.erase(convergenceDt.begin());
    std::vector<double> stepIndices(convergenceDt.size());
    
    for(int i=0; i < stepIndices.size(); i++) {
        stepIndices.at(i)=i+1;
    }

    TGraph *convergenceGr 
        = new TGraph(convergenceDt.size(),
                     stepIndices.data(),
                     convergenceDt.data());
    return convergenceGr;
}
