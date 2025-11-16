#include <lightwave.hpp>

namespace lightwave {

class PathTracer final : public SamplingIntegrator {
private:
    int m_maxDepth; // maximum number of segments (bounces + primary)
    bool m_neeFlag; // user-set "nee" parameter
    bool m_useNee;  // actual NEE usage (disabled if no lights in scene)

public:
    PathTracer(const Properties &properties) : SamplingIntegrator(properties) {

        // Depth property: default 2
        m_maxDepth = properties.get<int>("depth", 2);

        // NEE toggle: default true
        m_neeFlag = properties.get<bool>("nee", true);

        // Disable NEE if there are no non-intersectable lights
        // (only environment map).
        m_useNee = m_neeFlag && m_scene->hasLights();
    }

    Color Li(const Ray &ray0, Sampler &rng) override {
        Color L(0.0f); // accumulated radiance
        Color T(1.0f); // path throughput

        Ray ray = ray0;

        for (int b = 0; b < m_maxDepth; ++b) {
            Intersection its = m_scene->intersect(ray, rng);

            if (!its) {
                EmissionEval env = its.evaluateEmission(); // background
                L += T * env.value;
                break;
            }

            if (EmissionEval e = its.evaluateEmission()) {
                L += T * e.value;
            }

            if (m_useNee) {
                LightSample ls = m_scene->sampleLight(rng);
                if (ls.light) {
                    DirectLightSample s =
                        ls.light->sampleDirect(its.position, rng);
                    if (s) {
                        Ray shadow(its.position, s.wi);
                        Intersection occ = m_scene->intersect(shadow, rng);

                        bool occluded = false;
                        if (occ) {
                            occluded = std::isinf(s.distance)
                                           ? true
                                           : (occ.t < s.distance);
                        }

                        if (!occluded) {
                            if (BsdfEval f = its.evaluateBsdf(s.wi)) {
                                L += T * f.value * s.weight / ls.probability;
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
