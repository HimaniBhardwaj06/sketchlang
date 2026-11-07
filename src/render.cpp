#include "render.hpp"
#include <iomanip>
#include <sstream>

namespace sk {

std::string toSvg(const std::vector<DrawCmd>& draws) {
    std::ostringstream os;
    os << std::setprecision(6);
    os << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 100\" width=\"600\" height=\"600\">\n";
    os << "<rect width=\"100\" height=\"100\" fill=\"white\"/>\n";
    for (const DrawCmd& d : draws) {
        std::ostringstream color;
        color << "rgb(" << int(d.r) << "," << int(d.g) << "," << int(d.bl) << ")";
        switch (d.kind) {
            case DrawCmd::Kind::Circle:
                os << "<circle cx=\"" << d.a << "\" cy=\"" << d.b << "\" r=\"" << d.c
                   << "\" fill=\"none\" stroke=\"" << color.str() << "\" stroke-width=\"0.4\"/>\n";
                break;
            case DrawCmd::Kind::Rect:
                os << "<rect x=\"" << d.a << "\" y=\"" << d.b << "\" width=\"" << d.c << "\" height=\"" << d.d
                   << "\" fill=\"none\" stroke=\"" << color.str() << "\" stroke-width=\"0.4\"/>\n";
                break;
            case DrawCmd::Kind::Line:
                os << "<line x1=\"" << d.a << "\" y1=\"" << d.b << "\" x2=\"" << d.c << "\" y2=\"" << d.d
                   << "\" stroke=\"" << color.str() << "\" stroke-width=\"0.4\"/>\n";
                break;
        }
    }
    os << "</svg>\n";
    return os.str();
}

} // namespace sk
