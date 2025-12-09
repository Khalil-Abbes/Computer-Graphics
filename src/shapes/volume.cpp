#include <lightwave.hpp>

namespace lightwave {

class Volume : public Shape {
    float m_density;
    ref<Shape> m_boundary;

public:
    Volume(const Properties &properties) {
        m_density  = properties.get<float>("density");
        m_boundary = properties.getOptionalChild<Shape>();
    }

    bool intersect(const Ray &ray, Intersection &its,
                   Sampler &rng) const override {
        // Sample a distance in the volume using inverse transform sampling
        float u = rng.next();

        // Distance sampling: t = -ln(1 - u) / density
        u                     = std::min(u, 1 - Epsilon);
        float sampledDistance = -std::log(1 - u) / m_density;

        // Ensure numerical stability
        sampledDistance = std::max(sampledDistance, Epsilon);

        // Check if the sampled distance is closer than current intersection
        if (sampledDistance >= its.t) {
            return false;
        }

        // Populate the intersection
        its.t        = sampledDistance;
        its.position = ray(sampledDistance);

        // Create valid coordinate frame for volumes
        Vector normal      = -ray.direction;
        its.shadingNormal  = normal;
        its.geometryNormal = normal;
        its.tangent = Frame(normal).tangent; // Generate orthogonal tangent

        its.uv = Point2(0, 0);

        return true;
    }

    float transmittance(const Ray &ray, float tMax,
                        Sampler &rng) const override {
        return std::exp(-m_density * tMax);
    }

    Bounds getBoundingBox() const override {
        if (!m_boundary) {
            return Bounds::full();
        }
        return m_boundary->getBoundingBox();
    }

    Point getCentroid() const override {
        if (!m_boundary) {
            return Point(0, 0, 0);
        }
        return m_boundary->getCentroid();
    }

    std::string toString() const override {
        return tfm::format(
            "Volume[\n"
            "  density = %f\n"
            "]",
            m_density);
    }
};

} // namespace lightwave

REGISTER_SHAPE(Volume, "volume")
