#pragma once
#include <cstdint>

namespace jd
{
class IGameEventGenerator
{
public:
    virtual ~IGameEventGenerator() = default;

    virtual int generateLandPrice() const                                                                = 0;
    virtual int generateWheatYield() const                                                               = 0;
    virtual int generateRatsActivity(int32_t total_wheat) const                                          = 0;
    virtual int generateMigration(int32_t current_death, int32_t total_wheat, int32_t wheat_yield) const = 0;
    virtual bool checkPlague() const                                                                     = 0;
};
} // namespace jd
