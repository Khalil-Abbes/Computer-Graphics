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
        // Check if wi and wo are in the same hemisphere
        // For diffuse materials, both should be on the same side of the surface
        if (!Frame::sameHemisphere(wo, wi)) {
            return { .value = Color(0) };
        }

        // Sample the albedo color from the texture
        Color albedo = m_albedo->evaluate(uv);

        // Use absCosTheta to handle both sides of the surface consistently
        // The cosine term naturally filters the hemisphere:
        // - absCosTheta ensures we get the magnitude regardless of orientation
        Color value = Frame::absCosTheta(wi) * albedo * InvPi;
        return { .value = value };
    }

    BsdfSample sample(const Point2 &uv, const Vector &wo,
                      Sampler &rng) const override {

        // Sample a cosine-weighted direction in local coordinates
        Vector wi = squareToCosineHemisphere(rng.next2D());

        // Flip wi if wo is below the surface to ensure they're in the same
        // hemisphere
        if (Frame::cosTheta(wo) < 0) {
            wi.z() = -wi.z();
        }

        // Always normalize to ensure perfect normalization
        wi = wi.normalized();

        // Get the albedo color
        Color albedo = m_albedo->evaluate(uv);

        // With cosine-weighted sampling, the weight simplifies to just albedo
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
