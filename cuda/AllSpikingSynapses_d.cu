/*
 * AllSpikingSynapses.cu
 *
 */

#include "AllSpikingSynapses.h"
#include "Book.h"

/*
 *  Set some parameters used for advanceSynapsesDevice.
 *  Currently we set a member variable: m_fpChangePSR_h.
 */
void AllSpikingSynapses::setAdvanceSynapsesDeviceParams()
{
    getFpChangePSR(m_fpChangePSR_h);
}

/*
 *  Advance all the Synapses in the simulation.
 *  Update the state of all synapses for a time step.
 *
 *  @param  allSynapsesDevice      Reference to the allSynapses struct on device memory.
 *  @param  allNeuronsDevice       Reference to the allNeurons struct on device memory.
 *  @param  synapseIndexMapDevice  Reference to the SynapseIndexMap on device memory.
 *  @param  sim_info               SimulationInfo class to read information from.
 */
void AllSpikingSynapses::advanceSynapses(IAllSynapses* allSynapsesDevice, IAllNeurons* allNeuronsDevice, void* synapseIndexMapDevice, const SimulationInfo *sim_info)
{
    // CUDA parameters
    const int threadsPerBlock = 256;
    int blocksPerGrid = ( total_synapse_counts + threadsPerBlock - 1 ) / threadsPerBlock;

    // Advance synapses ------------->
    advanceSpikingSynapsesDevice <<< blocksPerGrid, threadsPerBlock >>> ( total_synapse_counts, (SynapseIndexMap*)synapseIndexMapDevice, g_simulationStep, sim_info->deltaT, (AllSpikingSynapses*)allSynapsesDevice, (void (*)(AllSpikingSynapses*, const uint32_t, const uint64_t, const BGFLOAT))m_fpChangePSR_h );
}

/*
 *  Get a pointer to the device function preSpikeHit.
 *  The function will be called from advanceNeuronsDevice device function.
 *  Because we cannot use virtual function (Polymorphism) in device functions,
 *  we use this scheme.
 *
 *  @param  fpPreSpikeHit_h       Reference to the memory location
 *                                where the function pointer will be set.
 */
void AllSpikingSynapses::getFpPreSpikeHit(unsigned long long& fpPreSpikeHit_h)
{
    unsigned long long *fpPreSpikeHit_d;

    HANDLE_ERROR( cudaMalloc(&fpPreSpikeHit_d, sizeof(unsigned long long)) );

    getFpSpikingSynapsesPreSpikeHitDevice<<<1,1>>>((void (**)(const uint32_t, AllSpikingSynapses*))fpPreSpikeHit_d);

    HANDLE_ERROR( cudaMemcpy(&fpPreSpikeHit_h, fpPreSpikeHit_d, sizeof(unsigned long long), cudaMemcpyDeviceToHost) );

    HANDLE_ERROR( cudaFree( fpPreSpikeHit_d ) );
}

/*
 *  Get a pointer to the device function ostSpikeHit.
 *  The function will be called from advanceNeuronsDevice device function.
 *  Because we cannot use virtual function (Polymorphism) in device functions,
 *  we use this scheme.
 *
 *  @param  fpostSpikeHit_h       Reference to the memory location
 *                                where the function pointer will be set.
 */
void AllSpikingSynapses::getFpPostSpikeHit(unsigned long long& fpPostSpikeHit_h)
{
    unsigned long long *fpPostSpikeHit_d;

    HANDLE_ERROR( cudaMalloc(&fpPostSpikeHit_d, sizeof(unsigned long long)) );

    getFpSpikingSynapsesPostSpikeHitDevice<<<1,1>>>((void (**)(const uint32_t, AllSpikingSynapses*))fpPostSpikeHit_d);

    HANDLE_ERROR( cudaMemcpy(&fpPostSpikeHit_h, fpPostSpikeHit_d, sizeof(unsigned long long), cudaMemcpyDeviceToHost) );

    HANDLE_ERROR( cudaFree( fpPostSpikeHit_d ) );
}

/*
 *  Get a pointer to the device function changeSpikingSynapsesPSR.
 *  The function will be called from advanceSpikingSynapsesDevice device function.
 *  Because we cannot use virtual function (Polymorphism) in device functions,
 *  we use this scheme.
 *
 *  @param  fpChangePSR_h         Reference to the memory location
 *                                where the function pointer will be set.
 */
