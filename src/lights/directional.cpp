#include <lightwave.hpp>

namespace lightwave {

class DirectionalLight final : public Light {
    // Member variables
    Vector m_direction; // Direction the light comes FROM
    Color m_intensity;  // Constant intensity (no falloff)

public:
    DirectionalLight(const Properties &properties) : Light(properties) {
        // Extract direction and intensity from properties
        m_direction = properties.get<Vector>("direction");
        m_intensity = properties.get<Color>("intensity");
    }

    DirectLightSample sampleDirect(const Point &origin,
                                   Sampler &rng) const override {
        // Direction toward the light (negate since m_direction points
        // where light comes FROM)
        Vector wi = m_direction.normalized();

        // Return sample with constant intensity and infinite distance
        return {
            .wi       = wi.normalized(),
            .weight   = m_intensity,
            .distance = Infinity // Light is at infinity
        };
    }

    bool canBeIntersected() const override { return false; }

    std::string toString() const override {
        return tfm::format(
            "DirectionalLight[\n"
            "  direction = %s,\n"
            "  intensity = %s\n"
            "]",
            m_direction,
            m_intensity);
    }
};

} // namespace lightwave

REGISTER_LIGHT(DirectionalLight, "directional")
