#include <lightwave.hpp>

#include "fresnel.hpp"
#include "microfacet.hpp"

namespace lightwave {

struct DiffuseLobe {
    Color color;

    BsdfEval evaluate(const Vector &wo, const Vector &wi) const {
        // Check if wi and wo are in the same hemisphere
        if (!Frame::sameHemisphere(wo, wi)) {
            return BsdfEval::invalid();
        }

        // Diffuse BRDF: (albedo / π) * cos(θ_i)
        Color value = Frame::absCosTheta(wi) * color * InvPi;
        return BsdfEval{ .value = value };
    }

    BsdfSample sample(const Vector &wo, Sampler &rng) const {
        // Sample a cosine-weighted direction in local coordinates
        Vector wi = squareToCosineHemisphere(rng.next2D());

        // Flip wi if wo is below the surface to ensure they're in the same
        // hemisphere
        if (Frame::cosTheta(wo) < 0) {
            wi.z() = -wi.z();
        }

        // Always normalize to ensure perfect normalization
        wi = wi.normalized();

        // With cosine-weighted sampling, the weight simplifies to just the
        // color
        return BsdfSample{ .wi = wi, .weight = color };
    }
};

struct MetallicLobe {
    float alpha;
    Color color;

    BsdfEval evaluate(const Vector &wo, const Vector &wi) const {
        using namespace microfacet;

        float cosThetaI = Frame::cosTheta(wi);
        float cosThetaO = Frame::cosTheta(wo);

        if (cosThetaI <= 1e-6f || cosThetaO <= 1e-6f) {
            return BsdfEval::invalid();
        }

        Vector h = (wi + wo).normalized();

        if (Frame::cosTheta(h) <= 0) {
            return BsdfEval::invalid();
        }

        float D = evaluateGGX(alpha, h);

        float G1_wi = smithG1(alpha, h, wi);
        float G1_wo = smithG1(alpha, h, wo);
        float G2    = G1_wi * G1_wo;

        float denominator = 4.0f * cosThetaI * cosThetaO;
        Color brdf        = color * (D * (G2 / denominator));

        return BsdfEval{ .value = brdf * cosThetaI };
    }

    BsdfSample sample(const Vector &wo, Sampler &rng) const {
        using namespace microfacet;

        if (Frame::cosTheta(wo) <= 1e-6f) {
            return BsdfSample::invalid();
        }

        Vector h  = sampleGGXVNDF(alpha, wo, rng.next2D());
        Vector wi = reflect(wo, h);

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
        const auto baseColor = m_baseColor->evaluate(uv);
        const auto alpha     = max(float(1e-3), sqr(m_roughness->scalar(uv)));
        const auto specular  = m_specular->scalar(uv);
        const auto metallic  = m_metallic->scalar(uv);
        const auto F =
            specular * schlick((1 - metallic) * 0.08f, Frame::cosTheta(wo));

        const DiffuseLobe diffuseLobe = {
            .color = (1 - F) * (1 - metallic) * baseColor,
        };
        const MetallicLobe metallicLobe = {
            .alpha = alpha,
            .color = F * Color(1) + (1 - F) * metallic * baseColor,
        };

        const auto diffuseAlbedo = diffuseLobe.color.mean();
        const auto totalAlbedo =
            diffuseLobe.color.mean() + metallicLobe.color.mean();
        return {
            .diffuseSelectionProb =
                totalAlbedo > 0 ? diffuseAlbedo / totalAlbedo : 1.0f,
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

        // Evaluate both lobes and sum their contributions
        BsdfEval diffuseEval  = combination.diffuse.evaluate(wo, wi);
        BsdfEval metallicEval = combination.metallic.evaluate(wo, wi);

        // Return the sum of both lobes
        return BsdfEval{ .value = diffuseEval.value + metallicEval.value };
    }

    BsdfSample sample(const Point2 &uv, const Vector &wo,
                      Sampler &rng) const override {
        PROFILE("Principled")

        const auto combination = combine(uv, wo);

        // Sample either diffuse or metallic lobe based on selection probability
        if (rng.next() < combination.diffuseSelectionProb) {
            // Sample diffuse lobe
            BsdfSample sample = combination.diffuse.sample(wo, rng);
            // Divide weight by selection probability for correct importance
            // sampling
            sample.weight = sample.weight / combination.diffuseSelectionProb;
            return sample;
        } else {
            // Sample metallic lobe
            BsdfSample sample = combination.metallic.sample(wo, rng);
            // Divide weight by selection probability (1 - diffuseSelectionProb)
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
