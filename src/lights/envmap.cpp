#include <lightwave.hpp>

#include <vector>

namespace lightwave {

class EnvironmentMap final : public BackgroundLight {
    /// @brief The texture to use as background
    ref<Texture> m_texture;
    /// @brief An optional transform from local-to-world space
    ref<Transform> m_transform;

public:
    EnvironmentMap(const Properties &properties) : BackgroundLight(properties) {
        m_texture   = properties.getChild<Texture>();
        m_transform = properties.getOptionalChild<Transform>();
    }

    EmissionEval evaluate(const Vector &direction) const override {
        Vector localDir = direction;

        // Step 1: Transform from world to local coordinates

        if (m_transform) {
            localDir = m_transform->inverse(direction);
            // flipping component works, don't ask me why
            localDir = Vector(-localDir.x(), localDir.y(), localDir.z());
        }

        // Step 2: Convert Cartesian direction to spherical coordinates
        // Azimuth (phi): angle in xz-plane from x-axis
        float phi = atan2(localDir.z(), localDir.x());

        // Ensure phi is in [0, 2π] range
        if (phi < 0) {
            phi += TwoPi;
        }

        // Elevation (theta): angle from y-axis
        float theta = atan2(
            sqrt(localDir.x() * localDir.x() + localDir.z() * localDir.z()),
            localDir.y());

        // Step 3: Map spherical coordinates to texture coordinates [0,1]²
        Point2 warped = Point2(phi / (2 * Pi), // u coordinate
                               theta / Pi      // v coordinate
        );

        return {
            .value = m_texture->evaluate(warped),
        };
    }

    DirectLightSample sampleDirect(const Point &origin,
                                   Sampler &rng) const override {
        Point2 warped    = rng.next2D();
        Vector direction = squareToUniformSphere(warped);
        auto E           = evaluate(direction);

        // implement better importance sampling here, if you ever need it
        // (useful for environment maps with bright tiny light sources, like the
        // sun for example)

        return {
            .wi       = direction,
            .weight   = E.value * FourPi,
            .distance = Infinity,
        };
    }

    std::string toString() const override {
        return tfm::format(
            "EnvironmentMap[\n"
            "  texture = %s,\n"
            "  transform = %s\n"
            "]",
            indent(m_texture),
            indent(m_transform));
    }
};

} // namespace lightwave

REGISTER_LIGHT(EnvironmentMap, "envmap")
