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

        const float alpha       = max(1e-3f, sqr(m_roughness->scalar(uv)));
        const Color reflectance = m_reflectance->evaluate(uv);

        const float cosThetaI = Frame::cosTheta(wi);
        const float cosThetaO = Frame::cosTheta(wo);

        // Only require that wi and wo lie in the same hemisphere
        if (!Frame::sameHemisphere(wi, wo)) {
            return BsdfEval::invalid();
        }

        const float absCosThetaI = std::abs(cosThetaI);
        const float absCosThetaO = std::abs(cosThetaO);

        // Avoid division by very small values (grazing)
        if (absCosThetaI <= 1e-4f || absCosThetaO <= 1e-4f) {
            return BsdfEval::invalid();
        }

        // Half-vector
        Vector h = (wi + wo).normalized();

        // Microfacet normal must be in the visible hemisphere
        if (Frame::cosTheta(h) <= 0.0f) {
            return BsdfEval::invalid();
        }

        const float D     = evaluateGGX(alpha, h);
        const float G1_wi = smithG1(alpha, h, wi);
        const float G1_wo = smithG1(alpha, h, wo);
        const float G2    = G1_wi * G1_wo;

        // f * |cosThetaI| = ρ D G / (4 |cosThetaO|)
        const float denominator = 4.0f * absCosThetaO;
        const Color value       = reflectance * (D * (G2 / denominator));

        return BsdfEval{ .value = value };
    }

    BsdfSample sample(const Point2 &uv, const Vector &wo,
                      Sampler &rng) const override {
        using namespace microfacet;

        const float alpha       = max(1e-3f, sqr(m_roughness->scalar(uv)));
        const Color reflectance = m_reflectance->evaluate(uv);

        // Sample visible microfacet normal
        Vector h  = sampleGGXVNDF(alpha, wo, rng.next2D());
        Vector wi = reflect(wo, h);

        // Ensure wo and wi are in the same hemisphere
        if (!Frame::sameHemisphere(wi, wo)) {
            return BsdfSample::invalid();
        }

        const float G1_wi = smithG1(alpha, h, wi);

        return BsdfSample{ .wi = wi, .weight = reflectance * G1_wi };
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
