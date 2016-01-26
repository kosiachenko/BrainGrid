/**
 *      @file AllSpikingSynapses.h
 *
 *      @brief A container of all spiking synapse data
 */

/** 
 * @authors Aaron Oziel, Sean Blackbourn
 * 
 * @class AllSpikingSynapses AllSpikingSynapses.h "AllSpikingSynapses.h"
 *
 * \latexonly  \subsubsection*{Implementation} \endlatexonly
 * \htmlonly   <h3>Implementation</h3> \endhtmlonly
 *
 *  The container holds synapse parameters of all synapses. 
 *  Each kind of synapse parameter is stored in a 2D array. Each item in the first 
 *  dimention of the array corresponds with each neuron, and each item in the second
 *  dimension of the array corresponds with a synapse parameter of each synapse of the neuron. 
 *  Bacause each neuron owns different number of synapses, the number of synapses 
 *  for each neuron is stored in a 1D array, synapse_counts.
 *
 *  For CUDA implementation, we used another structure, AllSynapsesDevice, where synapse
 *  parameters are stored in 1D arrays instead of 2D arrays, so that device functions
 *  can access these data less latency. When copying a synapse parameter, P[i][j],
 *  from host to device, it is stored in P[i * max_synapses_per_neuron + j] in 
 *  AllSynapsesDevice structure.
 *
 *  The latest implementation uses the identical data struture between host and CUDA;
 *  that is, synapse parameters are stored in a 1D array, so we don't need conversion
 *  when copying data between host and device memory.
 *
 * \latexonly  \subsubsection*{Credits} \endlatexonly
 * \htmlonly   <h3>Credits</h3> \endhtmlonly
 *
 * Some models in this simulator is a rewrite of CSIM (2006) and other
 * work (Stiber and Kawasaki (2007?))
 */
#pragma once

#include "AllSynapses.h"
#include "AllSpikingNeurons.h"

class AllSpikingSynapses : public AllSynapses
{
    public:
        AllSpikingSynapses();
        AllSpikingSynapses(const int num_neurons, const int max_synapses);
        virtual ~AllSpikingSynapses();

        /**
         *  Setup the internal structure of the class (allocate memories and initialize them).
         *
         *  @param  sim_info  SimulationInfo class to read information from.
         */
        virtual void setupSynapses(SimulationInfo *sim_info);

        /**
         *  Cleanup the class (deallocate memories).
         */
        virtual void cleanupSynapses();

        /**
         *  Reset time varying state vars and recompute decay.
         *
         *  @param  iSyn     Index of the synapse to set.
         *  @param  deltaT   Inner simulation step duration
         */
        virtual void resetSynapse(const uint32_t iSyn, const BGFLOAT deltaT);

        /**
         *  Create a Synapse and connect it to the model.
         *
         *  @param  synapses    The synapse list to reference.
         *  @param  iSyn        Index of the synapse to set.
         *  @param  source      Coordinates of the source Neuron.
         *  @param  dest        Coordinates of the destination Neuron.
         *  @param  sum_point   Summation point address.
         *  @param  deltaT      Inner simulation step duration.
         *  @param  type        Type of the Synapse to create.
         */
        virtual void createSynapse(const uint32_t iSyn, int source_index, int dest_index, BGFLOAT* sp, const BGFLOAT deltaT, synapseType type);

        /**
         *  Check if the back propagation (notify a spike event to the pre neuron)
         *  is allowed in the synapse class.
         *
         *  @retrun true if the back propagation is allowed.
         */
        virtual bool allowBackPropagation();

    protected:
        /**
         *  Setup the internal structure of the class (allocate memories and initialize them).
         *
         *  @param  num_neurons   Total number of neurons in the network.
         *  @param  max_synapses  Maximum number of synapses per neuron.
         */
        virtual void setupSynapses(const int num_neurons, const int max_synapses);

        /**
         *  Initializes the queues for the Synapse.
         *
         *  @param  iSyn   index of the synapse to set.
         */
        virtual void initSpikeQueue(const uint32_t iSyn);

