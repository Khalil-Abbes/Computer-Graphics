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
    Vector s_x, s_y, v;

public:
    Perspective(const Properties &properties) : Camera(properties) {
        const float fov           = properties.get<float>("fov");
        const float fov_rad       = (fov * Pi) / 180.0;
        const std::string fovAxis = properties.get<std::string>("fovAxis");

        // NOT_IMPLEMENTED

        // hints:
        // * precompute any expensive operations here (most importantly
        // trigonometric functions)
        // * use m_resolution to find the aspect ratio of the image
        const float aspect_ratio = (float) m_resolution.x() / m_resolution.y();
        float s_x_norm, s_y_norm;

        if (fovAxis == "x") {
            s_x_norm = tan(fov_rad / 2) * 1.0;
            s_y_norm = s_x_norm / aspect_ratio;
        }

        else if (fovAxis == "y") {
            s_y_norm = tan(fov_rad / 2) * 1.0;
            s_x_norm = s_y_norm * aspect_ratio;
        }

        v = Vector{ 0.f, 0.f, 1.f };
        Vector u{ 0.f, 1.f, 0.f };

        const Vector s_x_bar = u.cross(v);
        const Vector s_y_bar = v.cross(s_x_bar);

        s_x = s_x_bar.normalized() * s_x_norm;
        s_y = s_y_bar.normalized() * s_y_norm;
    }

    CameraSample sample(const Point2 &normalized, Sampler &rng) const override {
        // hints:
        // * use m_transform to transform the local camera coordinate system
        // into the world coordinate system
        Ray camera_ray =
            Ray(Vector(0.f, 0.f, 0.f),
                (v - (-normalized.x()) * s_x - (-normalized.y()) * s_y));
        Ray world_ray = m_transform->apply(camera_ray);
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
