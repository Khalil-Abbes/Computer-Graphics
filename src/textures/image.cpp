#include <lightwave.hpp>
#include <lightwave/logger.hpp>
namespace lightwave {

class ImageTexture : public Texture {
    enum class BorderMode {
        Clamp,
        Repeat,
    };

    enum class FilterMode {
        Nearest,
        Bilinear,
    };

    ref<Image> m_image;
    float m_exposure;
    BorderMode m_border;
    FilterMode m_filter;

public:
    ImageTexture(const Properties &properties) {
        if (properties.has("filename")) {
            m_image = std::make_shared<Image>(properties);
        } else {
            m_image = properties.getChild<Image>();
        }
        m_exposure = properties.get<float>("exposure", 1);

        // clang-format off
        m_border = properties.getEnum<BorderMode>("border", BorderMode::Repeat, {
            { "clamp", BorderMode::Clamp },
            { "repeat", BorderMode::Repeat },
        });

        m_filter = properties.getEnum<FilterMode>("filter", FilterMode::Bilinear, {
            { "nearest", FilterMode::Nearest },
            { "bilinear", FilterMode::Bilinear },
        });
        // clang-format on
    }

    Color evaluate(const Point2 &uv) const override {
        // Get image dimensions
        int width  = m_image->resolution().x();
        int height = m_image->resolution().y();

        // Convert UV [0,1] to continuous texture coordinates
        // Subtract 0.5 to account for pixel-centered convention
        float x = uv.x() * width - 0.5f;
        float y = (1.0f - uv.y()) * height - 0.5f;

        Color result;

        if (m_filter == FilterMode::Nearest) {
            // Nearest-neighbor: round to nearest texel
            result = sampleNearest(x, y, width, height);
        } else if (m_filter == FilterMode::Bilinear) {
            // Bilinear: interpolate between 4 nearest texels
            result = sampleBilinear(x, y, width, height);
        }

        // Apply exposure correction
        return result * m_exposure;
    }

    float scalar(const Point2 &uv) const override {
        return std::clamp(m_image->evaluateAlpha(uv), 0.0f, 1.0f);
    }

private:
    // Apply border handling to integer coordinates
    int applyBorderMode(int coord, int size) const {
        if (m_border == BorderMode::Clamp) {
            // Clamp to [0, size-1]
            return clamp(coord, 0, size - 1);
        } else if (m_border == BorderMode::Repeat) {
            // Repeat mode: wrap using modulo
            // Handle negative coordinates properly
            coord = coord % size;
            if (coord < 0) {
                coord += size;
            }
            return coord;
        }
    }

    // Sample using nearest-neighbor filtering
    Color sampleNearest(float x, float y, int width, int height) const {
        // Round to nearest integer
        int ix = static_cast<int>(std::round(x));
        int iy = static_cast<int>(std::round(y));

        // Apply border handling to integer coordinates
        ix = applyBorderMode(ix, width);
        iy = applyBorderMode(iy, height);

        return m_image->get(Point2i(ix, iy));
    }

    // Sample using bilinear filtering
    Color sampleBilinear(float x, float y, int width, int height) const {
        // Get integer coordinates of the four surrounding texels
        int x0 = static_cast<int>(std::floor(x));
        int y0 = static_cast<int>(std::floor(y));
        int x1 = x0 + 1;
        int y1 = y0 + 1;

        // Apply border handling to integer coordinates (NOT to UV!)
        x0 = applyBorderMode(x0, width);
        y0 = applyBorderMode(y0, height);
        x1 = applyBorderMode(x1, width);
        y1 = applyBorderMode(y1, height);

        // Compute fractional parts for interpolation
        float tx = x - std::floor(x);
        float ty = y - std::floor(y);

        // Fetch the four corner texels
        Color T00 = m_image->get(Point2i(x0, y0));
        Color T10 = m_image->get(Point2i(x1, y0));
        Color T01 = m_image->get(Point2i(x0, y1));
        Color T11 = m_image->get(Point2i(x1, y1));

        // Bilinear interpolation
        // First interpolate horizontally (along x)
        Color T0 = tx * T10 + (1 - tx) * T00;
        Color T1 = tx * T11 + (1 - tx) * T01;

        // Then interpolate vertically (along y)
        return ty * T1 + (1 - ty) * T0;
    }

public:
    std::string toString() const override {
        return tfm::format(
            "ImageTexture[\n"
            "  image = %s,\n"
            "  exposure = %f,\n"
            "]",
            indent(m_image),
            m_exposure);
    }
};

} // namespace lightwave

REGISTER_TEXTURE(ImageTexture, "image")
