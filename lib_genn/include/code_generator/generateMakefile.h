#pragma once

// Standard C++ includes
#include <string>
#include <vector>

// Forward declarations
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
void generateMakefile(std::ostream &os, const BackendBase &backend,
                      const std::vector<std::string> &moduleNames);
}