void AllSpikingSynapses::getFpChangePSR(unsigned long long& fpChangePSR_h)
{
    unsigned long long *fpChangePSR_d;

    HANDLE_ERROR( cudaMalloc(&fpChangePSR_d, sizeof(unsigned long long)) );

    getFpSpikingSynapsesChangePSRDevice<<<1,1>>>((void (**)(AllSpikingSynapses*, const uint32_t, const uint64_t, const BGFLOAT))fpChangePSR_d);

    HANDLE_ERROR( cudaMemcpy(&fpChangePSR_h, fpChangePSR_d, sizeof(unsigned long long), cudaMemcpyDeviceToHost) );
    HANDLE_ERROR( cudaFree( fpChangePSR_d ) );
}

/* ------------------*\
|* # Global Functions
\* ------------------*/

/* 
 * @param[in] total_synapse_counts       Total number of synapses.
 * @param[in] synapseIndexMap            Inverse map, which is a table indexed by an input neuron and maps to the synapses that provide input to that neuron.
 * @param[in] simulationStep             The current simulation step.
 * @param[in] deltaT                     Inner simulation step duration.
 * @param[in] allSynapsesDevice  Pointer to Synapse structures in device memory.
 */
__global__ void advanceSpikingSynapsesDevice ( int total_synapse_counts, SynapseIndexMap* synapseIndexMapDevice, uint64_t simulationStep, const BGFLOAT deltaT, AllSpikingSynapses* allSynapsesDevice, void (*fpChangePSR)(AllSpikingSynapses*, const uint32_t, const uint64_t, const BGFLOAT) ) {
        int idx = blockIdx.x * blockDim.x + threadIdx.x;
        if ( idx >= total_synapse_counts )
                return;

        uint32_t iSyn = synapseIndexMapDevice->activeSynapseIndex[idx];

        BGFLOAT &psr = allSynapsesDevice->psr[iSyn];
        BGFLOAT decay = allSynapsesDevice->decay[iSyn];

        // Checks if there is an input spike in the queue.
        bool isFired = isSpikingSynapsesSpikeQueueDevice(allSynapsesDevice, iSyn);

        // is an input in the queue?
        if (isFired) {
                fpChangePSR(allSynapsesDevice, iSyn, simulationStep, deltaT);
        }
        // decay the post spike response
        psr *= decay;
}

__device__ bool isSpikingSynapsesSpikeQueueDevice(AllSpikingSynapses* allSynapsesDevice, uint32_t iSyn)
{
    uint32_t &delay_queue = allSynapsesDevice->delayQueue[iSyn];
    int &delayIdx = allSynapsesDevice->delayIdx[iSyn];
    int ldelayQueue = allSynapsesDevice->ldelayQueue[iSyn];

    uint32_t delayMask = (0x1 << delayIdx);
    bool isFired = delay_queue & (delayMask);
    delay_queue &= ~(delayMask);
    if ( ++delayIdx >= ldelayQueue ) {
            delayIdx = 0;
    }

    return isFired;
}

__global__ void getFpSpikingSynapsesPreSpikeHitDevice(void (**fpPreSpikeHit_d)(const uint32_t, AllSpikingSynapses*))
{
    *fpPreSpikeHit_d = preSpikingSynapsesSpikeHitDevice;
}

__global__ void getFpSpikingSynapsesPostSpikeHitDevice(void (**fpPostSpikeHit_d)(const uint32_t, AllSpikingSynapses*))
{
    *fpPostSpikeHit_d = postSpikingSynapsesSpikeHitDevice;
}

__device__ void preSpikingSynapsesSpikeHitDevice( const uint32_t iSyn, AllSpikingSynapses* allSynapsesDevice ) {
        uint32_t &delay_queue = allSynapsesDevice->delayQueue[iSyn];
        int delayIdx = allSynapsesDevice->delayIdx[iSyn];
        int ldelayQueue = allSynapsesDevice->ldelayQueue[iSyn];
        int total_delay = allSynapsesDevice->total_delay[iSyn];

        // Add to spike queue

        // calculate index where to insert the spike into delayQueue
        int idx = delayIdx +  total_delay;
        if ( idx >= ldelayQueue ) {
                idx -= ldelayQueue;
        }

        // set a spike
        //assert( !(delay_queue[0] & (0x1 << idx)) );
        delay_queue |= (0x1 << idx);
}

__device__ void postSpikingSynapsesSpikeHitDevice( const uint32_t iSyn, AllSpikingSynapses* allSynapsesDevice ) {
}

__global__ void getFpSpikingSynapsesChangePSRDevice(void (**fpChangePSR_d)(AllSpikingSynapses*, const uint32_t, const uint64_t, const BGFLOAT))
{
    *fpChangePSR_d = changeSpikingSynapsesPSR;
}

