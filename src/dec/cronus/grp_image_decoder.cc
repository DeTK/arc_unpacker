#include "dec/cronus/grp_image_decoder.h"
#include "algo/pack/lzss.h"
#include "algo/range.h"
#include "dec/cronus/common.h"
#include "err.h"
#include "util/plugin_mgr.h"

using namespace au;
using namespace au::dec::cronus;

namespace
{
    enum EncType
    {
        Delta,
        SwapBytes,
        None,
    };

    struct Header final
    {
        size_t width;
        size_t height;
        size_t bpp;
        size_t input_offset;
        size_t output_size;
        bool flip;
        bool use_transparency;
        EncType encryption_type;
    };

    using HeaderFunc = std::function<Header(io::IStream&)>;
}

static void swap_decrypt(bstr &input, size_t encrypted_size)
{
    u16 *input_ptr = input.get<u16>();
    const u16 *input_end = input.end<const u16>();
    size_t repetitions = encrypted_size >> 1;
    while (repetitions-- && input_ptr < input_end)
    {
        *input_ptr = ((*input_ptr << 8) | (*input_ptr >> 8)) ^ 0x33CC;
        input_ptr++;
    }
}

struct GrpImageDecoder::Priv final
{
    util::PluginManager<HeaderFunc> plugin_mgr;
    Header header;
};

static bool validate_header(const Header &header)
{
    size_t expected_output_size = header.width * header.height;
    if (header.bpp == 8)
        expected_output_size += 1024;
    else if (header.bpp == 24)
        expected_output_size *= 3;
    else if (header.bpp == 32)
        expected_output_size *= 4;
    else
        return false;
    return header.output_size == expected_output_size;
}

static HeaderFunc reader_v1(u32 key1, u32 key2, u32 key3, EncType enc_type)
{
    return [=](io::IStream &input_stream)
    {
        Header header;
        header.width = input_stream.read_u32_le() ^ key1;
        header.height = input_stream.read_u32_le() ^ key2;
        header.bpp = input_stream.read_u32_le();
        input_stream.skip(4);
        header.output_size = input_stream.read_u32_le() ^ key3;
        input_stream.skip(4);
        header.use_transparency = false;
        header.flip = true;
        header.encryption_type = enc_type;
        return header;
    };
}

static HeaderFunc reader_v2()
{
    return [](io::IStream &input_stream)
    {
        Header header;
        input_stream.skip(4);
        header.output_size = input_stream.read_u32_le();
        input_stream.skip(8);
        header.width = input_stream.read_u32_le();
        header.height = input_stream.read_u32_le();
        header.bpp = input_stream.read_u32_le();
        input_stream.skip(8);
        header.flip = false;
        header.use_transparency = input_stream.read_u32_le() != 0;
        header.encryption_type = EncType::None;
        return header;
    };
}

GrpImageDecoder::GrpImageDecoder() : p(new Priv)
{
    p->plugin_mgr.add(
        "dokidoki", "Doki Doki Princess",
        reader_v1(0xA53CC35A, 0x35421005, 0xCF42355D, EncType::SwapBytes));

    p->plugin_mgr.add(
        "sweet", "Sweet Pleasure",
        reader_v1(0x2468FCDA, 0x4FC2CC4D, 0xCF42355D, EncType::Delta));

    p->plugin_mgr.add("nursery", "Nursery Song", reader_v2());
}

GrpImageDecoder::~GrpImageDecoder()
{
}

bool GrpImageDecoder::is_recognized_impl(io::File &input_file) const
{
    for (auto header_func : p->plugin_mgr.get_all())
    {
        input_file.stream.seek(0);
        try
        {
            p->header = header_func(input_file.stream);
            if (!validate_header(p->header))
                continue;
            p->header.input_offset = input_file.stream.tell();
            return true;
        }
        catch (...)
        {
            continue;
        }
    }
    return false;
}

res::Image GrpImageDecoder::decode_impl(
    const Logger &logger, io::File &input_file) const
{
    input_file.stream.seek(p->header.input_offset);
    auto data = input_file.stream.read_to_eof();

    if (p->header.encryption_type == EncType::Delta)
        delta_decrypt(data, get_delta_key(input_file.path.name()));
    else if (p->header.encryption_type == EncType::SwapBytes)
        swap_decrypt(data, p->header.output_size);
    data = algo::pack::lzss_decompress(data, p->header.output_size);

    std::unique_ptr<res::Image> image;

    if (p->header.bpp == 8)
    {
        res::Palette palette(256, data, res::PixelFormat::BGRA8888);
        image = std::make_unique<res::Image>(
            p->header.width, p->header.height, data.substr(1024), palette);
    }
    else if (p->header.bpp == 24)
    {
        image = std::make_unique<res::Image>(
            p->header.width, p->header.height, data, res::PixelFormat::BGR888);
    }
    else if (p->header.bpp == 32)
    {
        image = std::make_unique<res::Image>(
            p->header.width,
            p->header.height,
            data,
            res::PixelFormat::BGRA8888);
    }
    else
    {
        throw err::UnsupportedBitDepthError(p->header.bpp);
    }

    if (!p->header.use_transparency)
    {
        for (auto x : algo::range(p->header.width))
        for (auto y : algo::range(p->header.height))
            image->at(x, y).a = 0xFF;
    }
    if (p->header.flip)
        image->flip_vertically();

    return *image;
}

static auto _ = dec::register_decoder<GrpImageDecoder>("cronus/grp");