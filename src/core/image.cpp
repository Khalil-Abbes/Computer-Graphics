#include <lightwave/core.hpp>
#include <lightwave/image.hpp>
#include <lightwave/registry.hpp>
#include <lightwave/texture.hpp>
#include <stb_image.h>
#include <tinyexr.h>

namespace lightwave {

float Image::evaluateAlpha(const Point2 &uv) const {
    // 1. Calculate pixel coordinates
    // (This logic mirrors existing evaluate function to handle tiling)
    Point2 pixelCoords = uv;
    pixelCoords.x()    = pixelCoords.x() * m_resolution.x() - 0.5f;
    pixelCoords.y()    = (1.0f - pixelCoords.y()) * m_resolution.y() - 0.5f;

    Point2i p00(std::floor(pixelCoords.x()), std::floor(pixelCoords.y()));
    Point2i p10 = p00 + Vector2i(1, 0);
    Point2i p01 = p00 + Vector2i(0, 1);
    Point2i p11 = p00 + Vector2i(1, 1);

    // 2. Calculate interpolation weights
    float tx = pixelCoords.x() - p00.x();
    float ty = pixelCoords.y() - p00.y();

    // 3. Fetch alpha values (assuming  getAlpha or access m_alpha
    // directly) .

    auto getA = [&](const Point2i &p) {
        int x =
            (p.x() % m_resolution.x() + m_resolution.x()) % m_resolution.x();
        int y =
            (p.y() % m_resolution.y() + m_resolution.y()) % m_resolution.y();
        return m_alpha[y * m_resolution.x() + x];
    };

    float a00 = getA(p00);
    float a10 = getA(p10);
    float a01 = getA(p01);
    float a11 = getA(p11);

    // 4. Bilinear Interpolation
    return (1 - ty) * ((1 - tx) * a00 + tx * a10) +
           ty * ((1 - tx) * a01 + tx * a11);
}

void Image::loadImage(const std::filesystem::path &path, bool isLinearSpace) {
    const auto extension = path.extension();
    logger(EInfo, "loading image %s", path);

    if (extension == ".exr") {
        // loading of EXR files is handled by TinyEXR
        float *data;
        const char *err;
        // TinyEXR loads RGBA by default, so 'data' contains 4 floats per pixel
        if (LoadEXR(&data,
                    &m_resolution.x(),
                    &m_resolution.y(),
                    path.generic_string().c_str(),
                    &err)) {
            lightwave_throw("could not load image %s: %s", path, err);
        }

        m_data.resize(m_resolution.x() * m_resolution.y());
        m_alpha.resize(m_resolution.x() * m_resolution.y()); // <--- Resize
                                                             // alpha buffer

        auto it   = data;
        int index = 0;
        for (auto &pixel : m_data) {
            for (int i = 0; i < pixel.NumComponents; i++)
                pixel[i] = *it++; // Read R, G, B

            // CHANGE: Store alpha instead of skipping it
            m_alpha[index++] = *it++;
        }
        free(data);
    } else {
        // anything that is not an EXR file is handled by stb
        stbi_ldr_to_hdr_gamma(isLinearSpace ? 1.0f : 2.2f);

        int numChannels;
        // CHANGE: Request 4 channels (RGBA) instead of 3
        float *data =
            stbi_loadf(path.generic_string().c_str(),
                       &m_resolution.x(),
                       &m_resolution.y(),
                       &numChannels,
                       4); // <--- Changed 3 to 4 here for alpha masking
        if (data == nullptr) {
            lightwave_throw(
                "could not load image %s: %s", path, stbi_failure_reason());
        }

        m_data.resize(m_resolution.x() * m_resolution.y());
        m_alpha.resize(m_resolution.x() * m_resolution.y()); // <--- Resize
                                                             // alpha buffer

        auto it   = data;
        int index = 0;
        for (auto &pixel : m_data) {
            for (int i = 0; i < pixel.NumComponents; i++)
                pixel[i] = *it++; // Read R, G, B

            // CHANGE: Store the 4th channel (Alpha)
            m_alpha[index++] = *it++;
        }
        free(data);
    }
}

void Image::saveAt(const std::filesystem::path &path, float norm) const {
    if (resolution().isZero()) {
        logger(EWarn, "cannot save empty image %s!", path);
        return;
    }

    assert_condition(Color::NumComponents == 3, {
        logger(EError,
               "the number of components in Color has changed, you need to "
               "update Image::saveAt with new channel names.");
    });

    logger(EInfo, "saving image %s", path);

    // MARK: Create metadata

    std::string log = logger.history();
    std::vector<EXRAttribute> customAttributes;
    customAttributes.emplace_back(EXRAttribute{
        .name  = "log",
        .type  = "string",
        .value = reinterpret_cast<unsigned char *>(log.data()),
        .size  = int(log.size()),
    });

    // MARK: Create EXR header

    EXRHeader header;
    InitEXRHeader(&header);

    header.custom_attributes     = customAttributes.data();
    header.num_custom_attributes = int(customAttributes.size());

    header.compression_type =
        (m_resolution.x() < 16) && (m_resolution.y() < 16)
            ? TINYEXR_COMPRESSIONTYPE_NONE /* No compression for small image. */
            : TINYEXR_COMPRESSIONTYPE_ZIP;

    header.num_channels = Color::NumComponents;
    header.channels     = static_cast<EXRChannelInfo *>(malloc(
        sizeof(EXRChannelInfo) * static_cast<size_t>(header.num_channels)));
    header.pixel_types  = static_cast<int *>(
        malloc(sizeof(int) * static_cast<size_t>(header.num_channels)));
    header.requested_pixel_types = static_cast<int *>(
        malloc(sizeof(int) * static_cast<size_t>(header.num_channels)));

    // MARK: Create EXR image

    EXRImage image;
    InitEXRImage(&image);

    float *channelPtr[Color::NumComponents];
    image.width        = m_resolution.x();
    image.height       = m_resolution.y();
    image.num_channels = header.num_channels;
    image.images       = reinterpret_cast<unsigned char **>(channelPtr);

    // MARK: Copy normalized data

    std::vector<float> channels[Color::NumComponents];
    const size_t pixelCount = static_cast<size_t>(m_resolution.x()) *
                              static_cast<size_t>(m_resolution.y());

    for (int chan = 0; chan < Color::NumComponents; chan++) {
        channels[chan].resize(pixelCount);
        for (size_t px = 0; px < pixelCount; px++) {
            channels[chan][px] = m_data[px][chan] * norm;
        }

        header.pixel_types[chan] =
            TINYEXR_PIXELTYPE_FLOAT; // pixel type of input image
        header.requested_pixel_types[chan] =
            TINYEXR_PIXELTYPE_FLOAT; // save with float(fp32) pixel format
                                     // (i.e., no precision reduction)

        // Must be BGR order, since most EXR viewers expect this channel order.
        header.channels[chan].name[0]                 = "BGR"[chan];
        header.channels[chan].name[1]                 = 0;
        channelPtr[Color::NumComponents - (chan + 1)] = channels[chan].data();
    }

    // MARK: Save EXR

    const char *error;
    int ret = SaveEXRImageToFile(
        &image, &header, path.generic_string().c_str(), &error);

    header.num_custom_attributes = 0;
    header.custom_attributes     = nullptr; // memory freed by std::vector
    FreeEXRHeader(&header);

    if (ret != TINYEXR_SUCCESS) {
        logger(EError, "  error saving image %s: %s", path, error);
    }
}
} // namespace lightwave

REGISTER_CLASS(Image, "image", "default")
