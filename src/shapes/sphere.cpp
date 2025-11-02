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

        // map the position from [-1,-1,0]..[+1,+1,0] to [0,0]..[1,1] by
        // discarding the z component and rescaling
        // surf.uv.x() = (position.x() + 1) / 2;
        // surf.uv.y() = (position.y() + 1) / 2;

        // TODO: Implement this
        // surf.tangent = Vector(position);

        surf.geometryNormal = Vector(position).normalized();
        surf.shadingNormal  = Vector(position).normalized();

        // TODO: not implemented
        surf.pdf = 1.0f;

        // Copied from Khalil's implementation
        // TODO: Come back to this and implement properly
        // TODO: Doesn't seem to be covered by tests so not sure if correct
        Vector up    = abs(surf.geometryNormal.z()) < 0.999f ? Vector(0, 0, 1)
                                                             : Vector(1, 0, 0);
        surf.tangent = up.cross(surf.geometryNormal).normalized();
    }

    bool intersect(const Ray &ray, Intersection &its,
                   Sampler &rng) const override {
        PROFILE("Sphere")
        const float r = 1.0;
        const Vector c{ 0.f, 0.f, 0.f };

        const float quad_a = ray.direction.dot(ray.direction);
        const float quad_b = 2 * Vector(ray.origin).dot(ray.direction);
        const float quad_c = Vector(ray.origin).dot(Vector(ray.origin)) - r * r;
        const float disc   = quad_b * quad_b - 4 * quad_a * quad_c;

        if (disc < 0) {
            return false;
        }

        float t1, t2;

        const float sqrt_disc = sqrt(disc);

        t1 = (-quad_b + sqrt_disc) / (2 * quad_a);
        t2 = (-quad_b - sqrt_disc) / (2 * quad_a);

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
        }

        else {
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

        // TODO: Doesn't seem to be covered by tests so not sure if correct
        its.wo = -ray.direction;

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