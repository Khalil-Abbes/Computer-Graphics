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
        // Sample the albedo color from the texture
        Color albedo = m_albedo->evaluate(uv);

        // The cosine term naturally filters the hemisphere:
        // - Positive for wi above surface (correct)
        // - Negative for wi below surface (gets clamped to zero)
        Color value = Frame::cosTheta(wi) * albedo * InvPi;
        return { .value = value };
    }

    BsdfSample sample(const Point2 &uv, const Vector &wo,
                      Sampler &rng) const override {
        // Check if wo is in upper hemisphere
        // if (Frame::cosTheta(wo) < 0) {
        //      return BsdfSample::invalid();
        //  }

        // Sample a cosine-weighted direction in local coordinates
        Vector wi = squareToCosineHemisphere(rng.next2D());

        // Get the albedo color
        Color albedo = m_albedo->evaluate(uv);

        // With cosine-weighted sampling, the weight simplifies to just albedo
        // Because: weight = (albedo/π * cos(θ)) / (cos(θ)/π) = albedo
        Color weight = albedo;

        return { .wi = wi, .weight = weight };
    }

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
