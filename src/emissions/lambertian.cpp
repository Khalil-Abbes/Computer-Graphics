#include <lightwave.hpp>

namespace lightwave {

class Lambertian : public Emission {
    ref<Texture> m_emission;

public:
    Lambertian(const Properties &properties) {
        m_emission = properties.get<Texture>("emission");
    }

    EmissionEval evaluate(const Point2 &uv, const Vector &wo) const override {
        // Only emit light on the front side (where normal points)
        // wo points toward the viewer/camera
        // Front side: wo is in upper hemisphere (cosTheta > 0)
        if (Frame::cosTheta(wo) <= 0) {
            return EmissionEval{ .value = Color(0) };
        }

        Color value = m_emission->evaluate(uv); // texture sampling
        return EmissionEval{ .value = value };
    }

    std::string toString() const override {
        return tfm::format(
            "Lambertian[\n"
            "  emission = %s\n"
            "]",
            indent(m_emission));
    }
};

} // namespace lightwave

REGISTER_EMISSION(Lambertian, "lambertian")
