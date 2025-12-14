#include <lightwave.hpp>

namespace lightwave {

class Volume : public Shape {
    float m_density;
    ref<Shape> m_boundary;

    /**
     * @brief Determines the entry and exit points of the ray with the volume's
     * bounding shape.
     * @param ray The ray to trace.
     * @param tEntry Output for the entry distance along the ray.
     * @param tExit Output for the exit distance along the ray.
     * @param rng The sampler to use for intersection tests.
     * @return true if the ray hits the boundary (or is inside an unbounded
     * volume), false otherwise.
     */
    bool getInterval(const Ray &ray, float &tEntry, float &tExit,
                     Sampler &rng) const {
        if (!m_boundary) {
            // Infinite volume case
            tEntry = 0;
            tExit  = Infinity;
            return true;
        }

        Intersection boundaryIts;
        boundaryIts.t = Infinity;

        // Check if the ray intersects the boundary shape
        if (!m_boundary->intersect(ray, boundaryIts, rng)) {
            return false;
        }

        // Check if we are entering or exiting based on the geometry normal.
        // If the normal points against the ray, we are entering.
        if (boundaryIts.geometryNormal.dot(ray.direction) < 0) {
            // We are outside, entering the volume
            tEntry = boundaryIts.t;

            // To find the exit point, trace a new ray starting slightly inside
            // the volume
            Ray insideRay    = ray;
            insideRay.origin = ray(tEntry + Epsilon);

            Intersection exitIts;
            exitIts.t = Infinity;

            if (m_boundary->intersect(insideRay, exitIts, rng)) {
                tExit = tEntry + exitIts.t;
            } else {
                // If we entered a closed shape but didn't find an exit, assume
                // it extends to infinity
                tExit = Infinity;
            }
        } else {
            // We are inside the volume, so the hit is the exit point
            tEntry = 0;
            tExit  = boundaryIts.t;
        }

        return true;
    }

public:
    Volume(const Properties &properties) {
        m_density  = properties.get<float>("density");
        m_boundary = properties.getOptionalChild<Shape>();
    }

    bool intersect(const Ray &ray, Intersection &its,
                   Sampler &rng) const override {
        // 1. Determine the interval [tEntry, tExit] where the ray is inside the
        // volume
        float tEntry, tExit;
        if (!getInterval(ray, tEntry, tExit, rng)) {
            return false;
        }

        // 2. Sample a scattering distance
        float u = rng.next();
        // Clamp u to avoid log(0)
        u = std::min(u, 1.0f - Epsilon);

        // Free-flight distance sampling: t = -ln(1 - u) / density
        float sampledDistance = -std::log(1.0f - u) / m_density;

        // Ensure numerical stability
        sampledDistance = std::max(sampledDistance, Epsilon);

        // Calculate actual distance along the ray from the origin
        float tHit = tEntry + sampledDistance;

        // 3. Rejection sampling
        // If the scattering event happens beyond the volume boundary (tExit),
        // it is invalid.
        if (tHit >= tExit) {
            return false;
        }

        // If the scattering event is behind a closer surface intersection we
        // already found, ignore it.
        if (tHit >= its.t) {
            return false;
        }

        // 4. Populate the intersection
        its.t        = tHit;
        its.position = ray(tHit);

        // For volumetric interactions, the normal is the opposite of
        // the ray direction
        Vector normal      = -ray.direction;
        its.shadingNormal  = normal;
        its.geometryNormal = normal;

        // Construct a coordinate frame (tangent/bitangent) from the normal
        its.tangent = Frame(normal).tangent;

        its.uv = Point2(0, 0);

        return true;
    }

    float transmittance(const Ray &ray, float tMax,
                        Sampler &rng) const override {
        float tEntry, tExit;

        // If we don't hit the boundary volume, transmittance is 1 (fully
        // transparent)
        if (!getInterval(ray, tEntry, tExit, rng)) {
            return 1.0f;
        }

        // Calculate the overlap between the ray segment [0, tMax] and the
        // volume interval [tEntry, tExit]
        float t0 = std::max(tEntry, 0.0f);
        float t1 = std::min(tExit, tMax);

        // If there is no overlap (distance <= 0), transmittance is 1
        if (t0 >= t1) {
            return 1.0f;
        }

        // Beer-Lambert law: T = exp(-sigma_t * distance)
        // Here sigma_t = sigma_s (since sigma_a = 0) = m_density
        return std::exp(-m_density * (t1 - t0));
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