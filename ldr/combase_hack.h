#pragma once

#include "hack.h"

class combase_hack: public arm64_hack
{
  public:
    combase_hack(arm64_pe_file *pe, exports_dict *ed)
     : arm64_hack(pe, ed)
    {
      zero_data();
    }
    virtual ~combase_hack()
    { }
    int hack(int verbose);
    void dump() const;
  protected:
    void zero_data();
    int resolve_gfEnableTracing(PBYTE);
    // output data
    PBYTE m_gfEnableTracing;
    PBYTE tlg_PoFAggregate;
    std::list<PBYTE> tlg_CombaseTraceLoggingProviderProv;
};