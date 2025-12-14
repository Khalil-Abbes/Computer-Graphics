#include <lightwave.hpp>

namespace lightwave {

class Tonemap : public Postprocess {

public:
    Tonemap(const Properties &properties) : Postprocess(properties) {}

    void execute() override {
        m_output->initialize(m_input->resolution());

        // Standard single-threaded loop
        // m_output->bounds() returns a range that iterates over every pixel
        // coordinate
        for (const Point2i &pixel : m_output->bounds()) {
            Color c              = m_input->get(pixel);
            m_output->get(pixel) = c / (c + Color(1.0f));
        }

        Streaming stream{ *m_output };
        stream.update();
        m_output->save();
    }

    std::string toString() const override {
        return tfm::format(
            "Tonemap[\n"
            "  input = %s,\n"
            "  output = %s,\n"
            "]",
            indent(m_input),
            indent(m_output));
    }
};

} // namespace lightwave

REGISTER_POSTPROCESS(Tonemap, "tonemap");