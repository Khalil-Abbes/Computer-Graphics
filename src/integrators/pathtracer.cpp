#include <lightwave.hpp>

namespace lightwave {

class PathTracer final : public SamplingIntegrator {
private:
    int m_maxDepth;
    bool m_neeFlag;
    bool m_useNee;

public:
    PathTracer(const Properties &properties) : SamplingIntegrator(properties) {
        m_maxDepth = properties.get<int>("depth", 2);
        m_neeFlag  = properties.get<bool>("nee", true);
        m_useNee   = m_neeFlag && m_scene->hasLights();
    }

    Color Li(const Ray &ray0, Sampler &rng) override {
        Color L(0.0f);
        Color T(1.0f);
        Ray ray = ray0;

        for (int b = 0;; ++b) { // Removed condition from loop
            Intersection its = m_scene->intersect(ray, rng);

            if (!its) {
                EmissionEval env = its.evaluateEmission();
                L += T * env.value;
                break;
            }

            if (EmissionEval e = its.evaluateEmission()) {
                L += T * e.value;
            }

            // Break before NEE/BSDF if we've reached max depth
            if (b >= m_maxDepth - 1) {
                break;
            }

            if (m_useNee) {
                LightSample ls = m_scene->sampleLight(rng);
                if (ls.light) {
                    DirectLightSample s =
                        ls.light->sampleDirect(its.position, rng);
                    if (s) {
                        // 1) Shadow ray towards the light
                        Ray shadow(its.position, s.wi);

                        // 2) Compute transmittance along this ray up to the
                        // light
                        //    - 0 if surface-occluded
                        //    - in (0,1] otherwise, including volumetric
                        //    attenuation
                        float Tr =
                            m_scene->transmittance(shadow, s.distance, rng);

                        // 3) Only add contribution if something makes it
                        // through
                        if (Tr > 0.0f) {
                            if (BsdfEval f = its.evaluateBsdf(s.wi)) {
                                L += T * (Tr * f.value) * s.weight /
                                     ls.probability;
                            }
                        }
                    }
                }
            }

            BsdfSample bs = its.sampleBsdf(rng);
            if (!bs)
                break;

            T *= bs.weight;
            ray = Ray(its.position, bs.wi);
        }

        return L;
    }

    std::string toString() const override {
        return tfm::format(
            "PathTracer[\n"
            "  maxDepth = %d,\n"
            "  neeFlag  = %s,\n"
            "  useNee   = %s\n"
            "]",
            m_maxDepth,
            indent(m_neeFlag),
            indent(m_useNee));
    }
};

} // namespace lightwave

REGISTER_INTEGRATOR(PathTracer, "pathtracer")
