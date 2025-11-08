#include <lightwave.hpp>

namespace lightwave {

class DirectIntegrator : public SamplingIntegrator {
public:
    DirectIntegrator(const Properties &properties)
        : SamplingIntegrator(properties) {}

    Color Li(const Ray &ray, Sampler &rng) override {
        // Step a) Intersect the ray with the scene
        Intersection its = m_scene->intersect(ray, rng);

        // Step b) If no intersection, return background emission
        if (!its) {
            EmissionEval emission = its.evaluateEmission();
            return emission.value;
        }

        // **NEW: Add emission from hit surface (for area lights)**
        Color result(0);
        EmissionEval emission = its.evaluateEmission();
        if (!emission) {
            result += emission.value;
        }

        // Step c) Sample a random light source from the scene
        LightSample lightSample = m_scene->sampleLight(rng);

        if (lightSample.light) {
            DirectLightSample directSample =
                lightSample.light->sampleDirect(its.position, rng);

            if (directSample) {
                // Cast shadow ray
                Point shadowOrigin = its.position + Epsilon * directSample.wi;
                Ray shadowRay(shadowOrigin, directSample.wi);
                Intersection shadowIts = m_scene->intersect(shadowRay, rng);

                // Check occlusion
                bool occluded = false;
                if (shadowIts) {
                    if (std::isinf(directSample.distance) ||
                        shadowIts.t < directSample.distance - Epsilon) {
                        occluded = true;
                    }
                }

                if (!occluded) {
                    BsdfEval bsdfEval = its.evaluateBsdf(directSample.wi);
                    if (!bsdfEval.isInvalid()) {
                        result += bsdfEval.value * directSample.weight /
                                  lightSample.probability;
                    }
                }
            }
        }

        return result;
    }

    std::string toString() const override { return "DirectIntegrator[]"; }
};

} // namespace lightwave

REGISTER_INTEGRATOR(DirectIntegrator, "direct")