__device__ void changeSpikingSynapsesPSR(AllSpikingSynapses* allSynapsesDevice, const uint32_t iSyn, const uint64_t simulationStep, const BGFLOAT deltaT)
{
    BGFLOAT &psr = allSynapsesDevice->psr[iSyn];
    BGFLOAT &W = allSynapsesDevice->W[iSyn];
    BGFLOAT &decay = allSynapsesDevice->decay[iSyn];

    psr += ( W / decay );    // calculate psr
}

/*
 * Adds a synapse to the network.  Requires the locations of the source and
 * destination neurons.
 * @param allSynapsesDevice      Pointer to the Synapse structures in device memory.
 * @param type                   Type of the Synapse to create.
 * @param src_neuron             Index of the source neuron.
 * @param dest_neuron            Index of the destination neuron.
 * @param source_x               X location of source.
 * @param source_y               Y location of source.
 * @param dest_x                 X location of destination.
 * @param dest_y                 Y location of destination.
 * @param sum_point              Pointer to the summation point.
 * @param deltaT                 The time step size.
 * @param W_d                    Array of synapse weight.
 * @param num_neurons            The number of neurons.
 */
__device__ void addSpikingSynapse(AllSpikingSynapses* allSynapsesDevice, synapseType type, const int src_neuron, const int dest_neuron, int source_index, int dest_index, BGFLOAT *sum_point, const BGFLOAT deltaT, BGFLOAT* W_d, int num_neurons, void (*fpCreateSynapse)(AllSpikingSynapses*, const int, const int, int, int, BGFLOAT*, const BGFLOAT, synapseType))
{
    if (allSynapsesDevice->synapse_counts[src_neuron] >= allSynapsesDevice->maxSynapsesPerNeuron) {
        return; // TODO: ERROR!
    }

    // add it to the list
    size_t synapse_index;
    size_t max_synapses = allSynapsesDevice->maxSynapsesPerNeuron;
    uint32_t iSync = max_synapses * src_neuron;
    for (synapse_index = 0; synapse_index < max_synapses; synapse_index++) {
        if (!allSynapsesDevice->in_use[iSync + synapse_index]) {
            break;
        }
    }

    allSynapsesDevice->synapse_counts[src_neuron]++;

    // create a synapse
    fpCreateSynapse(allSynapsesDevice, src_neuron, synapse_index, source_index, dest_index, sum_point, deltaT, type );
    allSynapsesDevice->W[iSync + synapse_index] = W_d[src_neuron * num_neurons + dest_neuron] * synSign(type) * AllSynapses::SYNAPSE_STRENGTH_ADJUSTMENT;
}

/*
 * Remove a synapse from the network.
 * @param[in] allSynapsesDevice         Pointer to the Synapse structures in device memory.
 * @param neuron_index   Index of a neuron.
 * @param synapse_index  Index of a synapse.
 * @param[in] maxSynapses        Maximum number of synapses per neuron.
 */
__device__ void eraseSpikingSynapse( AllSpikingSynapses* allSynapsesDevice, const int neuron_index, const int synapse_index, int maxSynapses )
{
    uint32_t iSync = maxSynapses * neuron_index + synapse_index;
    allSynapsesDevice->synapse_counts[neuron_index]--;
    allSynapsesDevice->in_use[iSync] = false;
    allSynapsesDevice->summationPoint[iSync] = NULL;
}

/*
 * Returns the type of synapse at the given coordinates
 * @param[in] allNeuronsDevice          Pointer to the Neuron structures in device memory.
 * @param src_neuron             Index of the source neuron.
 * @param dest_neuron            Index of the destination neuron.
 */
__device__ synapseType synType( neuronType* neuron_type_map_d, const int src_neuron, const int dest_neuron )
{
    if ( neuron_type_map_d[src_neuron] == INH && neuron_type_map_d[dest_neuron] == INH )
        return II;
    else if ( neuron_type_map_d[src_neuron] == INH && neuron_type_map_d[dest_neuron] == EXC )
        return IE;
    else if ( neuron_type_map_d[src_neuron] == EXC && neuron_type_map_d[dest_neuron] == INH )
        return EI;
    else if ( neuron_type_map_d[src_neuron] == EXC && neuron_type_map_d[dest_neuron] == EXC )
        return EE;

    return STYPE_UNDEF;

}

/*
 * Return 1 if originating neuron is excitatory, -1 otherwise.
 * @param[in] t  synapseType I to I, I to E, E to I, or E to E
 * @return 1 or -1
 */
__device__ int synSign( synapseType t )
{
        switch ( t )
        {
        case II:
        case IE:
                return -1;
        case EI:
        case EE:
                return 1;
        }

        return 0;
}
