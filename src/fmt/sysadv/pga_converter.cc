// PGA image file
//
// Company:   Palette
// Engine:    sysadv
// Extension: .pga
// Archives:  PAK
//
// Known games:
// - Moshimo Ashita ga Harenaraba

#include "fmt/sysadv/pga_converter.h"

using namespace au;
using namespace au::fmt::sysadv;

static const bstr magic = "PGAPGAH\x0A"_b;

bool PgaConverter::is_recognized_internal(File &file) const
{
    return file.io.read(magic.size()) == magic;
}

std::unique_ptr<File> PgaConverter::decode_internal(File &file) const
{
    file.io.skip(magic.size());
    std::unique_ptr<File> output_file(new File);
    output_file->io.write("\x89\x50\x4E\x47"_b);
    output_file->io.write("\x0D\x0A\x1A\x0A"_b);
    output_file->io.write("\x00\x00\x00\x0D"_b);
    output_file->io.write("IHDR"_b);
    file.io.seek(0xB);
    output_file->io.write_from_io(file.io);
    output_file->name = file.name;
    output_file->change_extension("png");
    return output_file;
}