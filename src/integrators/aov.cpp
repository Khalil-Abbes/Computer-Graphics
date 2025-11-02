#include <lightwave.hpp>

namespace lightwave {
class AOVIntegrator : public SamplingIntegrator {
    std::string m_variable;
    int m_scale;

public:
    AOVIntegrator(const Properties &properties)
        : SamplingIntegrator(properties) {

        m_variable = properties.get<std::string>("variable");
        m_scale    = properties.get<int>("scale", 1);
    }

    Color Li(const Ray &ray, Sampler &rng) override {
        if (m_variable == "normals") {
            const Intersection intersection = m_scene->intersect(ray, rng);
            Vector shading_normal;
            if (intersection) {
                shading_normal = intersection.shadingNormal;
            } else {
                shading_normal = Vector(0, 0, 0);
            }

            shading_normal = (shading_normal + Vector(1.f, 1.f, 1.f)) / 2;
            return Color(shading_normal);
        } else if (m_variable == "bvh") {
            const Intersection intersection = m_scene->intersect(ray, rng);
            Color shading_normal;
            shading_normal = Color(intersection.stats.bvhCounter,
                                   intersection.stats.primCounter,
                                   0);
            return shading_normal / m_scale;
        }
    }

    std::string toString() const override {
        return tfm::format(
            "AOVIntegrator[\n"
            "  sampler = %s,\n"
            "  image = %s,\n"
            "]",
            indent(m_sampler),
            indent(m_image));
    }
};
} // namespace lightwave

// this informs lightwave to use our class AOVIntegrator whenever a
// <integrator type="aov" /> is found in a scene file
REGISTER_INTEGRATOR(AOVIntegrator, "aov")
