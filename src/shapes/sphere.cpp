#include <lightwave.hpp>

namespace lightwave {
class Sphere : public Shape {
public:
    Sphere(const Properties &properties) {}

    /**
     * @brief Constructs a surface event for a given position, used by @ref
     * intersect to populate the @ref Intersection and by @ref sampleArea to
     * populate the @ref AreaSample .
     * @param surf The surface event to populate with texture coordinates,
     * shading frame and area pdf?
     * @param position The hitpoint (i.e., point in [?,?,?] to [?,?,?]),
     * found via intersection or area sampling?
     */
    inline void populate(SurfaceEvent &surf, const Point &position) const {
        surf.position = position;

        // Calculate UV coordinates using spherical mapping
        Vector localPoint = Vector(position).normalized();

        // Spherical coordinates: theta (azimuthal), phi (polar)
        float theta = atan2(localPoint.z(), localPoint.x());
        float phi   = safe_acos(localPoint.y());

        // Map to [0,1] range
        surf.uv.x() = 1.0f - (theta + Pi) / (2 * Pi);
        surf.uv.y() = phi / Pi;

        // Normal for unit sphere centered at origin
        surf.geometryNormal = Vector(position).normalized();
        surf.shadingNormal  = surf.geometryNormal;

        // **FIXED TANGENT** - must be perpendicular to normal
        // Tangent follows the direction of increasing theta (dP/dtheta)
        Vector n     = surf.shadingNormal;
        surf.tangent = Vector(-n.z(), 0.0f, n.x());

        float tangentLen = surf.tangent.length();
        if (tangentLen > 1e-8f) {
            surf.tangent = surf.tangent / tangentLen;
        } else {
            // At poles, use arbitrary tangent
            surf.tangent = Vector(1, 0, 0);
        }

        surf.pdf = 1.0f;
    }

    bool intersect(const Ray &ray, Intersection &its,
                   Sampler &rng) const override {
        PROFILE("Sphere")

        // Simplified: quad_a is always 1 since ray.direction is normalized
        const float quad_b = 2 * Vector(ray.origin).dot(ray.direction);

        // Simplified: use lengthSquared() directly
        const float quad_c = Vector(ray.origin).lengthSquared() - 1.f;

        const float disc = quad_b * quad_b - 4 * quad_c;

        if (disc < 0) {
            return false;
        }

        float t1, t2;

        const float sqrt_disc = sqrt(disc);

        // Simplified: multiply by 0.5 instead of dividing by 2*quad_a (which
        // was 2)
        t1 = (-quad_b + sqrt_disc) * 0.5f;
        t2 = (-quad_b - sqrt_disc) * 0.5f;

        // TODO: This is a mess
        bool t2_valid = true;
        if (t2 < Epsilon || t2 > its.t) {
            t2_valid = false;
        }

        if (t1 < Epsilon || t1 > its.t) {
            if (t2_valid) {
                t1 = t2;
            } else {
                // Both invalid matches
                return false;
            }
        } else {
            if (t2_valid) {
                // Both valid matches
                if (t2 < t1) {
                    t1 = t2;
                }
            }
        }

        // compute the hitpoint
        const Point position = ray(t1);

        // we have determined there was an intersection! we are now free to
        // change the intersection object and return true.

        its.t = t1;

        // Line 97 removed: Don't overwrite its.wo

        populate(its,
                 position); // compute the shading frame, texture coordinates
                            // and area pdf? (same as sampleArea)

        return true;
    }

    Bounds getBoundingBox() const override {
        return Bounds(Point{ -1, -1, -1 }, Point{ +1, +1, +1 });
    }

    Point getCentroid() const override { return Point(0, 0, 0); }

    AreaSample sampleArea(Sampler &rng) const override{ NOT_IMPLEMENTED }

    std::string toString() const override {
        return "Sphere[]";
    }
};
} // namespace lightwave
REGISTER_SHAPE(Sphere, "sphere");
