#include <lightwave.hpp>

namespace lightwave {

class Diffuse : public Bsdf {
    ref<Texture> m_albedo;

public:
    Diffuse(const Properties &properties) {
        m_albedo = properties.get<Texture>("albedo");
    }

    BsdfEval evaluate(const Point2 &uv, const Vector &wo,
                      const Vector &wi) const override {
        // Check if wi is below the surface
        if (Frame::cosTheta(wi) <= 0) {
            return BsdfEval::invalid();
        }

        // Sample the albedo color from the texture
        Color albedo = m_albedo->evaluate(uv);

        // Return: cos(theta) * BRDF = cos(theta) * (albedo / pi)
        Color value = Frame::absCosTheta(wi) * albedo * InvPi;
        return { .value = value };
    }

    BsdfSample sample(const Point2 &uv, const Vector &wo,
                      Sampler &rng) const override{ NOT_IMPLEMENTED }

    std::string toString() const override {
        return tfm::format(
            "Diffuse[\n"
            "  albedo = %s\n"
            "]",
            indent(m_albedo));
    }
};

} // namespace lightwave

REGISTER_BSDF(Diffuse, "diffuse")
