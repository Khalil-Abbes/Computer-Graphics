#include <lightwave.hpp>

#include "fresnel.hpp"
#include "microfacet.hpp"

namespace lightwave {

struct DiffuseLobe {
    Color color;

    BsdfEval evaluate(const Vector &wo, const Vector &wi) const {
        // Ensure wi and wo are in the same hemisphere
        if (!Frame::sameHemisphere(wo, wi)) {
            return BsdfEval::invalid();
        }

        // Diffuse BRDF: f = color / π, BsdfEval.value = f * |cosθ_i|
        Color value = Frame::absCosTheta(wi) * color * InvPi;
        return BsdfEval{ .value = value };
    }

    BsdfSample sample(const Vector &wo, Sampler &rng) const {
        // Sample a cosine-weighted direction in local coordinates
        Vector wi = squareToCosineHemisphere(rng.next2D());

        // Flip wi if wo is below the surface to ensure they're in the same
        // hemisphere
        if (Frame::cosTheta(wo) < 0.0f) {
            wi.z() = -wi.z();
        }

        wi = wi.normalized();

        // With cosine-weighted sampling for Lambertian, the weight is just the
        // albedo
        return BsdfSample{ .wi = wi, .weight = color };
    }
};

struct MetallicLobe {
    float alpha;
    Color color;

    BsdfEval evaluate(const Vector &wo, const Vector &wi) const {
        using namespace microfacet;

        // Require same hemisphere, but allow above or below
        if (!Frame::sameHemisphere(wo, wi)) {
            return BsdfEval::invalid();
        }

        float absCosThetaI = Frame::absCosTheta(wi);
        float absCosThetaO = Frame::absCosTheta(wo);

        // Avoid division by values close to zero (grazing)
        if (absCosThetaI <= 1e-4f || absCosThetaO <= 1e-4f) {
            return BsdfEval::invalid();
        }

        Vector h = (wi + wo).normalized();

        // Microfacet normal must be in the visible hemisphere
        if (Frame::cosTheta(h) <= 0.0f) {
            return BsdfEval::invalid();
        }

        float D     = evaluateGGX(alpha, h);
        float G1_wi = smithG1(alpha, h, wi);
        float G1_wo = smithG1(alpha, h, wo);
        float G2    = G1_wi * G1_wo;

        // Microfacet BRDF: f = ρ D G / (4 |cosθ_i| |cosθ_o|)
        // BsdfEval.value stores f * |cosθ_i| = ρ D G / (4 |cosθ_o|)
        float denominator = 4.0f * absCosThetaO;
        Color value       = color * (D * (G2 / denominator));

        return BsdfEval{ .value = value };
    }

    BsdfSample sample(const Vector &wo, Sampler &rng) const {
        using namespace microfacet;

        // Sample visible microfacet normal
        Vector h  = sampleGGXVNDF(alpha, wo, rng.next2D());
        Vector wi = reflect(wo, h);

        // Ensure wo and wi end up in the same hemisphere
        if (!Frame::sameHemisphere(wo, wi)) {
            return BsdfSample::invalid();
        }

        float G1_wi = smithG1(alpha, h, wi);

        return BsdfSample{ .wi = wi, .weight = color * G1_wi };
    }
};

class Principled : public Bsdf {
    ref<Texture> m_baseColor;
    ref<Texture> m_roughness;
    ref<Texture> m_metallic;
    ref<Texture> m_specular;

    struct Combination {
        float diffuseSelectionProb;
        DiffuseLobe diffuse;
        MetallicLobe metallic;
    };

    Combination combine(const Point2 &uv, const Vector &wo) const {
        const Color baseColor = m_baseColor->evaluate(uv);
        const float alpha     = max(1e-3f, sqr(m_roughness->scalar(uv)));
        const float specular  = m_specular->scalar(uv);
        const float metallic  = m_metallic->scalar(uv);

        const float F =
            specular * schlick((1.0f - metallic) * 0.08f, Frame::cosTheta(wo));

        const DiffuseLobe diffuseLobe = {
            .color = (1.0f - F) * (1.0f - metallic) * baseColor,
        };

        const MetallicLobe metallicLobe = {
            .alpha = alpha,
            .color = F * Color(1.0f) + (1.0f - F) * metallic * baseColor,
        };

        const float diffuseAlbedo = diffuseLobe.color.mean();
        const float totalAlbedo =
            diffuseLobe.color.mean() + metallicLobe.color.mean();

        return {
            .diffuseSelectionProb =
                totalAlbedo > 0.0f ? diffuseAlbedo / totalAlbedo : 1.0f,
            .diffuse  = diffuseLobe,
            .metallic = metallicLobe,
        };
    }

public:
    Principled(const Properties &properties) {
        m_baseColor = properties.get<Texture>("baseColor");
        m_roughness = properties.get<Texture>("roughness");
        m_metallic  = properties.get<Texture>("metallic");
        m_specular  = properties.get<Texture>("specular");
    }

    BsdfEval evaluate(const Point2 &uv, const Vector &wo,
                      const Vector &wi) const override {
        PROFILE("Principled")

        const auto combination = combine(uv, wo);

        // Evaluate both lobes and add their contributions
        BsdfEval diffuseEval  = combination.diffuse.evaluate(wo, wi);
        BsdfEval metallicEval = combination.metallic.evaluate(wo, wi);

        return BsdfEval{ .value = diffuseEval.value + metallicEval.value };
    }

    BsdfSample sample(const Point2 &uv, const Vector &wo,
                      Sampler &rng) const override {
        PROFILE("Principled")

        const auto combination = combine(uv, wo);

        // Sample either diffuse or metallic lobe based on selection probability
        if (rng.next() < combination.diffuseSelectionProb) {
            // Diffuse lobe
            BsdfSample sample = combination.diffuse.sample(wo, rng);
            sample.weight = sample.weight / combination.diffuseSelectionProb;
            return sample;
        } else {
            // Metallic lobe
            BsdfSample sample = combination.metallic.sample(wo, rng);
            sample.weight =
                sample.weight / (1.0f - combination.diffuseSelectionProb);
            return sample;
        }
    }

    std::string toString() const override {
        return tfm::format(
            "Principled[\n"
            "  baseColor = %s,\n"
            "  roughness = %s,\n"
            "  metallic  = %s,\n"
            "  specular  = %s,\n"
            "]",
            indent(m_baseColor),
            indent(m_roughness),
            indent(m_metallic),
            indent(m_specular));
    }
};

} // namespace lightwave

REGISTER_BSDF(Principled, "principled")
