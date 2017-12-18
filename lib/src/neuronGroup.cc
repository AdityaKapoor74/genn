#include "neuronGroup.h"

// Standard includes
#include <algorithm>
#include <cmath>

// GeNN includes
#include "codeGenUtils.h"
#include "standardSubstitutions.h"
#include "synapseGroup.h"
#include "utils.h"

// ------------------------------------------------------------------------
// NeuronGroup
// ------------------------------------------------------------------------
void NeuronGroup::checkNumDelaySlots(unsigned int requiredDelay)
{
    if (requiredDelay >= getNumDelaySlots())
    {
        m_NumDelaySlots = requiredDelay + 1;
    }
}

void NeuronGroup::updateVarQueues(const string &code)
{
    // Loop through variables
    const auto vars = getNeuronModel()->getVars();
    for(size_t i = 0; i < vars.size(); i++) {
        const std::string &varName = vars[i].first;

        // If the code contains a reference to this variable, set corresponding flag
        if (code.find(varName + "_pre") != string::npos) {
            m_AnyVarQueuesRequired = true;
            m_VarQueueRequired[i] = true;
        }
    }
}

void NeuronGroup::setVarMode(const std::string &varName, VarMode mode)
{
    m_VarMode[getNeuronModel()->getVarIndex(varName)] = mode;
}

VarMode NeuronGroup::getVarMode(const std::string &varName) const
{
    return m_VarMode[getNeuronModel()->getVarIndex(varName)];
}

void NeuronGroup::addSpkEventCondition(const std::string &code, const std::string &supportCodeNamespace)
{
    m_SpikeEventCondition.insert(std::pair<std::string, std::string>(code, supportCodeNamespace));
}

void NeuronGroup::initDerivedParams(double dt)
{
    auto derivedParams = getNeuronModel()->getDerivedParams();

    // Reserve vector to hold derived parameters
    m_DerivedParams.reserve(derivedParams.size());

    // Loop through derived parameters
    for(const auto &d : derivedParams) {
        m_DerivedParams.push_back(d.second(m_Params, dt));
    }

    // Initialise derived parameters for variable initialisers
    for(auto &v : m_VarInitialisers) {
        v.initDerivedParams(dt);
    }
}

void NeuronGroup::calcSizes(unsigned int blockSize,  unsigned int &idStart, unsigned int &paddedIDStart)
{
    // paddedSize is the lowest multiple of neuronBlkSz >= neuronN[i]
    const unsigned int paddedSize = ceil((double)getNumNeurons() / (double) blockSize) * (double) blockSize;

    // Store current cummulative sum in first
    m_IDRange.first = idStart;
    m_PaddedIDRange.first = paddedIDStart;

    // Update global cummulative sums of neurons
    idStart += getNumNeurons();
    paddedIDStart +=  paddedSize;

    // Store cummulative sums of point after this neuron group
    m_IDRange.second = idStart;
    m_PaddedIDRange.second = paddedIDStart;
}

bool NeuronGroup::isVarQueueRequired(const std::string &var) const
{
    // Return flag corresponding to variable
    return m_VarQueueRequired[getNeuronModel()->getVarIndex(var)];
}

bool NeuronGroup::isZeroCopyEnabled() const
{
    // If any bits of spikes require zero-copy return true
    if(isSpikeZeroCopyEnabled() || isSpikeEventZeroCopyEnabled() || isSpikeTimeZeroCopyEnabled()) {
        return true;
    }

    // If there are any variables implemented in zero-copy mode return true
    if(any_of(m_VarMode.begin(), m_VarMode.end(),
        [](VarMode mode){ return (mode & VarLocation::ZERO_COPY); }))
    {
        return true;
    }

    return false;
}

bool NeuronGroup::isParamRequiredBySpikeEventCondition(const std::string &pnamefull) const
{
    // Loop through event conditions
    for(const auto &spkEventCond : m_SpikeEventCondition) {
        // If the event threshold code contains this parameter
        // (in it's non-uniquified form), set flag and stop searching
        if(spkEventCond.first.find(pnamefull) != string::npos) {
            return true;
        }
    }

    return false;
}

void NeuronGroup::addExtraGlobalParams(std::map<string, string> &kernelParameters) const
{
    for(auto const &p : getNeuronModel()->getExtraGlobalParams()) {
        std::string pnamefull = p.first + getName();
        if (kernelParameters.find(pnamefull) == kernelParameters.end()) {
            // parameter wasn't registered yet - is it used?
            if (getNeuronModel()->getSimCode().find("$(" + p.first + ")") != string::npos
                || getNeuronModel()->getThresholdConditionCode().find("$(" + p.first + ")") != string::npos
                || getNeuronModel()->getResetCode().find("$(" + p.first + ")") != string::npos) {
                kernelParameters.insert(pair<string, string>(pnamefull, p.second));
            }
        }
    }
}

bool NeuronGroup::isInitCodeRequired() const
{
    // Return true if any of the variables initialisers have any code
    return std::any_of(m_VarInitialisers.cbegin(), m_VarInitialisers.cend(),
                       [](const NewModels::VarInit &v)
                       {
                           return !v.getSnippet()->getCode().empty();
                       });
}

bool NeuronGroup::isSimRNGRequired() const
{
    // Returns true if any parts of the neuron code require an RNG
    if(::isRNGRequired(getNeuronModel()->getSimCode())
        || ::isRNGRequired(getNeuronModel()->getThresholdConditionCode())
        || ::isRNGRequired(getNeuronModel()->getResetCode()))
    {
        return true;
    }

    // Loop through incoming synapse groups
    for(const auto *sg : getInSyn()) {
        // Return true if any parts of the postsynaptic model require an RNG
        // **NOTE** these are included as they are simulated in the neuron kernel/function
        if(::isRNGRequired(sg->getPSModel()->getApplyInputCode())
            || ::isRNGRequired(sg->getPSModel()->getDecayCode()))
        {
            return true;
        }
    }

    return false;
}

bool NeuronGroup::isInitRNGRequired() const
{
    // Return true if any of the variables initialisers use rngs
    return std::any_of(m_VarInitialisers.cbegin(), m_VarInitialisers.cend(),
                       [](const NewModels::VarInit &v)
                       {
                           return ::isRNGRequired(v.getSnippet()->getCode());
                       });

    // **TODO** return true if any PSM variable initialisers use rngs
}

std::string NeuronGroup::getQueueOffset(const std::string &devPrefix) const
{
    return isDelayRequired()
        ? "(" + devPrefix + "spkQuePtr" + getName() + " * " + to_string(getNumNeurons()) + ") + "
        : "";
}