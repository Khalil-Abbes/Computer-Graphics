#include <lightwave/core.hpp>
#include <lightwave/instance.hpp>
#include <lightwave/registry.hpp>
#include <lightwave/sampler.hpp>
#include <lightwave/texture.hpp> // Required for m_alpha

namespace lightwave {

void Instance::transformFrame(SurfaceEvent &surf, const Vector &wo) const {
    surf.geometryNormal =
        surf.instance->m_transform->applyNormal(surf.geometryNormal)
            .normalized();
    surf.shadingNormal =
        surf.instance->m_transform->applyNormal(surf.shadingNormal)
            .normalized();
    // surf.tangent is not transformed here for simplicity, but could be
    surf.position = surf.instance->m_transform->apply(surf.position);
}

inline void validateIntersection(const Intersection &its) {
    assert_finite(its.t, {
        logger(
            EError,
            "  your intersection produced a non-finite intersection distance");
        logger(EError, "  offending shape: %s", its.instance->shape());
    });
    assert_condition(its.t >= Epsilon, {
        logger(EError,
               "  your intersection is susceptible to self-intersections");
        logger(EError, "  offending shape: %s", its.instance->shape());
        logger(EError,
               "  returned t: %.3g (smaller than Epsilon = %.3g)",
               its.t,
               Epsilon);
    });
}

bool Instance::intersect(const Ray &worldRay, Intersection &its,
                         Sampler &rng) const {
    // --- PATH 1: Fast Path (No Transform) ---
    if (!m_transform) {
        const Ray localRay        = worldRay;
        const bool wasIntersected = m_shape->intersect(localRay, its, rng);
        if (wasIntersected) {
            its.instance = this;
            validateIntersection(its);

            // [MISSING LOGIC ADDED HERE]
            if (m_alpha) {
                // LOGGER DEBUG: Check if we are actually reading the alpha
                // logger(EInfo,
                //        "Hit object with alpha mask at UV: %.2f, %.2f",
                //        its.uv.x(),
                //         its.uv.y());

                float alphaVal = m_alpha->scalar(its.uv);
                if (rng.next() > alphaVal) {
                    return false; // Treat as if we missed (Transparent)
                }
            }
        }
        return wasIntersected;
    }

    // --- PATH 2: Slow Path (With Transform) ---
    const float previousT = its.t;
    Ray localRay          = m_transform->inverse(worldRay).normalized();
    if (its) {
        its.t = (m_transform->inverse(its.position) - localRay.origin).length();
    }

    const bool wasIntersected = m_shape->intersect(localRay, its, rng);
    if (wasIntersected) {
        its.instance = this;
        validateIntersection(its);

        //
        if (m_alpha) {
            // LOGGER DEBUG
            // logger(EInfo,
            //       "Hit Transformed object at UV: %.2f, %.2f",
            //       its.uv.x(),
            //       its.uv.y());

            float alphaVal = m_alpha->scalar(its.uv);
            if (rng.next() > alphaVal) {
                its.t = previousT; // IMPORTANT: Restore old T so we can hit
                                   // things behind this
                return false;
            }
        }

        transformFrame(its, -localRay.direction);
        its.t = (its.position - worldRay.origin).length();
    } else {
        its.t = previousT;
    }

    return wasIntersected;
}

float Instance::transmittance(const Ray &worldRay, float tMax,
                              Sampler &rng) const {
    // If alpha mask exists, we must use the full intersection test
    // to determine if the specific UV coordinate is transparent.
    if (m_alpha) {
        Intersection its;
        if (this->intersect(worldRay, its, rng)) {
            if (its.t < tMax) {
                return 0.0f; // Blocked by opaque part
            }
        }
        return 1.0f; // Transparent or missed
    }

    if (!m_transform) {
        return m_shape->transmittance(worldRay, tMax, rng);
    }

    Ray localRay = m_transform->inverse(worldRay);

    const float dLength = localRay.direction.length();
    if (dLength == 0)
        return 0;
    localRay.direction /= dLength;
    tMax *= dLength;

    return m_shape->transmittance(localRay, tMax, rng);
}

Bounds Instance::getBoundingBox() const {
    if (!m_transform) {
        return m_shape->getBoundingBox();
    }

    const Bounds untransformedAABB = m_shape->getBoundingBox();
    if (untransformedAABB.isUnbounded()) {
        return Bounds::full();
    }

    Bounds result;
    for (int point = 0; point < 8; point++) {
        Point p = untransformedAABB.min();
        for (int dim = 0; dim < p.Dimension; dim++) {
            if ((point >> dim) & 1) {
                p[dim] = untransformedAABB.max()[dim];
            }
        }
        p = m_transform->apply(p);
        result.extend(p);
    }
    return result;
}

Point Instance::getCentroid() const {
    if (!m_transform) {
        return m_shape->getCentroid();
    }
    return m_transform->apply(m_shape->getCentroid());
}

AreaSample Instance::sampleArea(Sampler &rng) const {
    AreaSample sample = m_shape->sampleArea(rng);
    transformFrame(sample, Vector());
    return sample;
}

} // namespace lightwave

REGISTER_CLASS(Instance, "instance", "default")