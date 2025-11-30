#include "fresnel.hpp"
#include <lightwave.hpp>

namespace lightwave {

class Dielectric : public Bsdf {
    ref<Texture> m_ior;
    ref<Texture> m_reflectance;
    ref<Texture> m_transmittance;

public:
    Dielectric(const Properties &properties) {
        m_ior           = properties.get<Texture>("ior");
        m_reflectance   = properties.get<Texture>("reflectance");
        m_transmittance = properties.get<Texture>("transmittance");
    }

    BsdfEval evaluate(const Point2 &uv, const Vector &wo,
                      const Vector &wi) const override {
        // the probability of a light sample picking exactly the direction `wi'
        // that results from reflecting or refracting `wo' is zero, hence we can
        // just ignore that case and always return black
        return BsdfEval::invalid();
    }

    BsdfSample sample(const Point2 &uv, const Vector &wo,
                      Sampler &rng) const override {
        // Get material properties
        float eta           = m_ior->scalar(uv);
        Color reflectance   = m_reflectance->evaluate(uv);
        Color transmittance = m_transmittance->evaluate(uv);

        // Determine if we're entering or exiting the medium
        float cosTheta_o = Frame::cosTheta(wo);
        bool entering    = cosTheta_o > 0;

        // Set up the relative IOR for Snell's law
        // entering: eta_o/eta_i (air to glass: 1/eta)
        // exiting: eta_i/eta_o (glass to air: eta/1)
        float eta_ratio = entering ? (1.0f / eta) : eta;

        // Compute sin²(theta_i) using Snell's law
        float sin2Theta_i =
            eta_ratio * eta_ratio * (1.0f - cosTheta_o * cosTheta_o);

        // Check for total internal reflection
        if (sin2Theta_i >= 1.0f) {
            // Total internal reflection - all light reflects
            Vector wi = reflect(wo, Vector(0, 0, 1));

            return BsdfSample{ .wi = wi, .weight = reflectance };
        }

        // Compute cos(theta_i) for the transmitted ray
        float cosTheta_i  = std::sqrt(1.0f - sin2Theta_i);
        float absCosTheta = std::abs(cosTheta_o);

        // Compute Fresnel reflectance using the Fresnel equations for
        // dielectrics Perpendicular (S) and parallel (P) polarized components
        float Rs = (eta_ratio * absCosTheta - cosTheta_i) /
                   (eta_ratio * absCosTheta + cosTheta_i);
        float Rp = (absCosTheta - eta_ratio * cosTheta_i) /
                   (absCosTheta + eta_ratio * cosTheta_i);

        // Average for unpolarized light
        float Fr = 0.5f * (Rs * Rs + Rp * Rp);

        // Importance sampling: weight by both Fresnel term and color brightness
        // This is the KEY to reducing variance!
        // The "contribution" from each lobe is Fresnel * color_brightness
        float reflectContrib = Fr * reflectance.mean();
        float refractContrib = (1.0f - Fr) * transmittance.mean();
        float totalContrib   = reflectContrib + refractContrib;

        // Calculate sampling probability proportional to contribution
        // If totalContrib is 0 (both black), default to reflection
        float reflectProb =
            (totalContrib > 0) ? (reflectContrib / totalContrib) : 1.0f;

        if (rng.next() < reflectProb) {
            // REFLECTION: Perfect mirror reflection about the normal
            Vector wi = reflect(wo, Vector(0, 0, 1));

            // Monte Carlo estimator: E = f(x) / p(x)
            // f(x) = reflectance * Fr (the actual contribution)
            // p(x) = reflectProb (our sampling probability)
            // Weight = f(x) / p(x) = (reflectance * Fr) / reflectProb

            // The BSDF value is: reflectance * delta(wi - reflect(wo))
            // When we importance sample with probability reflectProb, the
            // weight becomes: weight = (reflectance * Fr) / reflectProb

            Color weight = reflectance;
            if (reflectProb > 0) {
                weight = weight * (Fr / reflectProb);
            }

            return BsdfSample{ .wi = wi, .weight = weight };
        } else {
            // REFRACTION: Compute refracted direction using Snell's law

            // Determine sign for entering vs exiting
            float sign = entering ? 1.0f : -1.0f;

            // Compute refracted direction
            // wi = eta_ratio * (-wo) + (eta_ratio * cos_o - cos_i) * n
            Vector wi = Vector(
                -eta_ratio * wo.x(), -eta_ratio * wo.y(), -sign * cosTheta_i);

            // Solid angle compression factor: eta_ratio²
            float eta2 = eta_ratio * eta_ratio;

            // Monte Carlo estimator: E = f(x) / p(x)
            // f(x) = transmittance * (1 - Fr) * eta2
            // p(x) = 1 - reflectProb (our sampling probability for refraction)
            float refractProb = 1.0f - reflectProb;

            Color weight = transmittance * eta2;
            if (refractProb > 0) {
                weight = weight * ((1.0f - Fr) / refractProb);
            }

            return BsdfSample{ .wi = wi, .weight = weight };
        }
    }

    std::string toString() const override {
        return tfm::format(
            "Dielectric[\n"
            "  ior           = %s,\n"
            "  reflectance   = %s,\n"
            "  transmittance = %s\n"
            "]",
            indent(m_ior),
            indent(m_reflectance),
            indent(m_transmittance));
    }
};

} // namespace lightwave

REGISTER_BSDF(Dielectric, "dielectric")
