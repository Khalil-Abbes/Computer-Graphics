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
            return its.evaluateEmission().value;
        }

        Color result(0);

        // 2. Add emission if the hit surface is emissive (area lights etc.)
        // No need to check if (emission) - value will be zero if not emissive
        result += its.evaluateEmission().value;

        // 3. Sample a random non-intersectable (delta) light
        LightSample lightSample = m_scene->sampleLight(rng);
        // Use boolean operator instead of checking .light pointer
        if (lightSample) {
            DirectLightSample directSample =
                lightSample.light->sampleDirect(its.position, rng);
            if (directSample) {
                // Shadow ray - NO Epsilon offset needed
                Ray shadowRay(its.position, directSample.wi);

                // Use transmittance instead of intersect for shadow rays
                // This handles self-intersection prevention internally
                // transmittance returns a float, not Color
                float transmittance = m_scene->transmittance(
                    shadowRay, directSample.distance, rng);

                // If transmittance is non-zero, the light is visible
                if (transmittance > 0) {
                    BsdfEval bsdfEval = its.evaluateBsdf(directSample.wi);
                    if (bsdfEval) {
                        result += bsdfEval.value * directSample.weight *
                                  transmittance / lightSample.probability;
                    }
                }
            }
        }

        // 4. Sample BSDF for indirect ray; accumulate emission from area
        // lights/background
        // Typically, only do this for diffuse/visible surfaces (NOT for
        // specular/delta BSDFs)
        BsdfSample bsdfSample = its.sampleBsdf(rng);
        if (bsdfSample) {
            // NO Epsilon offset needed - self-intersections handled by
            // intersection routine
            Ray bounceRay(its.position, bsdfSample.wi);
            Intersection bounceIts = m_scene->intersect(bounceRay, rng);

            // Simplified: just get emission value directly
            Color nextEmission = bounceIts.evaluateEmission().value;

            result += bsdfSample.weight * nextEmission;
        }

        return result;
    }

    std::string toString() const override { return "DirectIntegrator[]"; }
};

} // namespace lightwave

REGISTER_INTEGRATOR(DirectIntegrator, "direct")
