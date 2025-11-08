#include <lightwave.hpp>

namespace lightwave {

class DirectIntegrator : public SamplingIntegrator {
public:
    DirectIntegrator(const Properties &properties)
        : SamplingIntegrator(properties) {}

    Color Li(const Ray &ray, Sampler &rng) override {
        Intersection its = m_scene->intersect(ray, rng);

        // 1. Return environment/background if no hit.
        if (!its) {
            EmissionEval emission = its.evaluateEmission();
            return emission.value;
        }

        Color result(0);

        // 2. Add emission if the hit surface is emissive (area lights etc.)
        EmissionEval emission = its.evaluateEmission();
        if (emission) {
            result += emission.value;
        }

        // 3. Sample a random non-intersectable (delta) light
        LightSample lightSample = m_scene->sampleLight(rng);
        if (lightSample.light) {
            DirectLightSample directSample =
                lightSample.light->sampleDirect(its.position, rng);
            if (directSample) {
                // Shadow ray
                Point shadowOrigin = its.position + Epsilon * directSample.wi;
                Ray shadowRay(shadowOrigin, directSample.wi);
                Intersection shadowIts = m_scene->intersect(shadowRay, rng);
                bool occluded          = false;
                if (shadowIts) {
                    if (std::isinf(directSample.distance) ||
                        shadowIts.t < directSample.distance - Epsilon) {
                        occluded = true;
                    }
                }
                if (!occluded) {
                    BsdfEval bsdfEval = its.evaluateBsdf(directSample.wi);
                    if (bsdfEval) {
                        result += bsdfEval.value * directSample.weight /
                                  lightSample.probability;
                    }
                }
            }
        }

        // 4. Sample BSDF for indirect ray; accumulate emission from area
        // lights/background Typically, only do this for diffuse/visible
        // surfaces (NOT for specular/delta BSDFs)
        BsdfSample bsdfSample = its.sampleBsdf(rng);
        if (bsdfSample) {
            Point newOrigin = its.position + Epsilon * bsdfSample.wi;
            Ray bounceRay(newOrigin, bsdfSample.wi);
            Intersection bounceIts = m_scene->intersect(bounceRay, rng);

            Color nextEmission(0);
            if (bounceIts) {
                EmissionEval bounceEmission = bounceIts.evaluateEmission();
                if (bounceEmission) {
                    nextEmission = bounceEmission.value;
                }
            } else {
                nextEmission = bounceIts.evaluateEmission().value;
            }
            result += bsdfSample.weight * nextEmission;
        }
        return result;
    }

    std::string toString() const override { return "DirectIntegrator[]"; }
};

} // namespace lightwave

REGISTER_INTEGRATOR(DirectIntegrator, "direct")
