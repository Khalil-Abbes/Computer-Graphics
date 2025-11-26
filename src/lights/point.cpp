#include <lightwave.hpp>

namespace lightwave {

class PointLight final : public Light {
    // member variables to store position and power
    Point m_position;
    Color m_power;
    Color m_powerOverFourPi; // Precomputed power/(4π)

public:
    PointLight(const Properties &properties) : Light(properties) {
        // Extract position and power from properties
        m_position = properties.get<Point>("position");
        m_power    = properties.get<Color>("power");

        // Precompute power/(4π) to avoid redundant computation in sampleDirect
        m_powerOverFourPi = m_power / FourPi;
    }

    DirectLightSample sampleDirect(const Point &origin,
                                   Sampler &rng) const override {
        // 1. Compute vector from surface point to light
        Vector toLight = m_position - origin;

        // 2. Compute distance to light
        float distance = toLight.length();

        // 3. Normalize to get direction toward light
        Vector wi = toLight / distance;

        // 4. Convert power to intensity with inverse square fallout
        // Using precomputed power/(4π), only need to divide by distance²
        Color weight = m_powerOverFourPi / (distance * distance);

        // 5. Return the light sample
        return { .wi = wi, .weight = weight, .distance = distance };
    }

    bool canBeIntersected() const override { return false; }

    std::string toString() const override {
        return tfm::format(
            "PointLight[\n"
            "  position = %s,\n"
            "  power = %s\n"
            "]",
            m_position,
            m_power);
    }
};

} // namespace lightwave

REGISTER_LIGHT(PointLight, "point")