        /**
         *  Updates the decay if the synapse selected.
         *
         *  @param  iSyn    Index of the synapse to set.
         *  @param  deltaT  Inner simulation step duration
         *  @return true is success.
         */
        bool updateDecay(const uint32_t iSyn, const BGFLOAT deltaT);

        /**
         *  Sets the data for Synapse to input's data.
         *
         *  @param  input  istream to read from.
         *  @param  iSyn   Index of the synapse to set.
         */
        virtual void readSynapse(istream &input, const uint32_t iSyn);

        /**
         *  Write the synapse data to the stream.
         *
         *  @param  output  stream to print out to.
         *  @param  iSyn    Index of the synapse to print out.
         */
        virtual void writeSynapse(ostream& output, const uint32_t iSyn) const;

#if defined(USE_GPU)
    public:
        /**
         *  Advance all the Synapses in the simulation.
         *  Update the state of all synapses for a time step.
         *
         *  @param  allSynapsesDevice      Reference to the allSynapses struct on device memory.
         *  @param  allNeuronsDevice       Reference to the allNeurons struct on device memory.
         *  @param  synapseIndexMapDevice  Reference to the SynapseIndexMap on device memory.
         *  @param  sim_info               SimulationInfo class to read information from.
         */
        virtual void advanceSynapses(IAllSynapses* allSynapsesDevice, IAllNeurons* allNeuronsDevice, void* synapseIndexMapDevice, const SimulationInfo *sim_info);

        /**
         *  Get a pointer to the device function preSpikeHit.
         *  The function will be called from advanceNeuronsDevice device function.
         *  Because we cannot use virtual function (Polymorphism) in device functions,
         *  we use this scheme.
         *
         *  @param  fpPreSpikeHit_h       Reference to the memory location
         *                                where the function pointer will be set.
         */
        virtual void getFpPreSpikeHit(unsigned long long& fpPreSpikeHit_h);

        /**
         *  Get a pointer to the device function ostSpikeHit.
         *  The function will be called from advanceNeuronsDevice device function.
         *  Because we cannot use virtual function (Polymorphism) in device functions,
         *  we use this scheme.
         *
         *  @param  fpostSpikeHit_h       Reference to the memory location
         *                                where the function pointer will be set.
         */
        virtual void getFpPostSpikeHit(unsigned long long& fpPostSpikeHit_h);

        /**
         *  Set some parameters used for advanceSynapsesDevice.
         *  Currently we set a member variable: m_fpChangePSR_h.
         */
        virtual void setAdvanceSynapsesDeviceParams();

        /**
         *  Get a pointer to the device function changePSR.
         *  The function will be called from advanceSynapsesDevice device function.
         *  Because we cannot use virtual function (Polymorphism) in device functions,
         *  we use this scheme.
         *
         *  @param  fpChangePSR_h         Reference to the memory location
         *                                where the function pointer will be set.
         */
        virtual void getFpChangePSR(unsigned long long& fpChangePSR_h);
#else  // !defined(USE_GPU)
public:
        /**
         *  Advance one specific Synapse.
         *
         *  @param  iSyn      Index of the Synapse to connect to.
         *  @param  sim_info  SimulationInfo class to read information from.
         *  @param  neurons   The Neuron list to search from.
         */
        virtual void advanceSynapse(const uint32_t iSyn, const SimulationInfo *sim_info, IAllNeurons *neurons);

        /**
         *  Prepares Synapse for a spike hit.
         *
         *  @param  iSyn   Index of the Synapse to update.
         */
        virtual void preSpikeHit(const uint32_t iSyn);

        /**
         *  Prepares Synapse for a spike hit (for back propagation).
         *
         *  @param  iSyn   Index of the Synapse to update.
         */
        virtual void postSpikeHit(const uint32_t iSyn);

    protected:
        /**
         *  Checks if there is an input spike in the queue.
         *
         *  @param  iSyn   Index of the Synapse to connect to.
         *  @return true if there is an input spike event.
         */
        bool isSpikeQueue(const uint32_t iSyn);

