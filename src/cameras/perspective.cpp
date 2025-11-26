#include <lightwave.hpp>

namespace lightwave {

/**
 * @brief A perspective camera with a given field of view angle and transform.
 *
 * In local coordinates (before applying m_transform), the camera looks in
 * positive z direction [0,0,1]. Pixels on the left side of the image ( @code
 * normalized.x < 0 @endcode ) are directed in negative x direction ( @code
 * ray.direction.x < 0 ), and pixels at the bottom of the image ( @code
 * normalized.y < 0 @endcode ) are directed in negative y direction ( @code
 * ray.direction.y < 0 ).
 */
class Perspective : public Camera {
private:
    float s_x, s_y;

public:
    Perspective(const Properties &properties) : Camera(properties) {
        const float fov           = properties.get<float>("fov");
        const float tan_fov       = tanf(Pi / 360 * fov);
        const std::string fovAxis = properties.get<std::string>("fovAxis");

        const float aspect_ratio = (float) m_resolution.x() / m_resolution.y();
        float s_x_norm, s_y_norm;

        if (fovAxis == "x") {
            s_x_norm = tan_fov;
            s_y_norm = s_x_norm / aspect_ratio;
        } else {
            s_y_norm = tan_fov;
            s_x_norm = s_y_norm * aspect_ratio;
        }

        s_x = s_x_norm;
        s_y = s_y_norm;
    }

    CameraSample sample(const Point2 &normalized, Sampler &rng) const override {
        const Point origin     = {};
        const Vector direction = { normalized.x() * s_x,
                                   normalized.y() * s_y,
                                   1.f };

        const auto camera_ray = Ray(origin, direction);
        const Ray world_ray   = m_transform->apply(camera_ray);
        return CameraSample{ .ray    = world_ray.normalized(),
                             .weight = Color(1.0f) };
    }

    std::string toString() const override {
        return tfm::format(
            "Perspective[\n"
            "  width = %d,\n"
            "  height = %d,\n"
            "  transform = %s,\n"
            "]",
            m_resolution.x(),
            m_resolution.y(),
            indent(m_transform));
    }
};

} // namespace lightwave

REGISTER_CAMERA(Perspective, "perspective")
