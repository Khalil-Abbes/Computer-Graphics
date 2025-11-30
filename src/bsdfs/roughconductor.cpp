#include "fresnel.hpp"
#include "microfacet.hpp"
#include <lightwave.hpp>

namespace lightwave {

class RoughConductor : public Bsdf {
    ref<Texture> m_reflectance;
    ref<Texture> m_roughness;

public:
    RoughConductor(const Properties &properties) {
        m_reflectance = properties.get<Texture>("reflectance");
        m_roughness   = properties.get<Texture>("roughness");
    }

    BsdfEval evaluate(const Point2 &uv, const Vector &wo,
                      const Vector &wi) const override {
        using namespace microfacet;

        const auto alpha  = max(float(1e-3), sqr(m_roughness->scalar(uv)));
        Color reflectance = m_reflectance->evaluate(uv);

        float cosThetaI = Frame::cosTheta(wi);
        float cosThetaO = Frame::cosTheta(wo);

        if (cosThetaI <= 1e-2f || cosThetaO <= 1e-2f) {
            return BsdfEval::invalid();
        }

        Vector h = (wi + wo).normalized();

        if (Frame::cosTheta(h) <= 0) {
            return BsdfEval::invalid();
        }

        float D = evaluateGGX(alpha, h);

        float G1_wi = smithG1(alpha, h, wi); // h first, then wi
        float G1_wo = smithG1(alpha, h, wo); // h first, then wo
        float G2    = G1_wi * G1_wo;

        float denominator = 4.0f * cosThetaI * cosThetaO;
        Color brdf        = reflectance * (D * (G2 / denominator));

        return BsdfEval{ .value = brdf * cosThetaI };
    }

    BsdfSample sample(const Point2 &uv, const Vector &wo,
                      Sampler &rng) const override {
        using namespace microfacet;

        const auto alpha  = max(float(1e-3), sqr(m_roughness->scalar(uv)));
        Color reflectance = m_reflectance->evaluate(uv);

        if (Frame::cosTheta(wo) <= 1e-6f) {
            return BsdfSample::invalid();
        }

        Vector h  = sampleGGXVNDF(alpha, wo, rng.next2D());
        Vector wi = reflect(wo, h);

        float G1_wi = smithG1(alpha, h, wi); // h first, then wi

        return BsdfSample{
            .wi     = wi,
            .weight = reflectance * G1_wi // NO extra factors here!
        };
    }

    std::string toString() const override {
        return tfm::format(
            "RoughConductor[\n"
            "  reflectance = %s,\n"
            "  roughness = %s\n"
            "]",
            indent(m_reflectance),
            indent(m_roughness));
    }
};

} // namespace lightwave

REGISTER_BSDF(RoughConductor, "roughconductor")
