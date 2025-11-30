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

        // Compute Fresnel reflectance using dielectric Fresnel equations
        // For unpolarized light at the interface
        float absCosTheta = std::abs(cosTheta_o);

        // Compute cos(theta_i) for the transmitted ray
        float sin2Theta_i =
            eta_ratio * eta_ratio * (1.0f - cosTheta_o * cosTheta_o);

        // Check for total internal reflection
        if (sin2Theta_i >= 1.0f) {
            // Total internal reflection - all light reflects
            Vector wi = reflect(wo, Vector(0, 0, 1));

            return BsdfSample{ .wi = wi, .weight = reflectance };
        }

        float cosTheta_i = std::sqrt(1.0f - sin2Theta_i);

        // Compute Fresnel reflectance using the Fresnel equations for
        // dielectrics Parallel and perpendicular polarized components
        float Rs = (eta_ratio * absCosTheta - cosTheta_i) /
                   (eta_ratio * absCosTheta + cosTheta_i);
        float Rp = (absCosTheta - eta_ratio * cosTheta_i) /
                   (absCosTheta + eta_ratio * cosTheta_i);

        // Average for unpolarized light
        float Fr = 0.5f * (Rs * Rs + Rp * Rp);

        // Importance sample: choose reflection or refraction based on Fresnel
        if (rng.next() < Fr) {
            // REFLECTION: Perfect mirror reflection about the normal
            Vector wi = reflect(wo, Vector(0, 0, 1));

            // Weight = reflectance / probability
            // The cosine term is included in weight, but cancels with pdf
            return BsdfSample{ .wi = wi, .weight = reflectance };
        } else {
            // REFRACTION: Compute refracted direction

            // Compute the refracted direction using Snell's law
            // wi = eta_ratio * (-wo) + (eta_ratio * cosTheta_o - cosTheta_i) *
            // n
            float sign = entering ? 1.0f : -1.0f;
            Vector wi  = Vector(
                -eta_ratio * wo.x(), -eta_ratio * wo.y(), -sign * cosTheta_i);

            // Solid angle compression factor: eta_ratio^2
            // This accounts for radiance scaling when crossing interfaces
            float eta2 = eta_ratio * eta_ratio;

            return BsdfSample{ .wi = wi, .weight = transmittance * eta2 };
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
