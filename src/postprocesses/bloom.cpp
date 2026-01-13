#include <lightwave.hpp>

namespace lightwave {

static inline float luminance(const Color &c) {
    return 0.2126f * c.r() + 0.7152f * c.g() + 0.0722f * c.b();
}

static inline int clampi(int v, int lo, int hi) {
    return std::max(lo, std::min(v, hi));
}

static inline Color sample_clamped(const ref<Image> &img, int x, int y) {
    const Point2i res = img->resolution();
    x                 = clampi(x, 0, res.x() - 1);
    y                 = clampi(y, 0, res.y() - 1);
    return img->get(Point2i(x, y));
}

static std::vector<float> gaussianWeights(int radius, float sigma) {
    std::vector<float> w(2 * radius + 1);
    float sum = 0.f;
    for (int i = -radius; i <= radius; ++i) {
        float x       = float(i);
        float wi      = std::exp(-(x * x) / (2.f * sigma * sigma));
        w[i + radius] = wi;
        sum += wi;
    }
    for (float &wi : w)
        wi /= sum;
    return w;
}

class BloomMinimal : public Postprocess {
    float m_threshold;
    float m_intensity;
    int m_radius;
    float m_sigma;

public:
    BloomMinimal(const Properties &properties) : Postprocess(properties) {
        m_threshold = properties.get<float>("threshold", 1.0f);
        m_intensity = properties.get<float>("intensity", 0.08f);
        m_radius    = properties.get<int>("radius", 7);
        m_sigma     = properties.get<float>("sigma", 4.0f);
    }

    void execute() override {
        const Point2i res = m_input->resolution();
        m_output->initialize(res);

        // temp buffers (bright-pass and ping-pong blur)
        auto bright = std::make_shared<Image>();
        bright->initialize(res);

        auto temp = std::make_shared<Image>();
        temp->initialize(res);

        // 1) Bright-pass (hard threshold)
        for (const Point2i &p : bright->bounds()) {
            Color c        = m_input->get(p);
            float y        = luminance(c);
            bright->get(p) = (y > m_threshold) ? c : Color(0.f);
        }

        // 2) Separable Gaussian blur (horizontal then vertical)
        const auto w = gaussianWeights(m_radius, m_sigma);

        // horizontal: bright -> temp
        for (const Point2i &p : bright->bounds()) {
            Color acc(0.f);
            for (int i = -m_radius; i <= m_radius; ++i)
                acc +=
                    w[i + m_radius] * sample_clamped(bright, p.x() + i, p.y());
            temp->get(p) = acc;
        }

        // vertical: temp -> bright  (reuse bright as output of blur)
        for (const Point2i &p : bright->bounds()) {
            Color acc(0.f);
            for (int i = -m_radius; i <= m_radius; ++i)
                acc += w[i + m_radius] * sample_clamped(temp, p.x(), p.y() + i);
            bright->get(p) = acc;
        }

        // 3) Combine: HDR + intensity * blurredBright
        for (const Point2i &p : m_output->bounds()) {
            Color hdr        = m_input->get(p);
            Color b          = bright->get(p);
            m_output->get(p) = hdr + m_intensity * b;
        }

        Streaming stream{ *m_output };
        stream.update();
        m_output->save();
    }

    std::string toString() const override {
        return tfm::format(
            "BloomMinimal[\n"
            "  input = %s,\n"
            "  output = %s,\n"
            "  threshold = %f,\n"
            "  intensity = %f,\n"
            "  radius = %d, sigma = %f,\n"
            "]",
            indent(m_input),
            indent(m_output),
            m_threshold,
            m_intensity,
            m_radius,
            m_sigma);
    }
};

} // namespace lightwave

REGISTER_POSTPROCESS(BloomMinimal, "bloom_minimal");