        /**
         *  Calculate the post synapse response after a spike.
         *
         *  @param  iSyn        Index of the synapse to set.
         *  @param  deltaT      Inner simulation step duration.
         */
        virtual void changePSR(const uint32_t iSyn, const BGFLOAT deltaT);
#endif

    public:

        /**
         *  The decay for the psr.
         */
        BGFLOAT *decay;

        /**
         *  The synaptic time constant \f$\tau\f$ [units=sec; range=(0,100)].
         */
        BGFLOAT *tau;

#define BYTES_OF_DELAYQUEUE         ( sizeof(uint32_t) / sizeof(uint8_t) )
#define LENGTH_OF_DELAYQUEUE        ( BYTES_OF_DELAYQUEUE * 8 )

        /**
         *  The synaptic transmission delay, descretized into time steps.
         */
        int *total_delay;

        /**
         *  Pointer to the delayed queue.
         */
        uint32_t *delayQueue;

        /**
         *  The index indicating the current time slot in the delayed queue
         *  Note: This variable is used in GpuSim_struct.cu but I am not sure 
         *  if it is actually from a synapse. Will need a little help here. -Aaron
         *  Note: This variable can be GLOBAL VARIABLE, but need to modify the code.
         */
        int *delayIdx;

        /**
         *  Length of the delayed queue.
         */
        int *ldelayQueue;

    protected:
        /**
         *  Pointer to the changePSR device function.
         */
        unsigned long long m_fpChangePSR_h;
};

#if defined(__CUDACC__)
//! Perform updating synapses for one time step.
extern __global__ void advanceSpikingSynapsesDevice ( int total_synapse_counts, SynapseIndexMap* synapseIndexMapDevice, uint64_t simulationStep, const BGFLOAT deltaT, AllSpikingSynapses* allSynapsesDevice, void (*fpChangePSR)(AllSpikingSynapses*, const uint32_t, const uint64_t, const BGFLOAT) );

extern __global__ void getFpSpikingSynapsesPreSpikeHitDevice(void (**fpPreSpikeHit_d)(const uint32_t, AllSpikingSynapses*));

extern __global__ void getFpSpikingSynapsesPostSpikeHitDevice(void (**fpPostSpikeHit_d)(const uint32_t, AllSpikingSynapses*));

extern __global__ void getFpSpikingSynapsesChangePSRDevice(void (**fpChangePSR_d)(AllSpikingSynapses*, const uint32_t, const uint64_t, const BGFLOAT));

extern __device__ bool isSpikingSynapsesSpikeQueueDevice(AllSpikingSynapses* allSynapsesDevice, uint32_t iSyn);

extern __device__ void preSpikingSynapsesSpikeHitDevice( const uint32_t iSyn, AllSpikingSynapses* allSynapsesDevice );

extern __device__ void postSpikingSynapsesSpikeHitDevice( const uint32_t iSyn, AllSpikingSynapses* allSynapsesDevice );

extern __device__ void changeSpikingSynapsesPSR(AllSpikingSynapses* allSynapsesDevice, const uint32_t, const uint64_t, const BGFLOAT deltaT);

//! Add a synapse to the network.
extern __device__ void addSpikingSynapse( AllSpikingSynapses* allSynapsesDevice, synapseType type, const int src_neuron, const int dest_neuron, int source_index, int dest_index, BGFLOAT *sum_point, const BGFLOAT deltaT, BGFLOAT* W_d, int num_neurons, void (*fpCreateSynapse)(AllSpikingSynapses*, const int, const int, int, int, BGFLOAT*, const BGFLOAT, synapseType) );

//! Remove a synapse from the network.
extern __device__ void eraseSpikingSynapse( AllSpikingSynapses* allSynapsesDevice, const int neuron_index, const int synapse_index, int maxSynapses );

//! Get the type of synapse.
extern __device__ synapseType synType( neuronType* neuron_type_map_d, const int src_neuron, const int dest_neuron );

//! Get the type of synapse (excitatory or inhibitory)
extern __device__ int synSign( synapseType t );
#endif

