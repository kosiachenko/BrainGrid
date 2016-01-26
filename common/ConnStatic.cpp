#include "ConnStatic.h"
#include "ParseParamError.h"
#include "IAllSynapses.h"
#include "XmlRecorder.h"
#ifdef USE_HDF5
#include "Hdf5Recorder.h"
#endif
#include <algorithm>

ConnStatic::ConnStatic() : Connections()
{
    threshConnsRadius = 0;
    nConnsPerNeuron = 0;
    pRewiring = 0;
}

ConnStatic::~ConnStatic()
{
    cleanupConnections();
}

/*
 *  Setup the internal structure of the class (allocate memories and initialize them).
 *  Initialize the small world network characterized by parameters: 
 *  number of maximum connections per neurons, connection radius threshold, and
 *  small-world rewiring probability.
 *
 *  @param  sim_info  SimulationInfo class to read information from.
 *  @param  layout    Layout information of the neunal network.
 *  @param  neurons   The Neuron list to search from.
 *  @param  synapses  The Synapse list to search from.
 */
void ConnStatic::setupConnections(const SimulationInfo *sim_info, Layout *layout, IAllNeurons *neurons, IAllSynapses *synapses)
{
    int num_neurons = sim_info->totalNeurons;
    vector<DistDestNeuron> distDestNeurons[num_neurons];

    int added = 0;

    DEBUG(cout << "Initializing connections" << endl;)

    for (int src_neuron = 0; src_neuron < num_neurons; src_neuron++) {
        distDestNeurons[src_neuron].clear();

        // pick the connections shorter than threshConnsRadius
        for (int dest_neuron = 0; dest_neuron < num_neurons; dest_neuron++) {
            if (src_neuron != dest_neuron) {
                BGFLOAT dist = (*layout->dist)(src_neuron, dest_neuron);
                if (dist <= threshConnsRadius) {
                    DistDestNeuron distDestNeuron;
                    distDestNeuron.dist = dist;
                    distDestNeuron.dest_neuron = dest_neuron;
                    distDestNeurons[src_neuron].push_back(distDestNeuron);
                }
            }
        }

        // sort ascendant
        sort(distDestNeurons[src_neuron].begin(), distDestNeurons[src_neuron].end());

        // pick the shortest nConnsPerNeuron connections
        for (int i = 0; i < distDestNeurons[src_neuron].size() && i < nConnsPerNeuron; i++) {
            int dest_neuron = distDestNeurons[src_neuron][i].dest_neuron;
            synapseType type = layout->synType(src_neuron, dest_neuron);
            BGFLOAT* sum_point = &( dynamic_cast<AllNeurons*>(neurons)->summation_map[dest_neuron] );

            DEBUG_MID (cout << "source: " << src_neuron << " dest: " << dest_neuron << " dist: " << distDestNeurons[src_neuron][i].dist << endl;)

            uint32_t iSyn;
            synapses->addSynapse(iSyn, type, src_neuron, dest_neuron, sum_point, sim_info->deltaT);
            added++;
        }
    }

    int nRewiring = added * pRewiring;

    DEBUG(cout << "Rewiring connections: " << nRewiring << endl;)

    DEBUG (cout << "added connections: " << added << endl << endl << endl;)
}

/*
 *  Cleanup the class.
 */
void ConnStatic::cleanupConnections()
{
}

/*
 *  Attempts to read parameters from a XML file.
 *  @param  element TiXmlElement to examine.
 *  @return true if successful, false otherwise.
 */
bool ConnStatic::readParameters(const TiXmlElement& element)
{
    if (element.ValueStr().compare("ConnectionsParams") == 0) {
        // number of maximum connections per neurons
        if (element.QueryIntAttribute("nConnsPerNeuron", &nConnsPerNeuron) != TIXML_SUCCESS) {
                throw ParseParamError("nConnsPerNeuron", "Static Connections param 'nConnsPerNeuron' missing in XML.");
        }
        if (nConnsPerNeuron < 0) {
                throw ParseParamError("nConnsPerNeuron", "Invalid negative Growth param 'nConnsPerNeuron' value.");
        }

        // Connection radius threshold
        if (element.QueryFLOATAttribute("threshConnsRadius", &threshConnsRadius) != TIXML_SUCCESS) {
                throw ParseParamError("threshConnsRadius", "Static Connections param 'threshConnsRadius' missing in XML.");
        }
        if (threshConnsRadius < 0) {
                throw ParseParamError("threshConnsRadius", "Invalid negative Growth param 'threshConnsRadius' value.");
        }

        // Small-world rewiring probability
        if (element.QueryFLOATAttribute("pRewiring", &pRewiring) != TIXML_SUCCESS) {
                throw ParseParamError("pRewiring", "Static Connections param 'pRewiring' missing in XML.");
        }
        if (pRewiring < 0 || pRewiring > 1.0) {
                throw ParseParamError("pRewiring", "Invalid negative Growth param 'pRewiring' value.");
        }
    }

    return true;
}

/*
 *  Prints out all parameters of the connections to ostream.
 *
 *  @param  output  ostream to send output to.
 */
void ConnStatic::printParameters(ostream &output) const
{
}

/*
 *  Reads the intermediate connection status from istream.
 *
 *  @param  input    istream to read status from.
 *  @param  sim_info SimulationInfo class to read information from.
 */
void ConnStatic::readConns(istream& input, const SimulationInfo *sim_info)
{
}

/*
 *  Writes the intermediate connection status to ostream.
 *
 *  @param  output   ostream to write status to.
 *  @param  sim_info SimulationInfo class to read information from.
 */
void ConnStatic::writeConns(ostream& output, const SimulationInfo *sim_info)
{
}

/*
 *  Creates a recorder class object for the connection.
 *
 *  @param  stateOutputFileName  Name of the state output file.
 *                               This function tries to create either Xml recorder or
 *                               Hdf5 recorder based on the extension of the file name.
 *  @param  model                Poiner to the model class object.
 *  @param  simInfo              SimulationInfo to refer from.
 *  @return Pointer to the recorder class object.
 */
IRecorder* ConnStatic::createRecorder(const string &stateOutputFileName, IModel *model, const SimulationInfo *simInfo)
{
    // create & init simulation recorder
    IRecorder* simRecorder = NULL;
    if (stateOutputFileName.find(".xml") != string::npos) {
        simRecorder = new XmlRecorder(model, simInfo);
    }
#ifdef USE_HDF5
    else if (stateOutputFileName.find(".h5") != string::npos) {
        simRecorder = new Hdf5Recorder(model, simInfo);
    }
#endif // USE_HDF5
    else {
        return NULL;
    }
    if (simRecorder != NULL) {
        simRecorder->init(stateOutputFileName);
    }

    return simRecorder;
}
