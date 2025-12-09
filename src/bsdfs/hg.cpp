#include <lightwave.hpp>

namespace lightwave {

class HenyeyGreenstein : public Bsdf {
    float m_g;
    Color m_albedo;

public:
    HenyeyGreenstein(const Properties &properties) {
        m_g      = properties.get<float>("g");
        m_albedo = properties.get<Color>("albedo");
    }

    BsdfEval evaluate(const Point2 &uv, const Vector &wo,
                      const Vector &wi) const override {
        // Cosine of the angle between camera ray and light ray
        float cosTheta = wo.dot(wi);

        // Calculate the denominator: (1 + g^2 + 2g * cosTheta)
        float denom = 1 + sqr(m_g) + 2 * m_g * cosTheta;

        // Avoid division by zero or negative roots (though mathematically safe
        // here)
        denom = std::max(1e-5f, denom);

        // Phase function value p(theta)
        float phase = (1 - sqr(m_g)) * (InvPi * 0.25f) / std::pow(denom, 1.5f);

        // Return albedo * phase (The BsdfEval struct typically just takes the
        // value)
        return { m_albedo * phase };
    }

    BsdfSample sample(const Point2 &uv, const Vector &wo,
                      Sampler &rng) const override {
        Point2 u = rng.next2D();
        float cosTheta;

        // 1. Sample cosTheta using Inverse Transform Sampling
        if (std::abs(m_g) < 1e-3f) {
            cosTheta = 1 - 2 * u.x();
        } else {
            float sqrTerm = (1 - sqr(m_g)) / (1 + m_g - 2 * m_g * u.x());
            cosTheta      = (1 + sqr(m_g) - sqr(sqrTerm)) / (2 * m_g);
        }

        // 2. Compute sinTheta and Phi
        float sinTheta = std::sqrt(std::max(0.0f, 1 - sqr(cosTheta)));
        float phi      = 2 * Pi * u.y();

        // 3. Construct local direction
        Vector localDir(
            sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta);

        // 4. Transform to world space
        // Aligning to -wo represents forward scattering relative to the
        // incoming ray
        Vector wi = Frame(-wo).toWorld(localDir);

        // 5. Return the sample
        // For perfect importance sampling: Weight = Albedo * Phase / PDF
        // Since we sampled exactly according to the Phase function, Phase ==
        // PDF. Therefore, Weight = Albedo.
        return { .wi = wi, .weight = m_albedo };
    }

    std::string toString() const override {
        return tfm::format(
            "HenyeyGreenstein[\n"
            "  albedo = %s\n"
            "]",
            indent(m_albedo));
    }
};

} // namespace lightwave

REGISTER_BSDF(HenyeyGreenstein, "hg")
