#ifndef OPENMM_CUDANONBONDEDUTILITIES_H_
#define OPENMM_CUDANONBONDEDUTILITIES_H_

/* -------------------------------------------------------------------------- *
 *                                   OpenMM                                   *
 * -------------------------------------------------------------------------- *
 * This is part of the OpenMM molecular simulation toolkit originating from   *
 * Simbios, the NIH National Center for Physics-Based Simulation of           *
 * Biological Structures at Stanford, funded under the NIH Roadmap for        *
 * Medical Research, grant U54 GM072970. See https://simtk.org.               *
 *                                                                            *
 * Portions copyright (c) 2009-2012 Stanford University and the Authors.      *
 * Authors: Peter Eastman                                                     *
 * Contributors:                                                              *
 *                                                                            *
 * This program is free software: you can redistribute it and/or modify       *
 * it under the terms of the GNU Lesser General Public License as published   *
 * by the Free Software Foundation, either version 3 of the License, or       *
 * (at your option) any later version.                                        *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU Lesser General Public License for more details.                        *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.      *
 * -------------------------------------------------------------------------- */

#include "CudaContext.h"
#include "openmm/System.h"
#include "CudaExpressionUtilities.h"
#include <sstream>
#include <string>
#include <vector>

namespace OpenMM {

/**
 * This class provides a generic interface for calculating nonbonded interactions.  It does this in two
 * ways.  First, it can be used to create kernels that evaluate nonbonded interactions.  Clients
 * only need to provide the code for evaluating a single interaction and the list of parameters it depends on.
 * A complete kernel is then synthesized using an appropriate algorithm to evaluate all interactions on all
 * atoms.
 *
 * Second, this class itself creates and invokes a single "default" interaction kernel, allowing several
 * different forces to be evaluated at once for greater efficiency.  Call addInteraction() and addParameter()
 * to add interactions to this default kernel.
 *
 * During each force or energy evaluation, the following sequence of steps takes place:
 *
 * 1. Data structures (e.g. neighbor lists) are calculated to allow nonbonded interactions to be evaluated
 * quickly.
 *
 * 2. calcForcesAndEnergy() is called on each ForceImpl in the System.
 *
 * 3. Finally, the default interaction kernel is invoked to calculate all interactions that were added
 * to it.
 *
 * This sequence means that the default interaction kernel may depend on quantities that were calculated
 * by ForceImpls during calcForcesAndEnergy().
 */

class OPENMM_EXPORT_CUDA CudaNonbondedUtilities {
public:
    class ParameterInfo;
    CudaNonbondedUtilities(CudaContext& context);
    ~CudaNonbondedUtilities();
    /**
     * Add a nonbonded interaction to be evaluated by the default interaction kernel.
     *
     * @param usesCutoff     specifies whether a cutoff should be applied to this interaction
     * @param usesPeriodic   specifies whether periodic boundary conditions should be applied to this interaction
     * @param usesExclusions specifies whether this interaction uses exclusions.  If this is true, it must have identical exclusions to every other interaction.
     * @param cutoffDistance the cutoff distance for this interaction (ignored if usesCutoff is false)
     * @param exclusionList  for each atom, specifies the list of other atoms whose interactions should be excluded
     * @param kernel         the code to evaluate the interaction
     * @param forceGroup     the force group in which the interaction should be calculated
     */
    void addInteraction(bool usesCutoff, bool usesPeriodic, bool usesExclusions, double cutoffDistance, const std::vector<std::vector<int> >& exclusionList, const std::string& kernel, int forceGroup);
    /**
     * Add a per-atom parameter that the default interaction kernel may depend on.
     */
    void addParameter(const ParameterInfo& parameter);
    /**
     * Add an array (other than a per-atom parameter) that should be passed as an argument to the default interaction kernel.
     */
    void addArgument(const ParameterInfo& parameter);
    /**
     * Specify the list of exclusions that an interaction outside the default kernel will depend on.
     * 
     * @param exclusionList  for each atom, specifies the list of other atoms whose interactions should be excluded
     */
    void requestExclusions(const std::vector<std::vector<int> >& exclusionList);
    /**
     * Initialize this object in preparation for a simulation.
     */
    void initialize(const System& system);
    /**
     * Get the number of energy buffers required for nonbonded forces.
     */
    int getNumEnergyBuffers() {
        return numForceThreadBlocks*forceThreadBlockSize;
    }
    /**
     * Get whether a cutoff is being used.
     */
    bool getUseCutoff() {
        return useCutoff;
    }
    /**
     * Get whether periodic boundary conditions are being used.
     */
    bool getUsePeriodic() {
        return usePeriodic;
    }
    /**
     * Get the number of work groups used for computing nonbonded forces.
     */
    int getNumForceThreadBlocks() {
        return numForceThreadBlocks;
    }
    /**
     * Get the size of each work group used for computing nonbonded forces.
     */
    int getForceThreadBlockSize() {
        return forceThreadBlockSize;
    }
    /**
     * Get the cutoff distance.
     */
    double getCutoffDistance() {
        return cutoff;
    }
    /**
     * Get whether any interactions have been added.
     */
    bool getHasInteractions() {
        return cutoff != -1.0;
    }
    /**
     * Get the force group in which nonbonded interactions should be computed.
     */
    int getForceGroup() {
        return nonbondedForceGroup;
    }
    /**
     * Prepare to compute interactions.  This updates the neighbor list.
     */
    void prepareInteractions();
    /**
     * Compute the nonbonded interactions.
     */
    void computeInteractions();
    /**
     * Check to see if the neighbor list arrays are large enough, and make them bigger if necessary.
     */
    void updateNeighborListSize();
    /**
     * Get the array containing the center of each atom block.
     */
    CudaArray& getBlockCenters() {
        return *blockCenter;
    }
    /**
     * Get the array containing the dimensions of each atom block.
     */
    CudaArray& getBlockBoundingBoxes() {
        return *blockBoundingBox;
    }
    /**
     * Get the array whose first element contains the number of tiles with interactions.
     */
    CudaArray& getInteractionCount() {
        return *interactionCount;
    }
    /**
     * Get the array containing tiles with interactions.
     */
    CudaArray& getInteractingTiles() {
        return *interactingTiles;
    }
    /**
     * Get the array containing flags for tiles with interactions.
     */
    CudaArray& getInteractionFlags() {
        return *interactionFlags;
    }
    /**
     * Get the array containing exclusion flags.
     */
    CudaArray& getExclusions() {
        return *exclusions;
    }
    /**
     * Get the array containing the index into the exclusion array for each tile.
     */
    CudaArray& getExclusionIndices() {
        return *exclusionIndices;
    }
    /**
     * Get the array listing where the exclusion data starts for each row.
     */
    CudaArray& getExclusionRowIndices() {
        return *exclusionRowIndices;
    }
    /**
     * Get the index of the first tile this context is responsible for processing.
     */
    int getStartTileIndex() const {
        return startTileIndex;
    }
    /**
     * Get the total number of tiles this context is responsible for processing.
     */
    int getNumTiles() const {
        return numTiles;
    }
    /**
     * Set the range of tiles that should be processed by this context.
     */
    void setTileRange(int startTileIndex, int numTiles);
    /**
     * Create a Kernel for evaluating a nonbonded interaction.  Cutoffs and periodic boundary conditions
     * are assumed to be the same as those for the default interaction Kernel, since this kernel will use
     * the same neighbor list.
     * 
     * @param source        the source code for evaluating the force and energy
     * @param params        the per-atom parameters this kernel may depend on
     * @param arguments     arrays (other than per-atom parameters) that should be passed as arguments to the kernel
     * @param useExclusions specifies whether exclusions are applied to this interaction
     * @param isSymmetric   specifies whether the interaction is symmetric
     */
    CUfunction createInteractionKernel(const std::string& source, std::vector<ParameterInfo>& params, std::vector<ParameterInfo>& arguments, bool useExclusions, bool isSymmetric);
    /**
     * This is a utility routine for locating data in the exclusions array.  It takes the (x,y) indices of a tile,
     * and returns the location in the array where the data for that tile begins.
     * 
     * This routine requires that x >= y.  If not, it will throw an exception.
     * 
     * @param x                   the x index of the tile
     * @param y                   the y index of the tile
     * @param exclusionIndices    the content of the exclusionIndices array
     * @param exclusionRowIndices the content of the exclusionRowIndices array
     * @return the index in the exclusions array at which the data for that tile begins
     */
    static int findExclusionIndex(int x, int y, const std::vector<unsigned int>& exclusionIndices, const std::vector<unsigned int>& exclusionRowIndices);
private:
    CudaContext& context;
    CUfunction forceKernel;
    CUfunction findBlockBoundsKernel;
    CUfunction findInteractingBlocksKernel;
    CUfunction findInteractionsWithinBlocksKernel;
    CudaArray* exclusions;
    CudaArray* exclusionIndices;
    CudaArray* exclusionRowIndices;
    CudaArray* interactingTiles;
    CudaArray* interactionFlags;
    CudaArray* interactionCount;
    CudaArray* blockCenter;
    CudaArray* blockBoundingBox;
    std::vector<void*> forceArgs, findBlockBoundsArgs, findInteractingBlocksArgs, findInteractionsWithinBlocksArgs;
    std::vector<std::vector<int> > atomExclusions;
    std::vector<ParameterInfo> parameters;
    std::vector<ParameterInfo> arguments;
    std::string kernelSource;
    std::map<std::string, std::string> kernelDefines;
    double cutoff;
    bool useCutoff, usePeriodic, anyExclusions;
    int startTileIndex, numTiles, maxTiles, numForceThreadBlocks, forceThreadBlockSize, nonbondedForceGroup, numAtoms;
};

/**
 * This class stores information about a per-atom parameter that may be used in a nonbonded kernel.
 */

class CudaNonbondedUtilities::ParameterInfo {
public:
    /**
     * Create a ParameterInfo object.
     *
     * @param name           the name of the parameter
     * @param type           the data type of the parameter's components
     * @param numComponents  the number of components in the parameter
     * @param size           the size of the parameter in bytes
     * @param memory         the memory containing the parameter values
     */
    ParameterInfo(const std::string& name, const std::string& componentType, int numComponents, int size, CUdeviceptr memory) :
            name(name), componentType(componentType), numComponents(numComponents), size(size), memory(memory) {
        if (numComponents == 1)
            type = componentType;
        else {
            std::stringstream s;
            s << componentType << numComponents;
            type = s.str();
        }
    }
    const std::string& getName() const {
        return name;
    }
    const std::string& getComponentType() const {
        return componentType;
    }
    const std::string& getType() const {
        return type;
    }
    int getNumComponents() const {
        return numComponents;
    }
    int getSize() const {
        return size;
    }
    CUdeviceptr& getMemory() {
        return memory;
    }
private:
    std::string name;
    std::string componentType;
    std::string type;
    int size, numComponents;
    CUdeviceptr memory;
};

} // namespace OpenMM

#endif /*OPENMM_CUDANONBONDEDUTILITIES_H_*/
