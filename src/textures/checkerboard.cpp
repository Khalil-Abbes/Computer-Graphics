/**
 * @file checkerboard.cpp
 * @brief Checkerboard texture implementation for Assignment 3.1
 */

#include <lightwave.hpp>

namespace lightwave {

/**
 * @brief A checkerboard texture that alternates between two colors based on UV
 * coordinates.
 *
 * The pattern is created by using the floor and modulo operators on scaled UV
 * coordinates, which creates alternating squares in a checkerboard pattern.
 */
class Checkerboard : public Texture {
    /// The first color of the checkerboard pattern (default: black)
    Color m_color0;

    /// The second color of the checkerboard pattern (default: white)
    Color m_color1;

    /// Scale factor for UV coordinates to control pattern size
    Vector2 m_scale;

public:
    Checkerboard(const Properties &properties) {
        // Read color0 parameter (defaults to black Color(0))
        m_color0 = properties.get<Color>("color0", Color(0));

        // Read color1 parameter (defaults to white Color(1))
        m_color1 = properties.get<Color>("color1", Color(1));

        // Read scale parameter (defaults to (1, 1))
        // Can be specified as a single value or as "x,y"
        m_scale = properties.get<Vector2>("scale", Vector2(1, 1));
    }

    Color evaluate(const Point2 &uv) const override {
        // Scale the UV coordinates
        Point2 scaledUV = Point2(uv.x() * m_scale.x(), uv.y() * m_scale.y());

        // Apply floor to get integer grid coordinates
        int gridX = static_cast<int>(std::floor(scaledUV.x()));
        int gridY = static_cast<int>(std::floor(scaledUV.y()));

        // Use modulo to determine which color to use
        // If the sum of grid coordinates is even, use color0, otherwise color1
        bool useColor0 = ((gridX + gridY) % 2) == 0;

        // Return the appropriate color
        return useColor0 ? m_color0 : m_color1;
    }

    std::string toString() const override {
        return tfm::format(
            "Checkerboard[\n"
            "  color0 = %s,\n"
            "  color1 = %s,\n"
            "  scale = %s\n"
            "]",
            m_color0,
            m_color1,
            m_scale);
    }
};

} // namespace lightwave

REGISTER_TEXTURE(Checkerboard, "checkerboard")
