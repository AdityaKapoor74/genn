#pragma once

// Forward declarations
class CodeStream;
class NNmodel;

namespace CodeGenerator
{
class BackendBase;
}

//--------------------------------------------------------------------------
// CodeGenerator
//--------------------------------------------------------------------------
namespace CodeGenerator
{
void generateNeuronUpdate(CodeStream &os, const NNmodel &model, const BackendBase &backend);
}