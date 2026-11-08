#include "xenos_hlsl.h"
#include "settings.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <algorithm>
#include <map>
#include <set>

namespace
{
    using namespace XenosHlsl;

    const char* const Components = "xyzw";

    std::string Format(const char* format, ...)
    {
        char buffer[1024];
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(buffer, sizeof(buffer), format, arguments);
        va_end(arguments);
        return buffer;
    }

    struct Translator
    {
        const std::vector<uint32_t>& words;
        const bool pixel;
        Translation out;
        std::string body;
        std::set<uint32_t> vertexSlots;
        std::map<uint32_t, uint32_t> vertexStrides;
        std::set<uint32_t> textureSlots;
        std::set<uint32_t> weightSlots;   // the slots getWeights is asked of: the shadows' filtering
        std::set<uint32_t> exportsUsed;
        int indent = 1;
        bool predicated = false;   // inside a predicated exec block

        Translator(const std::vector<uint32_t>& w, bool p) : words(w), pixel(p) {}

        // A float constant by index, or relative to a0. The indices read
        // are noted, and packed once the program is translated, so the
        // backend uploads only those; a relative read means the whole file.
        std::set<uint32_t> constantsRead;
        bool relativeConstants = false;

        std::string Constant(uint32_t index, bool relative)
        {
            if (relative) { relativeConstants = true; return Format("c[(a0 + %u) & 255]", index & 0xFF); }
            constantsRead.insert(index & 0xFF);
            return Format("c[%u]", index & 0xFF);
        }

        // The body's c[N] rewritten to the packed positions.
        void PackConstants()
        {
            if (relativeConstants || constantsRead.empty()) return;
            uint32_t position[256];
            for (uint32_t i = 0; i < 256; i++) position[i] = 0;
            for (uint32_t index : constantsRead)
            {
                position[index] = uint32_t(out.constantMap.size());
                out.constantMap.push_back(uint16_t(index));
            }
            std::string packed;
            packed.reserve(body.size());
            for (size_t i = 0; i < body.size();)
            {
                const bool word = i > 0 && (isalnum(static_cast<unsigned char>(body[i - 1])) || body[i - 1] == '_');
                if (!word && body.compare(i, 2, "c[") == 0 && isdigit(static_cast<unsigned char>(body[i + 2])))
                {
                    size_t end = i + 2;
                    while (end < body.size() && isdigit(static_cast<unsigned char>(body[end]))) end++;
                    if (end < body.size() && body[end] == ']')
                    {
                        const uint32_t index = uint32_t(strtoul(body.c_str() + i + 2, nullptr, 10)) & 0xFF;
                        packed += Format("c[%u]", position[index]);
                        i = end + 1;
                        continue;
                    }
                }
                packed += body[i];
                i++;
            }
            body.swap(packed);
        }

        void Line(const std::string& text)
        {
            body.append(size_t(indent) * 4, ' ');
            body += text;
            body += '\n';
        }

        // --- operands --------------------------------------------------------

        // A source operand of a vector operation, all four components after
        // the relative swizzle, the absolute value and the sign.
        std::string Source(uint32_t reg, bool temporary, uint32_t swizzle, bool negate,
                           bool absolute, bool relative)
        {
            std::string base;
            if (temporary)
                base = (reg & 0x40) ? Format("r[(%u + aL) & 31]", reg & 0x1F) : Format("r[%u]", reg & 0x1F);
            else
                base = Constant(reg, relative);
            std::string swizzled = base + ".";
            for (uint32_t i = 0; i < 4; i++)
                swizzled += Components[(i + ((swizzle >> (i * 2)) & 3)) & 3];
            if (absolute) swizzled = "abs(" + swizzled + ")";
            if (negate) swizzled = "(-" + swizzled + ")";
            return swizzled;
        }

        // The two components of the third operand a scalar operation takes:
        // `a` is the one the w lane's swizzle picks, `b` the one the x lane's
        // does.
        void ScalarPair(uint32_t reg, bool temporary, uint32_t swizzle, bool negate,
                        bool absolute, bool relative, bool vectorTakesThree, std::string& a, std::string& b, std::string& single)
        {
            std::string base;
            if (temporary) base = (reg & 0x40) ? Format("r[(%u + aL) & 31]", reg & 0x1F) : Format("r[%u]", reg & 0x1F);
            else base = Constant(reg, relative);
            // Which lanes of the third operand the two come from. The two
            // open translators disagree: Xenia says w and x, freedreno's
            // compiler says z and w and writes its operands into both pairs
            // to be safe. The title's programs say both, by the vector
            // operation the scalar one is issued with. When the vector
            // operation takes two operands the third is the scalar's alone:
            // the compiler then puts the two in w and x and fills the other
            // lanes with copies of x, and the products of a normal's
            // components that light a surface (xz, zz, yz, yy, distinct)
            // only come out with w and x. When the vector operation takes
            // three (mad, the conditional moves, dp2add), it reads the same
            // operand, lanes x, y, z, and the scalar's are z and w: the water
            // scales a texture coordinate by two, three and four that way,
            // and the grass's blade stands on its base, where w and x would
            // mix x into y and lay the blade flat. COD3_SCALARLANES=wx or
            // =zw uses one reading everywhere, for comparison.
            static const int forced = []() {
                const char* text = getenv("COD3_SCALARLANES");
                if (text == nullptr) return 0;
                if (strcmp(text, "wx") == 0) return 1;
                if (strcmp(text, "zw") == 0) return 2;
                return 0;
            }();
            // The operations that take one operand read lane w whichever
            // vector operation they go with; both translators agree there.
            const bool zw = forced == 2 || (forced == 0 && vectorTakesThree);
            const uint32_t laneA = zw ? 2 : 3, laneB = zw ? 3 : 0;
            a = base + "." + Components[(laneA + ((swizzle >> (laneA * 2)) & 3)) & 3];
            b = base + "." + Components[(laneB + ((swizzle >> (laneB * 2)) & 3)) & 3];
            single = base + "." + Components[(3 + ((swizzle >> 6) & 3)) & 3];
            if (absolute) single = "abs(" + single + ")";
            if (negate) single = "(-" + single + ")";
            if (absolute) { a = "abs(" + a + ")"; b = "abs(" + b + ")"; }
            if (negate) { a = "(-" + a + ")"; b = "(-" + b + ")"; }
        }

        std::string Mask(uint32_t mask)
        {
            std::string text;
            for (uint32_t i = 0; i < 4; i++) if (mask & (1u << i)) text += Components[i];
            return text;
        }

        std::string Destination(uint32_t reg, bool exported)
        {
            if (!exported) return Format("r[%u]", reg & 0x1F);
            exportsUsed.insert(reg);
            if (pixel)
            {
                if (reg < 4) return Format("oC%u", reg);
                if (reg == 61) { out.writesDepth = true; return "oDepth4"; }
                out.problem = Format("pixel export %u", reg);
                return "oUnused";
            }
            if (reg < 16) return Format("o%u", reg);
            if (reg == 62) return "oPos";
            if (reg == 63) return "oPointSize";
            out.problem = Format("vertex export %u", reg);
            return "oUnused";
        }

        // --- ALU ---------------------------------------------------------------

        void Alu(uint32_t d0, uint32_t d1, uint32_t d2)
        {
            const uint32_t vectorDest = d0 & 0x3F;
            const bool absoluteConstants = ((d0 >> 7) & 1) != 0;
            const uint32_t scalarDest = (d0 >> 8) & 0x3F;
            const bool exported = ((d0 >> 15) & 1) != 0;
            const uint32_t vectorMask = (d0 >> 16) & 0xF;
            const uint32_t scalarMask = (d0 >> 20) & 0xF;
            const bool vectorClamp = ((d0 >> 24) & 1) != 0;
            const bool scalarClamp = ((d0 >> 25) & 1) != 0;
            const uint32_t scalarOpcode = (d0 >> 26) & 0x3F;

            const uint32_t swizzle3 = d1 & 0xFF;
            const uint32_t swizzle2 = (d1 >> 8) & 0xFF;
            const uint32_t swizzle1 = (d1 >> 16) & 0xFF;
            const bool negate3 = ((d1 >> 24) & 1) != 0;
            const bool negate2 = ((d1 >> 25) & 1) != 0;
            const bool negate1 = ((d1 >> 26) & 1) != 0;
            const bool predicateCondition = ((d1 >> 27) & 1) != 0;
            const bool isPredicated = ((d1 >> 28) & 1) != 0;
            const bool relative0 = ((d1 >> 31) & 1) != 0;
            const bool relative1 = ((d1 >> 30) & 1) != 0;

            const uint32_t reg3 = d2 & 0xFF;
            const uint32_t reg2 = (d2 >> 8) & 0xFF;
            const uint32_t reg1 = (d2 >> 16) & 0xFF;
            const uint32_t vectorOpcode = (d2 >> 24) & 0x1F;
            const bool temp3 = ((d2 >> 29) & 1) != 0;
            const bool temp2 = ((d2 >> 30) & 1) != 0;
            const bool temp1 = ((d2 >> 31) & 1) != 0;

            const bool rel1 = !temp1 && relative0;
            const bool rel2 = !temp2 && (temp1 ? relative0 : relative1);
            const bool rel3 = !temp3 && ((temp1 && temp2) ? relative0 : relative1);

            const std::string a = Source(reg1, temp1, swizzle1, negate1, temp1 ? (reg1 & 0x80) != 0 : absoluteConstants, rel1);
            const std::string b = Source(reg2, temp2, swizzle2, negate2, temp2 ? (reg2 & 0x80) != 0 : absoluteConstants, rel2);
            const std::string c = Source(reg3, temp3, swizzle3, negate3, temp3 ? (reg3 & 0x80) != 0 : absoluteConstants, rel3);
            std::string sa, sb, s1;
            const bool vectorTakesThree = vectorOpcode == 11 || (vectorOpcode >= 12 && vectorOpcode <= 14) || vectorOpcode == 17;
            ScalarPair(reg3, temp3, swizzle3, negate3, temp3 ? (reg3 & 0x80) != 0 : absoluteConstants, rel3, vectorTakesThree, sa, sb, s1);

            Line("{");
            indent++;
            if (isPredicated) Line(Format("if (p0 == %s) {", predicateCondition ? "true" : "false"));
            if (isPredicated) indent++;

            // The vector operation.
            std::string vector;
            bool vectorSetsPredicate = false;
            bool kill = false;
            switch (vectorOpcode)
            {
            case 0:  vector = Format("(%s + %s)", a.c_str(), b.c_str()); break;
            case 1:  vector = Format("mulL(%s, %s)", a.c_str(), b.c_str()); break;
            case 2:  vector = Format("max(%s, %s)", a.c_str(), b.c_str()); break;
            case 3:  vector = Format("min(%s, %s)", a.c_str(), b.c_str()); break;
            case 4:  vector = Format("select4(%s == %s)", a.c_str(), b.c_str()); break;
            case 5:  vector = Format("select4(%s > %s)", a.c_str(), b.c_str()); break;
            case 6:  vector = Format("select4(%s >= %s)", a.c_str(), b.c_str()); break;
            case 7:  vector = Format("select4(%s != %s)", a.c_str(), b.c_str()); break;
            case 8:  vector = Format("frac(%s)", a.c_str()); break;
            case 9:  vector = Format("trunc(%s)", a.c_str()); break;
            case 10: vector = Format("floor(%s)", a.c_str()); break;
            case 11: vector = Format("(mulL(%s, %s) + %s)", a.c_str(), b.c_str(), c.c_str()); break;
            // The conditional moves are written as a blend by a mask rather
            // than a component wise ternary: the host's compiler has an
            // internal error on the ternary when an operand is negated.
            case 12: vector = Format("blend4(select4(%s == 0.0), %s, %s)", a.c_str(), b.c_str(), c.c_str()); break;
            case 13: vector = Format("blend4(select4(%s >= 0.0), %s, %s)", a.c_str(), b.c_str(), c.c_str()); break;
            case 14: vector = Format("blend4(select4(%s > 0.0), %s, %s)", a.c_str(), b.c_str(), c.c_str()); break;
            case 15: vector = Format("dot(mulL(%s, %s), float4(1.0, 1.0, 1.0, 1.0)).xxxx", a.c_str(), b.c_str()); break;
            case 16: vector = Format("dot(mulL(%s, %s).xyz, float3(1.0, 1.0, 1.0)).xxxx", a.c_str(), b.c_str()); break;
            case 17: vector = Format("(dot(mulL(%s, %s).xy, float2(1.0, 1.0)) + (%s).x).xxxx", a.c_str(), b.c_str(), c.c_str()); break;
            case 18: vector = Format("cubeMap(%s, %s)", a.c_str(), b.c_str()); break;
            case 19: vector = Format("max4(%s).xxxx", a.c_str()); break;
            case 20: vector = Format("((%s).w == 0.0 ? 0.0 : (%s).x + 1.0).xxxx", a.c_str(), b.c_str()); vectorSetsPredicate = true;
                     Line(Format("p0 = ((%s).w == 0.0) && ((%s).x == 0.0);", a.c_str(), b.c_str())); break;
            case 21: vector = Format("((%s).w == 0.0 ? 0.0 : (%s).x + 1.0).xxxx", a.c_str(), b.c_str()); vectorSetsPredicate = true;
                     Line(Format("p0 = ((%s).w == 0.0) && ((%s).x != 0.0);", a.c_str(), b.c_str())); break;
            case 22: vector = Format("((%s).w == 0.0 ? 0.0 : (%s).x + 1.0).xxxx", a.c_str(), b.c_str()); vectorSetsPredicate = true;
                     Line(Format("p0 = ((%s).w == 0.0) && ((%s).x > 0.0);", a.c_str(), b.c_str())); break;
            case 23: vector = Format("((%s).w == 0.0 ? 0.0 : (%s).x + 1.0).xxxx", a.c_str(), b.c_str()); vectorSetsPredicate = true;
                     Line(Format("p0 = ((%s).w == 0.0) && ((%s).x >= 0.0);", a.c_str(), b.c_str())); break;
            case 24: vector = Format("select4(%s == %s)", a.c_str(), b.c_str()); kill = true; break;
            case 25: vector = Format("select4(%s > %s)", a.c_str(), b.c_str()); kill = true; break;
            case 26: vector = Format("select4(%s >= %s)", a.c_str(), b.c_str()); kill = true; break;
            case 27: vector = Format("select4(%s != %s)", a.c_str(), b.c_str()); kill = true; break;
            case 28: vector = Format("float4(1.0, (%s).y * (%s).y, (%s).z, (%s).w)", a.c_str(), b.c_str(), a.c_str(), b.c_str()); break;
            case 29: vector = Format("max(%s, %s)", a.c_str(), b.c_str());
                     Line(Format("a0 = clamp((int)floor((%s).w + 0.5), -256, 255);", a.c_str())); break;
            default:
                out.problem = Format("vector opcode %u", vectorOpcode);
                vector = "float4(0.0, 0.0, 0.0, 0.0)";
                break;
            }
            (void)vectorSetsPredicate;

            if (kill && pixel)
            {
                out.usesKill = true;
                Line(Format("if (any(%s)) discard;", vector.c_str()));
            }
            // An instruction's two halves run together: both read the
            // registers as they were before it, and only then write. A
            // scalar that reads the very register the vector writes must
            // still see the old value, so the vector's result is kept aside
            // and written below, once the scalar has read what it needs.
            // (The third light of the level shaders is written this way:
            // the scalar wants the exponent times the logarithm the last
            // instruction left in the register, and with the write first it
            // took the dot product the vector had just put there instead.
            // Two to the tenth of that: every lit wall came out white.)
            // COD3_OLDALUPAIR=1 writes it first again, to see what the
            // rule is worth.
            static const bool writeFirst = getenv("COD3_OLDALUPAIR") != nullptr;
            std::string vectorDestination;
            if (vectorMask != 0)
            {
                if (vectorClamp) vector = "saturate(" + vector + ")";
                vectorDestination = Destination(vectorDest, exported);
                if (writeFirst)
                    Line(Format("%s.%s = (%s).%s;", vectorDestination.c_str(), Mask(vectorMask).c_str(),
                        vector.c_str(), Mask(vectorMask).c_str()));
                else Line(Format("float4 vectorValue = (%s);", vector.c_str()));
            }

            // The scalar operation, on the third operand's two components.
            std::string scalar;
            bool scalarKill = false;
            switch (scalarOpcode)
            {
            case 0:  scalar = Format("(%s + %s)", sa.c_str(), sb.c_str()); break;
            case 1:  scalar = Format("(%s + ps)", s1.c_str()); break;
            case 2:  scalar = Format("mulL1(%s, %s)", sa.c_str(), sb.c_str()); break;
            case 3:  scalar = Format("mulL1(%s, ps)", s1.c_str()); break;
            case 4:  scalar = Format("((ps == -3.402823466e38 || isnan(ps) || isnan(%s) || %s <= 0.0) ? -3.402823466e38 : %s * ps)",
                                     sb.c_str(), sb.c_str(), sa.c_str()); break;
            case 5:  scalar = Format("max(%s, %s)", sa.c_str(), sb.c_str()); break;
            case 6:  scalar = Format("min(%s, %s)", sa.c_str(), sb.c_str()); break;
            case 7:  scalar = Format("(%s == 0.0 ? 1.0 : 0.0)", s1.c_str()); break;
            case 8:  scalar = Format("(%s > 0.0 ? 1.0 : 0.0)", s1.c_str()); break;
            case 9:  scalar = Format("(%s >= 0.0 ? 1.0 : 0.0)", s1.c_str()); break;
            case 10: scalar = Format("(%s != 0.0 ? 1.0 : 0.0)", s1.c_str()); break;
            case 11: scalar = Format("frac(%s)", s1.c_str()); break;
            case 12: scalar = Format("trunc(%s)", s1.c_str()); break;
            case 13: scalar = Format("floor(%s)", s1.c_str()); break;
            case 14: scalar = Format("exp2(%s)", s1.c_str()); break;
            case 15: scalar = Format("(%s > 0.0 ? log2(%s) : -3.402823466e38)", s1.c_str(), s1.c_str()); break;
            case 16: scalar = Format("(%s > 0.0 ? log2(%s) : -3.402823466e38)", s1.c_str(), s1.c_str()); break;
            case 17: scalar = Format("(%s == 0.0 ? 3.402823466e38 : 1.0 / %s)", s1.c_str(), s1.c_str()); break;
            case 18: scalar = Format("(%s == 0.0 ? 0.0 : 1.0 / %s)", s1.c_str(), s1.c_str()); break;
            case 19: scalar = Format("(1.0 / %s)", s1.c_str()); break;
            case 20: scalar = Format("(%s <= 0.0 ? 3.402823466e38 : rsqrt(%s))", s1.c_str(), s1.c_str()); break;
            case 21: scalar = Format("(%s <= 0.0 ? 0.0 : rsqrt(%s))", s1.c_str(), s1.c_str()); break;
            case 22: scalar = Format("rsqrt(%s)", s1.c_str()); break;
            case 23: scalar = Format("max(%s, %s)", sa.c_str(), sb.c_str());
                     Line(Format("a0 = clamp((int)floor(%s + 0.5), -256, 255);", sa.c_str())); break;
            case 24: scalar = Format("max(%s, %s)", sa.c_str(), sb.c_str());
                     Line(Format("a0 = clamp((int)floor(%s), -256, 255);", sa.c_str())); break;
            case 25: scalar = Format("(%s - %s)", sa.c_str(), sb.c_str()); break;
            case 26: scalar = Format("(%s - ps)", s1.c_str()); break;
            case 27: Line(Format("p0 = (%s == 0.0);", s1.c_str())); scalar = "(p0 ? 0.0 : 1.0)"; break;
            case 28: Line(Format("p0 = (%s != 0.0);", s1.c_str())); scalar = "(p0 ? 0.0 : 1.0)"; break;
            case 29: Line(Format("p0 = (%s > 0.0);", s1.c_str())); scalar = "(p0 ? 0.0 : 1.0)"; break;
            case 30: Line(Format("p0 = (%s >= 0.0);", s1.c_str())); scalar = "(p0 ? 0.0 : 1.0)"; break;
            case 31: Line(Format("p0 = (%s == 1.0);", s1.c_str())); scalar = Format("(p0 ? 0.0 : blend1(%s == 0.0, 1.0, %s))", s1.c_str(), s1.c_str()); break;
            case 32: Line(Format("p0 = (%s - 1.0 <= 0.0);", s1.c_str())); scalar = Format("(p0 ? 0.0 : %s - 1.0)", s1.c_str()); break;
            case 33: Line("p0 = false;"); scalar = "3.402823466e38"; break;
            case 34: Line(Format("p0 = (%s == 0.0);", s1.c_str())); scalar = s1; break;
            case 35: scalar = Format("(%s == 0.0 ? 1.0 : 0.0)", s1.c_str()); scalarKill = true; break;
            case 36: scalar = Format("(%s > 0.0 ? 1.0 : 0.0)", s1.c_str()); scalarKill = true; break;
            case 37: scalar = Format("(%s >= 0.0 ? 1.0 : 0.0)", s1.c_str()); scalarKill = true; break;
            case 38: scalar = Format("(%s != 0.0 ? 1.0 : 0.0)", s1.c_str()); scalarKill = true; break;
            case 39: scalar = Format("(%s == 1.0 ? 1.0 : 0.0)", s1.c_str()); scalarKill = true; break;
            case 40: scalar = Format("sqrt(%s)", s1.c_str()); break;
            case 42: case 43: case 44: case 45: case 46: case 47:
            {
                // A constant and a temporary, the temporary's number spread
                // over the opcode's low bit, the third select and the middle
                // of the third swizzle.
                const uint32_t temporary = (scalarOpcode & 1) | (uint32_t(temp3) << 1) | (swizzle3 & 0x3C);
                std::string constant = Constant(reg3, rel3);
                constant += "."; constant += Components[(3 + ((swizzle3 >> 6) & 3)) & 3];
                std::string other = Format("r[%u].", temporary & 0x1F); other += Components[swizzle3 & 3];
                if (absoluteConstants) { constant = "abs(" + constant + ")"; other = "abs(" + other + ")"; }
                if (negate3) { constant = "(-" + constant + ")"; other = "(-" + other + ")"; }
                const char* op = scalarOpcode < 44 ? "*" : scalarOpcode < 46 ? "+" : "-";
                if (scalarOpcode < 44) scalar = Format("mulL1(%s, %s)", constant.c_str(), other.c_str());
                else scalar = Format("(%s %s %s)", constant.c_str(), op, other.c_str());
                break;
            }
            case 48: scalar = Format("sin(%s)", s1.c_str()); break;
            case 49: scalar = Format("cos(%s)", s1.c_str()); break;
            case 50: scalar = "ps"; break;
            default:
                out.problem = Format("scalar opcode %u", scalarOpcode);
                scalar = "0.0";
                break;
            }
            if (scalarKill && pixel)
            {
                out.usesKill = true;
                Line(Format("if (%s != 0.0) discard;", scalar.c_str()));
            }
            if (scalarClamp) scalar = "saturate(" + scalar + ")";
            Line(Format("ps = %s;", scalar.c_str()));
            if (vectorMask != 0 && !writeFirst)
                Line(Format("%s.%s = vectorValue.%s;", vectorDestination.c_str(),
                    Mask(vectorMask).c_str(), Mask(vectorMask).c_str()));
            if (scalarMask != 0)
            {
                const std::string destination = Destination(exported ? vectorDest : scalarDest, exported);
                Line(Format("%s.%s = ps.%s;", destination.c_str(), Mask(scalarMask).c_str(),
                    std::string(Mask(scalarMask).size(), 'x').c_str()));
            }

            if (isPredicated) { indent--; Line("}"); }
            indent--;
            Line("}");
            if (++aluCount == ShowAt() && showWhat[0] == 'r')
                Line(Format("shown = r[%u];", unsigned(strtoul(showWhat.c_str() + 1, nullptr, 10)) & 31u));
        }

        // --- fetches -----------------------------------------------------------

        // The destination swizzle both fetches share: three bits a component,
        // a source component, nought, one, or leave the register alone.
        void FetchWrite(uint32_t destReg, uint32_t swizzle, const std::string& fetched)
        {
            for (uint32_t i = 0; i < 4; i++)
            {
                const uint32_t selector = (swizzle >> (i * 3)) & 7;
                std::string value;
                if (selector < 4) value = fetched + "." + Components[selector];
                else if (selector == 4) value = "0.0";
                else if (selector == 5) value = "1.0";
                else if (selector == 6) value = "0.0";
                else continue;
                Line(Format("r[%u].%c = %s;", destReg & 0x1F, Components[i], value.c_str()));
            }
        }

        void VertexFetchInstruction(uint32_t d0, uint32_t d1, uint32_t d2)
        {
            const uint32_t srcReg = (d0 >> 5) & 0x3F;
            const uint32_t dstReg = (d0 >> 12) & 0x3F;
            const uint32_t slot = ((d0 >> 20) & 0x1F) * 3 + ((d0 >> 25) & 3);
            const uint32_t srcSwizzle = (d0 >> 30) & 3;
            const uint32_t dstSwizzle = d1 & 0xFFF;
            const bool isSigned = ((d1 >> 12) & 1) != 0;
            const bool whole = ((d1 >> 13) & 1) != 0;
            const bool rounded = ((d1 >> 15) & 1) != 0;
            const uint32_t format = (d1 >> 16) & 0x3F;
            int exponent = int((d1 >> 24) & 0x3F);
            if (exponent >= 32) exponent -= 64;
            const bool mini = ((d1 >> 30) & 1) != 0;
            const bool isPredicated = ((d1 >> 31) & 1) != 0;
            const uint32_t stride = d2 & 0xFF;
            const uint32_t offset = (d2 >> 8) & 0x7FFFFF;
            const bool predicateCondition = ((d2 >> 31) & 1) != 0;

            vertexSlots.insert(slot);
            if (!mini && stride != 0 && vertexStrides.count(slot) == 0) vertexStrides[slot] = stride;
            Line("{");
            indent++;
            if (isPredicated) { Line(Format("if (p0 == %s) {", predicateCondition ? "true" : "false")); indent++; }

            // The index: the source register's component, rounded as asked.
            if (!mini)
                Line(Format("fetchIndex = (uint)(%s(r[%u].%c));", rounded ? "round" : "trunc", srcReg & 0x1F, Components[srcSwizzle]));
            if (!mini) Line(Format("fetchStride = %u;", stride));
            Line(Format("fetchAddress = (fetchIndex * fetchStride + %u) * 4;", offset));

            std::string bufferName = Format("vb%u", slot);
            std::string decoded;
            const std::string scale = exponent != 0 ? Format(" * %.9g", double(exponent > 0 ? (1 << exponent) : 1.0 / (1 << -exponent))) : "";
            auto Normalised = [&](const std::string& value, int bits) -> std::string {
                if (isSigned)
                {
                    std::string wide = Format("((int)((%s) << %d) >> %d)", value.c_str(), 32 - bits, 32 - bits);
                    if (whole) return "(float)" + wide;
                    return Format("max((float)%s / %.1f, -1.0)", wide.c_str(), double((1u << (bits - 1)) - 1));
                }
                if (whole) return "(float)(" + value + ")";
                return Format("((float)(%s) / %.1f)", value.c_str(), double((1ull << bits) - 1));
            };
            switch (format)
            {
            case 36: decoded = Format("float4(asfloat(bswap(%s.Load(fetchAddress))), 0.0, 0.0, 1.0)", bufferName.c_str()); break;
            case 37: decoded = Format("float4(asfloat(bswap(%s.Load2(fetchAddress))), 0.0, 1.0)", bufferName.c_str()); break;
            case 57: decoded = Format("float4(asfloat(bswap(%s.Load3(fetchAddress))), 1.0)", bufferName.c_str()); break;
            case 38: decoded = Format("asfloat(bswap(%s.Load4(fetchAddress)))", bufferName.c_str()); break;
            case 6:
                Line(Format("fetchWord = bswap(%s.Load(fetchAddress));", bufferName.c_str()));
                decoded = Format("float4(%s, %s, %s, %s)",
                    Normalised("fetchWord & 0xFF", 8).c_str(), Normalised("(fetchWord >> 8) & 0xFF", 8).c_str(),
                    Normalised("(fetchWord >> 16) & 0xFF", 8).c_str(), Normalised("fetchWord >> 24", 8).c_str());
                break;
            case 7:
                Line(Format("fetchWord = bswap(%s.Load(fetchAddress));", bufferName.c_str()));
                decoded = Format("float4(%s, %s, %s, %s)",
                    Normalised("fetchWord & 0x3FF", 10).c_str(), Normalised("(fetchWord >> 10) & 0x3FF", 10).c_str(),
                    Normalised("(fetchWord >> 20) & 0x3FF", 10).c_str(), Normalised("fetchWord >> 30", 2).c_str());
                break;
            case 25:
                Line(Format("fetchWord = bswap(%s.Load(fetchAddress));", bufferName.c_str()));
                decoded = Format("float4(%s, %s, 0.0, 1.0)",
                    Normalised("fetchWord & 0xFFFF", 16).c_str(), Normalised("fetchWord >> 16", 16).c_str());
                break;
            case 26:
                Line(Format("fetchWords = bswap(%s.Load2(fetchAddress));", bufferName.c_str()));
                decoded = Format("float4(%s, %s, %s, %s)",
                    Normalised("fetchWords.x & 0xFFFF", 16).c_str(), Normalised("fetchWords.x >> 16", 16).c_str(),
                    Normalised("fetchWords.y & 0xFFFF", 16).c_str(), Normalised("fetchWords.y >> 16", 16).c_str());
                break;
            case 31:
                Line(Format("fetchWord = bswap(%s.Load(fetchAddress));", bufferName.c_str()));
                decoded = "float4(f16tof32(fetchWord & 0xFFFF), f16tof32(fetchWord >> 16), 0.0, 1.0)";
                break;
            case 32:
                Line(Format("fetchWords = bswap(%s.Load2(fetchAddress));", bufferName.c_str()));
                decoded = "float4(f16tof32(fetchWords.x & 0xFFFF), f16tof32(fetchWords.x >> 16), "
                          "f16tof32(fetchWords.y & 0xFFFF), f16tof32(fetchWords.y >> 16))";
                break;
            case 33:
                Line(Format("fetchWord = bswap(%s.Load(fetchAddress));", bufferName.c_str()));
                decoded = Format("float4(%s, 0.0, 0.0, 1.0)", Normalised("fetchWord", 32).c_str());
                break;
            case 34:
                Line(Format("fetchWords = bswap(%s.Load2(fetchAddress));", bufferName.c_str()));
                decoded = Format("float4(%s, %s, 0.0, 1.0)", Normalised("fetchWords.x", 32).c_str(), Normalised("fetchWords.y", 32).c_str());
                break;
            case 35:
                Line(Format("fetchWords4 = bswap(%s.Load4(fetchAddress));", bufferName.c_str()));
                decoded = Format("float4(%s, %s, %s, %s)", Normalised("fetchWords4.x", 32).c_str(), Normalised("fetchWords4.y", 32).c_str(),
                    Normalised("fetchWords4.z", 32).c_str(), Normalised("fetchWords4.w", 32).c_str());
                break;
            default:
                out.problem = Format("vertex format %u", format);
                decoded = "float4(0.0, 0.0, 0.0, 1.0)";
                break;
            }
            Line(Format("fetched = %s%s;", decoded.c_str(), scale.c_str()));
            FetchWrite(dstReg, dstSwizzle, "fetched");

            if (isPredicated) { indent--; Line("}"); }
            indent--;
            Line("}");
        }

        void TextureFetchInstruction(uint32_t d0, uint32_t d1, uint32_t d2)
        {
            const uint32_t opcode = d0 & 0x1F;
            const uint32_t srcReg = (d0 >> 5) & 0x3F;
            const uint32_t dstReg = (d0 >> 12) & 0x3F;
            const uint32_t slot = (d0 >> 20) & 0x1F;
            const bool unnormalised = ((d0 >> 25) & 1) != 0;
            const uint32_t srcSwizzle = (d0 >> 26) & 0x3F;
            const uint32_t dstSwizzle = d1 & 0xFFF;
            const bool useRegisterLod = ((d1 >> 29) & 1) != 0;
            const bool isPredicated = ((d1 >> 31) & 1) != 0;
            int lodBias = int((d2 >> 2) & 0x7F);
            if (lodBias >= 64) lodBias -= 128;
            // The offsets the fetch adds to its coordinates, signed five bit
            // fields in halves of a texel. The title's shadows are four
            // fetches of one map at the corners of a texel, and without
            // these they are four fetches of the same texel: the filtering
            // collapses and a surface comes out wholly lit or wholly dark.
            auto offsetOf = [](uint32_t bits) { int value = int(bits & 0x1F); return value >= 16 ? value - 32 : value; };
            // COD3_NOFETCHOFFSET=1 leaves them out again, to see what they do.
            static const bool noOffsets = getenv("COD3_NOFETCHOFFSET") != nullptr;
            const int offsetX = noOffsets ? 0 : offsetOf(d2 >> 16), offsetY = noOffsets ? 0 : offsetOf(d2 >> 21), offsetZ = offsetOf(d2 >> 26);
            const uint32_t dimension = (d2 >> 14) & 3;
            const bool predicateCondition = ((d2 >> 31) & 1) != 0;

            if (opcode != 1)
            {
                // The lod and gradient instructions: the sample they lead to
                // takes its own lod, so these are noted and passed over; the
                // gradients and weights are what the pixel program can work
                // out itself.
                const std::string coordinate = Format("r[%u].%c%c", srcReg & 0x1F,
                    Components[srcSwizzle & 3], Components[(srcSwizzle >> 2) & 3]);
                if (opcode == 24) Line(Format("textureLod = r[%u].x;", srcReg & 0x1F));
                else if (opcode == 17) Line(Format("fetched = float4(0.0, 0.0, 0.0, 0.0);", dstReg & 0x1F));
                else if (opcode == 18)
                {
                    // getGradients: (d/dx, d/dy) of the first component, then of
                    // the second. The characters' programs take a tangent frame
                    // from these, and with both x derivatives first the frame
                    // lost its y half: the uniforms went black in patches.
                    // COD3_OLDGRADIENTS=1 puts both d/dx first again, to compare.
                    static const bool oldGradients = getenv("COD3_OLDGRADIENTS") != nullptr;
                    if (pixel) Line(Format(oldGradients ? "fetched = float4(ddx(%s.x), ddx(%s.y), ddy(%s.x), ddy(%s.y));"
                                                        : "fetched = float4(ddx(%s.x), ddy(%s.x), ddx(%s.y), ddy(%s.y));",
                        coordinate.c_str(), coordinate.c_str(), coordinate.c_str(), coordinate.c_str()));
                    else Line("fetched = float4(0.0, 0.0, 0.0, 0.0);");
                }
                else if (opcode == 19)
                {
                    weightSlots.insert(slot);
                    Line(Format("fetched = float4(frac(%s * textureSize[%u].xy - 0.5), 0.0, 0.0);", coordinate.c_str(), slot));
                }
                else if (opcode != 25 && opcode != 26) { out.problem = Format("fetch opcode %u", opcode); return; }
                if (opcode == 17 || opcode == 18 || opcode == 19) FetchWrite(dstReg, dstSwizzle, "fetched");
                return;
            }

            textureSlots.insert(slot);
            uint32_t samplerIndex = 0;
            bool known = false;
            for (const TextureFetch& other : out.textureFetches)
                if (other.slot == slot) { known = true; samplerIndex = other.sampler; }
            if (!known)
            {
                TextureFetch fetch;
                fetch.slot = slot;
                fetch.dimension = dimension;
                fetch.sampler = uint32_t(out.textureFetches.size());
                samplerIndex = fetch.sampler;
                out.textureFetches.push_back(fetch);
            }

            Line("{");
            indent++;
            if (isPredicated) { Line(Format("if (p0 == %s) {", predicateCondition ? "true" : "false")); indent++; }

            std::string coordinate = Format("r[%u].%c%c%c", srcReg & 0x1F,
                Components[srcSwizzle & 3], Components[(srcSwizzle >> 2) & 3], Components[(srcSwizzle >> 4) & 3]);
            std::string sample;
            const std::string texture = Format("t%u", slot);
            const std::string sampler = Format("s%u", samplerIndex);
            const std::string size = Format("textureSize[%u]", slot);
            // A vertex program has no derivatives to pick a level by: it
            // samples the level it names, or the first.
            // A fetch that says volume reads a flat texture here: every one
            // the title issues names a flat texture in its fetch constant
            // (the particle sprites, 128 by 128 DXT), which is what the
            // hardware addresses by, and no volume texture has been seen.
            // The third coordinate is dropped, as the constant would drop it.
            std::string location;
            if (dimension == 3) location = Format("cubeDirection(%s)", coordinate.c_str());
            else location = unnormalised ? Format("((%s).xy * %s.zw)", coordinate.c_str(), size.c_str()) : Format("(%s).xy", coordinate.c_str());
            // The offsets are texels; the coordinates are the texture's own
            // measure, so they go through its size.
            if (dimension != 3 && (offsetX != 0 || offsetY != 0))
                location = Format("(%s + float2(%.4f, %.4f) * %s.zw)", location.c_str(), offsetX * 0.5, offsetY * 0.5, size.c_str());
            (void)offsetZ;
            if (useRegisterLod || !pixel)
                sample = Format("%s.SampleLevel(%s, %s, %s)", texture.c_str(), sampler.c_str(), location.c_str(),
                    useRegisterLod ? "textureLod" : "0.0");
            else if (lodBias != 0)
                sample = Format("%s.SampleBias(%s, %s, %.4f)", texture.c_str(), sampler.c_str(), location.c_str(), lodBias / 16.0);
            else
                sample = Format("%s.Sample(%s, %s)", texture.c_str(), sampler.c_str(), location.c_str());
            // The fetch constant's own component swizzle and sign treatment,
            // applied by a function the backend fills in per slot.
            Line(Format("fetched = textureAdjust(%u, %s);", slot, sample.c_str()));
            FetchWrite(dstReg, dstSwizzle, "fetched");

            if (isPredicated) { indent--; Line("}"); }
            indent--;
            Line("}");
        }

        // --- control flow ------------------------------------------------------

        // The instructions of one exec block.
        void ExecBody(uint64_t instruction)
        {
            const uint32_t address = uint32_t(instruction) & 0xFFF;
            const uint32_t count = (uint32_t(instruction) >> 12) & 7;
            const uint32_t sequence = (uint32_t(instruction) >> 16) & 0xFFF;
            for (uint32_t i = 0; i < count; i++)
            {
                const uint32_t slot = address + i;
                if (slot * 3 + 2 >= words.size()) { out.problem = "instruction past the end"; return; }
                const uint32_t d0 = words[slot * 3 + 0];
                const uint32_t d1 = words[slot * 3 + 1];
                const uint32_t d2 = words[slot * 3 + 2];
                const bool fetch = ((sequence >> (i * 2)) & 1) != 0;
                if (fetch)
                {
                    if ((d0 & 0x1F) == 0) VertexFetchInstruction(d0, d1, d2);
                    else TextureFetchInstruction(d0, d1, d2);
                }
                else Alu(d0, d1, d2);
            }
        }

        std::string BoolConstant(uint32_t index)
        {
            return Format("((bools[%u][%u] >> %u) & 1u)", index / 128, (index / 32) & 3, index & 31);
        }

        // The whole program as a loop over a switch on the program counter,
        // one case an instruction of the control flow block: an exec runs
        // its instructions and moves to the next case, a jump or a loop sets
        // the counter, and the end leaves the loop. Everything the sequencer
        // can do fits this shape, and the host's compiler unrolls the plain
        // ones.
        // The instructions from begin to end as straight code with ifs, or
        // false when a jump does not fit that shape. A conditional jump
        // forward is an if over what it skips; when the last instruction
        // it skips is an unconditional jump further on, that is the else.
        // The end of the program inside a block leaves the once loop.
        bool EmitStructured(const std::vector<uint64_t>& flow, uint32_t begin, uint32_t end, bool inLoop)
        {
            auto jumpOf = [&](uint32_t i, uint32_t& target, bool& unconditional) -> bool
            {
                const uint64_t instruction = flow[i];
                if ((uint32_t(instruction >> 44) & 0xF) != 11) return false;
                target = uint32_t(instruction) & 0x1FFF;
                unconditional = ((instruction >> 13) & 1) != 0;
                return true;
            };
            const std::string finish = inLoop ? "break;" : "";
            uint32_t i = begin;
            while (i < end)
            {
                const uint64_t instruction = flow[i];
                const uint32_t opcode = uint32_t(instruction >> 44) & 0xF;
                switch (opcode)
                {
                case 0: case 12: case 15:
                    i++;
                    break;
                case 1: ExecBody(instruction); i++; break;
                case 2: ExecBody(instruction); if (inLoop) Line(finish); i++; break;
                case 3: case 4:
                {
                    const uint32_t boolAddress = uint32_t(instruction >> 34) & 0xFF;
                    const bool condition = ((instruction >> 42) & 1) != 0;
                    Line(Format("if (%s == %uu) {", BoolConstant(boolAddress).c_str(), condition ? 1u : 0u));
                    indent++;
                    ExecBody(instruction);
                    indent--;
                    Line("}");
                    if (opcode == 4 && inLoop) Line(finish);
                    i++;
                    break;
                }
                case 5: case 6: case 13: case 14:
                {
                    // 13 and 14 are the "clean" predicated execs, which the
                    // predicate's own value does not change inside: the same
                    // here. They were taken for no-ops, and the blocks of
                    // the grass and the trees that sit in them never ran.
                    const bool condition = ((instruction >> 42) & 1) != 0;
                    Line(Format("if (p0 == %s) {", condition ? "true" : "false"));
                    indent++;
                    ExecBody(instruction);
                    indent--;
                    Line("}");
                    if ((opcode == 6 || opcode == 14) && inLoop) Line(finish);
                    i++;
                    break;
                }
                case 11:
                {
                    uint32_t target; bool unconditional;
                    jumpOf(i, target, unconditional);
                    if (target <= i || target > end) return false;
                    if (unconditional) { i = target; break; }   // what it skips is never run
                    const bool onPredicate = ((instruction >> 14) & 1) != 0;
                    const bool condition = ((instruction >> 42) & 1) != 0;
                    const uint32_t boolAddress = uint32_t(instruction >> 34) & 0xFF;
                    // The block runs when the jump is not taken.
                    if (onPredicate) Line(Format("if (p0 == %s) {", condition ? "false" : "true"));
                    else Line(Format("if (%s == %uu) {", BoolConstant(boolAddress).c_str(), condition ? 0u : 1u));
                    indent++;
                    uint32_t elseTarget = 0; bool elseUnconditional = false;
                    const uint32_t last = target - 1;
                    const bool ifElse = last > i && jumpOf(last, elseTarget, elseUnconditional)
                        && elseUnconditional && elseTarget > target && elseTarget <= end;
                    if (!EmitStructured(flow, i + 1, ifElse ? last : target, inLoop)) return false;
                    indent--;
                    if (ifElse)
                    {
                        Line("} else {");
                        indent++;
                        if (!EmitStructured(flow, target, elseTarget, inLoop)) return false;
                        indent--;
                    }
                    Line("}");
                    i = ifElse ? elseTarget : target;
                    break;
                }
                case 7:
                {
                    // A loop whose end is the loop end that names it, with
                    // nothing inside that ends the program, is a for: the
                    // count, the start and the step come from the loop
                    // constant, as the sequencer read them. Anything else
                    // is the loop over a switch.
                    const uint32_t loopEnd = uint32_t(instruction) & 0x1FFF;
                    const bool repeat = ((instruction >> 13) & 1) != 0;
                    const uint32_t loopId = uint32_t(instruction >> 16) & 0x1F;   // word 0, bits 16..20
                    if (loopEnd <= i || loopEnd >= end) return false;
                    const uint64_t last = flow[loopEnd];
                    if ((uint32_t(last >> 44) & 0xF) != 8 || (uint32_t(last) & 0x1FFF) != i) return false;
                    for (uint32_t j = i + 1; j < loopEnd; j++)
                    {
                        const uint32_t inside = uint32_t(flow[j] >> 44) & 0xF;
                        if (inside == 2 || inside == 4 || inside == 6 || inside == 14) return false;
                    }
                    const bool predicated = ((last >> 13) & 1) != 0;
                    const bool condition = ((last >> 42) & 1) != 0;
                    Line(Format("{ uint loopConstant%u = loops[%u][%u]; uint loopCount%u = loopConstant%u & 0xFFu;", i, loopId / 4, loopId & 3, i, i));
                    Line(Format("  int loopStep%u = (int)((loopConstant%u >> 16) & 0xFFu); if (loopStep%u >= 128) loopStep%u -= 256;", i, i, i, i));
                    Line(Format("  int savedAL%u = aL; aL = (int)((loopConstant%u >> 8) & 0xFFu);", i, i));
                    Line(Format("  if (loopCount%u != 0u || %s) {", i, repeat ? "true" : "false"));
                    Line(Format("  [loop] for (uint loopIndex%u = 0; loopIndex%u < 1024u; loopIndex%u++) {", i, i, i));
                    indent += 2;
                    if (!EmitStructured(flow, i + 1, loopEnd, inLoop)) return false;
                    Line(Format("loopCount%u--; aL += loopStep%u;", i, i));
                    if (predicated) Line(Format("if (loopCount%u == 0u || p0 == %s) break;", i, condition ? "true" : "false"));
                    else Line(Format("if (loopCount%u == 0u) break;", i));
                    indent -= 2;
                    Line("  } }");
                    Line(Format("  aL = savedAL%u; }", i));
                    i = loopEnd + 1;
                    break;
                }
                default:
                    return false;   // calls, a loop end on its own: the loop over a switch
                }
            }
            return true;
        }

        // What COD3_D3DSHOW asks of this program: "oN", "rN" or "rN@M",
        // and empty when the knob names another program's hash.
        std::string showWhat;
        int aluCount = 0;

        void ReadShow()
        {
            const char* show = getenv("COD3_D3DSHOW");
            if (show == nullptr || !pixel) return;
            if (const char* colon = strchr(show, ':'))
            {
                uint64_t hash = 1469598103934665603ull;
                for (uint32_t word : words)
                    for (int shift = 24; shift >= 0; shift -= 8) { hash ^= (word >> shift) & 0xFF; hash *= 1099511628211ull; }
                if (hash != strtoull(show, nullptr, 16)) return;
                showWhat = colon + 1;
            }
            else showWhat = show;
        }

        // The register the knob wants kept, and after which instruction, or
        // -1 when it wants the value the program ends with.
        int ShowAt() const
        {
            const size_t at = showWhat.find('@');
            return at == std::string::npos ? -1 : int(strtol(showWhat.c_str() + at + 1, nullptr, 10));
        }

        bool Run()
        {
            if (words.size() < 3) { out.problem = "no words"; return false; }
            ReadShow();

            uint32_t firstTarget = uint32_t(words.size() / 3);
            std::vector<uint64_t> flow;
            for (uint32_t slot = 0; slot * 3 + 2 < words.size() && slot < firstTarget; slot++)
            {
                const uint32_t d0 = words[slot * 3 + 0];
                const uint32_t d1 = words[slot * 3 + 1];
                const uint32_t d2 = words[slot * 3 + 2];
                const uint64_t a = uint64_t(d0) | (uint64_t(d1 & 0xFFFF) << 32);
                const uint64_t b = uint64_t(d1 >> 16) | (uint64_t(d2) << 16);
                for (uint64_t instruction : { a, b })
                {
                    flow.push_back(instruction);
                    const uint32_t opcode = uint32_t(instruction >> 44) & 0xF;
                    if ((opcode >= 1 && opcode <= 6) || opcode == 13 || opcode == 14)
                    {
                        const uint32_t address = uint32_t(instruction) & 0xFFF;
                        if (address < firstTarget) firstTarget = address;
                    }
                }
            }
            const uint32_t endCase = uint32_t(flow.size());

            // A program whose jumps nest as blocks, forward jumps over a
            // block and the if-else shape of a jump over a jump, is written
            // as the ifs it is; only one with loops, or jumps that cross,
            // gets the loop over a switch below, which the host's compiler
            // takes seconds over.
            {
                const std::string saved = body;
                const int savedIndent = indent;
                bool endsInside = false;
                for (size_t i = 0; i + 1 < flow.size(); i++)
                {
                    const uint32_t opcode = uint32_t(flow[i] >> 44) & 0xF;
                    if (opcode == 2 || opcode == 4 || opcode == 6 || opcode == 14) endsInside = true;
                }
                if (endsInside)
                {
                    Line("[loop] for (int once = 0; once < 1; once++) {");
                    indent++;
                }
                const bool ok = EmitStructured(flow, 0, uint32_t(flow.size()), endsInside);
                if (ok && endsInside)
                {
                    indent--;
                    Line("}");
                }
                if (ok) return true;
                body = saved;
                indent = savedIndent;
            }

            Line("[loop] for (int guard = 0; guard < 65536; guard++) {");
            indent++;
            Line("switch (programCounter) {");
            for (size_t i = 0; i < flow.size(); i++)
            {
                const uint64_t instruction = flow[i];
                const uint32_t opcode = uint32_t(instruction >> 44) & 0xF;
                const std::string next = Format("programCounter = %zu; break;", i + 1);
                const std::string finish = Format("programCounter = %u; break;", endCase);
                Line(Format("case %zu: {", i));
                indent++;
                switch (opcode)
                {
                case 0: case 12: case 15:
                    Line(next);
                    break;
                case 1: ExecBody(instruction); Line(next); break;
                case 2: ExecBody(instruction); Line(finish); break;
                case 3: case 4:
                {
                    const uint32_t boolAddress = uint32_t(instruction >> 34) & 0xFF;
                    const bool condition = ((instruction >> 42) & 1) != 0;
                    Line(Format("if (%s == %uu) {", BoolConstant(boolAddress).c_str(), condition ? 1u : 0u));
                    indent++;
                    ExecBody(instruction);
                    indent--;
                    Line("}");
                    Line(opcode == 4 ? finish : next);
                    break;
                }
                case 5: case 6: case 13: case 14:
                {
                    const bool condition = ((instruction >> 42) & 1) != 0;
                    Line(Format("if (p0 == %s) {", condition ? "true" : "false"));
                    indent++;
                    ExecBody(instruction);
                    indent--;
                    Line("}");
                    Line((opcode == 6 || opcode == 14) ? finish : next);
                    break;
                }
                case 7:   // loop start: the loop constant holds count, start and step
                {
                    const uint32_t loopEnd = uint32_t(instruction) & 0x1FFF;
                    const bool repeat = ((instruction >> 13) & 1) != 0;
                    const uint32_t loopId = uint32_t(instruction >> 16) & 0x1F;   // word 0, bits 16..20
                    Line(Format("{ uint constant = loops[%u][%u]; uint count = constant & 0xFF;", loopId / 4, loopId & 3));
                    Line("  if (depth < 4) { loopAddress[depth] = aL; loopCount[depth] = count; depth++; }");
                    Line("  aL = (int)((constant >> 8) & 0xFF);");
                    Line(Format("  if (count == 0 && !%s) { programCounter = %u; break; } }", repeat ? "true" : "false", loopEnd + 1));
                    Line(next);
                    break;
                }
                case 8:   // loop end: one more time round, or out
                {
                    const uint32_t loopStart = uint32_t(instruction) & 0x1FFF;
                    const bool predicated = ((instruction >> 13) & 1) != 0;   // predicate break
                    const bool condition = ((instruction >> 42) & 1) != 0;
                    const uint32_t loopId = uint32_t(instruction >> 16) & 0x1F;   // word 0, bits 16..20
                    Line(Format("{ uint constant = loops[%u][%u]; int step = (int)((constant >> 16) & 0xFF); if (step >= 128) step -= 256;",
                        loopId / 4, loopId & 3));
                    Line("  bool again = false;");
                    Line("  if (depth > 0) { loopCount[depth - 1]--; aL += step; again = loopCount[depth - 1] > 0; }");
                    if (predicated) Line(Format("  if (p0 == %s) again = false;", condition ? "true" : "false"));
                    Line(Format("  if (again) { programCounter = %u; break; }", loopStart + 1));
                    Line("  if (depth > 0) { depth--; aL = loopAddress[depth]; } }");
                    Line(next);
                    break;
                }
                case 11:
                {
                    const uint32_t target = uint32_t(instruction) & 0x1FFF;
                    const bool unconditional = ((instruction >> 13) & 1) != 0;
                    const bool onPredicate = ((instruction >> 14) & 1) != 0;
                    const bool condition = ((instruction >> 42) & 1) != 0;
                    const uint32_t boolAddress = uint32_t(instruction >> 34) & 0xFF;
                    if (unconditional) Line(Format("programCounter = %u; break;", target));
                    else if (onPredicate) Line(Format("if (p0 == %s) { programCounter = %u; break; }", condition ? "true" : "false", target));
                    else Line(Format("if (%s == %uu) { programCounter = %u; break; }", BoolConstant(boolAddress).c_str(), condition ? 1u : 0u, target));
                    Line(next);
                    break;
                }
                case 9: case 10:
                    out.problem = "subroutine call";
                    return false;
                default:
                    out.problem = Format("control flow opcode %u", opcode);
                    return false;
                }
                indent--;
                Line("}");
            }
            Line(Format("default: programCounter = %u; break;", endCase));
            Line("}");
            Line(Format("if (programCounter >= %u) break;", endCase));
            indent--;
            Line("}");
            return true;
        }
    };
}

bool XenosHlsl::Depth24()
{
    // The setting (console_depth), read once: the programs are translated
    // and cached for it. COD3_DEPTH24=0|1 overrides it.
    static const bool on = []() {
        if (const char* t = getenv("COD3_DEPTH24")) return t[0] != '0';
        return Settings::Get().consoleDepth;
    }();
    return on;
}

const char* const XenosHlsl::Depth20e4Source =
    "float Depth20e4(float z) {\n"
    "    uint f = asuint(z);\n"
    "    if (f < 0x38800000u) return floor(z * 17179869184.0) * (1.0 / 17179869184.0);\n"
    "    return asfloat(f & 0xFFFFFFF8u);\n"
    "}\n";

uint64_t XenosHlsl::Version()
{
    // A number that changes when the translation would, or when one of the
    // environment knobs that shape the HLSL is set: the disk cache keyed by
    // it then starts afresh rather than serving the other translation.
    std::string text = "xenos_hlsl 2026-10-06 bswap packed flat3d fetch offsets alu pairs read first pixel params gradients dx dy depth offset pixel centre loop ids index offset in program hud scale surface light fill";
    text += Depth24() ? " depth 20e4 centroid" : " depth 32";
    text += " shadow weights, coverage 2026-10-18, hdr clamp 2026-10-19";
    if (const char* lanes = getenv("COD3_SCALARLANES")) { text += " lanes="; text += lanes; }
    if (const char* show = getenv("COD3_D3DSHOW")) { text += " show="; text += show; }
    if (const char* sign = getenv("COD3_D3DSHOWSIGN")) { text += " sign="; text += sign; }
    if (getenv("COD3_NOFETCHOFFSET")) text += " no offsets";
    if (getenv("COD3_OLDALUPAIR")) text += " write first";
    if (getenv("COD3_OLDGRADIENTS")) text += " old gradients";
    if (getenv("COD3_NOPIXELGEN")) text += " no pixel params";
    if (getenv("COD3_NANSHOW")) text += " nan show";
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : text) { hash ^= c; hash *= 1099511628211ull; }
    return hash;
}

XenosHlsl::Translation XenosHlsl::Translate(const std::vector<uint32_t>& words, bool pixel, bool alphaTest)
{
    Translator translator(words, pixel);
    const bool ran = translator.Run();
    Translation& out = translator.out;
    if (!ran || !out.problem.empty()) { out.ok = false; return out; }
    translator.PackConstants();
    for (TextureFetch& fetch : out.textureFetches) fetch.weights = translator.weightSlots.count(fetch.slot) != 0 ? 1u : 0u;

    std::string hlsl;
    hlsl += "// Translated from the console's microcode.\n";
    hlsl += "cbuffer FloatConstants : register(b0) { float4 c[256]; };\n";
    hlsl += "cbuffer BoolConstants : register(b1) { uint4 bools[2]; uint4 loops[8]; };\n";
    hlsl += "cbuffer DrawConstants : register(b2) {\n"
            "    float4 viewportScale;    // x, y, z scales\n"
            "    float4 viewportOffset;   // x, y, z offsets\n"
            "    float4 targetSize;       // width, height in the title's pixels, and their reciprocals\n"
            "    uint4 flags;             // the viewport control word (bit 16: pixel centres at halves), alpha test function, alpha reference bits, unused\n"
            "    float4 textureSize[32];  // width, height, 1/width, 1/height\n"
            "    uint4 textureAdjustment[32];   // swizzle, sign modes, unused, unused\n"
            "    float4 pixelGen;         // guest pixels per host pixel x and y, the register, on or off\n"
            "    uint4 coverage;          // x: the target's samples when an alpha tested edge goes to coverage, else 0; y: the HDR targets; z: the index offset (signed); w: the HUD scale (float, 0 none)\n"
            "};\n";
    // The vertex buffers are bound as they lie in the console's memory, big
    // endian, and every word fetched is turned round here: a buffer upload
    // is then one copy, and the title's dynamic buffers cost what they are.
    hlsl += "uint bswap(uint v) { return (v >> 24) | ((v >> 8) & 0xFF00u) | ((v << 8) & 0xFF0000u) | (v << 24); }\n"
            "uint2 bswap(uint2 v) { return uint2(bswap(v.x), bswap(v.y)); }\n"
            "uint3 bswap(uint3 v) { return uint3(bswap(v.x), bswap(v.y), bswap(v.z)); }\n"
            "uint4 bswap(uint4 v) { return uint4(bswap(v.x), bswap(v.y), bswap(v.z), bswap(v.w)); }\n";
    hlsl += "float4 select4(bool4 v) { return float4(v.x ? 1.0 : 0.0, v.y ? 1.0 : 0.0, v.z ? 1.0 : 0.0, v.w ? 1.0 : 0.0); }\n";
    hlsl += "float max4(float4 v) { return max(max(v.x, v.y), max(v.z, v.w)); }\n";
    hlsl += "float4 blend4(float4 mask, float4 whenSet, float4 whenClear) { return whenSet * mask + whenClear * (1.0 - mask); }\n";
    hlsl += "float blend1(bool set, float whenSet, float whenClear) { float mask = set ? 1.0 : 0.0; return whenSet * mask + whenClear * (1.0 - mask); }\n";
    // The console multiplies the Direct3D 9 way: a zero times anything is
    // zero, infinity and NaN included. A title's normalisation of a zero
    // vector, rsq of zero times the components, comes out zero there and
    // would come out NaN here, and a NaN position is a triangle across the
    // whole screen on some drivers. (Xenia keeps the same rule.)
    hlsl += "float4 mulL(float4 a, float4 b) { float4 r = a * b; return (a == 0.0 || b == 0.0) ? float4(0.0, 0.0, 0.0, 0.0) : r; }\n";
    hlsl += "float mulL1(float a, float b) { float r = a * b; return (a == 0.0 || b == 0.0) ? 0.0 : r; }\n";
    // A sign chosen by a condition is flipped on the bits: a select between
    // x and -x is a negated select operand to the host's compiler, which it
    // then rejects in its own output.
    hlsl += "float flipIf(float v, bool flip) { return asfloat(asuint(v) ^ (flip ? 0x80000000u : 0u)); }\n"
            "float4 cubeMapX(float3 v) { return float4(flipIf(v.z, v.x >= 0.0), flipIf(v.y, true), v.x >= 0.0 ? 0.0 : 1.0, 2.0 * abs(v.x)); }\n"
            "float4 cubeMapY(float3 v) { return float4(v.x, flipIf(v.z, v.y < 0.0), v.y >= 0.0 ? 2.0 : 3.0, 2.0 * abs(v.y)); }\n"
            "float4 cubeMapZ(float3 v) { return float4(flipIf(v.x, v.z < 0.0), flipIf(v.y, true), v.z >= 0.0 ? 4.0 : 5.0, 2.0 * abs(v.z)); }\n"
            "float4 cubeMap(float4 a, float4 b) {\n"
            "    return (abs(a.x) >= abs(a.y) && abs(a.x) >= abs(a.z)) ? cubeMapX(a.xyz) : (abs(a.y) >= abs(a.z)) ? cubeMapY(a.xyz) : cubeMapZ(a.xyz); }\n";
    hlsl += "float3 cubeDirection(float3 c) {\n"
            "    // The face and the position within it back to a direction:\n"
            "    // the face's axis, and the two the position runs along.\n"
            "    uint face = (uint)c.z;\n"
            "    float3 major = face == 0 ? float3(1.0, 0.0, 0.0) : face == 1 ? float3(-1.0, 0.0, 0.0) : face == 2 ? float3(0.0, 1.0, 0.0)\n"
            "        : face == 3 ? float3(0.0, -1.0, 0.0) : face == 4 ? float3(0.0, 0.0, 1.0) : float3(0.0, 0.0, -1.0);\n"
            "    float3 sx = face == 0 ? float3(0.0, 0.0, -1.0) : face == 1 ? float3(0.0, 0.0, 1.0) : face == 5 ? float3(-1.0, 0.0, 0.0) : float3(1.0, 0.0, 0.0);\n"
            "    float3 sy = face == 2 ? float3(0.0, 0.0, 1.0) : face == 3 ? float3(0.0, 0.0, -1.0) : float3(0.0, -1.0, 0.0);\n"
            "    return major + sx * c.x + sy * c.y; }\n";

    // Textures and samplers, and the adjustment the fetch constant asks for.
    for (const TextureFetch& fetch : out.textureFetches)
    {
        const char* type = fetch.dimension == 3 ? "TextureCube" : "Texture2D";
        hlsl += Format("%s t%u : register(t%u);\nSamplerState s%u : register(s%u);\n",
            type, fetch.slot, fetch.slot, fetch.sampler, fetch.sampler);
    }
    // The fetch constant's component swizzle and sign treatment. Written
    // component by component: a loop here, unrolled inside a program with
    // its own loops, lost a variable in the host's compiler.
    // The helpers are single expressions without locals of their own: a
    // local assigned under an if, inlined into a program with a loop, is
    // "an unbound variable" to the host's compiler.
    hlsl += "float pickComponent(float4 v, uint s) { return s == 0 ? v.x : s == 1 ? v.y : s == 2 ? v.z : s == 3 ? v.w : s == 4 ? 0.0 : 1.0; }\n"
            "float adjustComponent(float4 v, uint s, uint mode) {\n"
            "    return mode == 2 ? pickComponent(v, s) * 2.0 - 1.0 : mode == 3 ? pow(abs(pickComponent(v, s)), 2.2) : pickComponent(v, s);\n"
            "}\n"
            "float4 textureAdjust(uint slot, float4 v) {\n"
            "    return float4(adjustComponent(v, textureAdjustment[slot].x & 7u, textureAdjustment[slot].y & 3u),\n"
            "        adjustComponent(v, (textureAdjustment[slot].x >> 3) & 7u, (textureAdjustment[slot].y >> 2) & 3u),\n"
            "        adjustComponent(v, (textureAdjustment[slot].x >> 6) & 7u, (textureAdjustment[slot].y >> 4) & 3u),\n"
            "        adjustComponent(v, (textureAdjustment[slot].x >> 9) & 7u, (textureAdjustment[slot].y >> 6) & 3u));\n"
            "}\n";

    // The depth as the console keeps it: its depth buffer is 24 bit float
    // (20 bits of mantissa, 4 of exponent, D24FS8), the host's 32 bit. The
    // title leans on the coarser one: two surfaces it draws nearly on top of
    // each other (a distant hillside and its lower detail, the Z pass's
    // programs and the colour pass's, which reach the same position by
    // other roads) come out the same on the console, and the greater or
    // equal test lets the later one through; on the host the last bits
    // decide, and wedges of the wrong surface showed and flickered as the
    // view moved (the hill behind the tanks in Saint-Lo's opening). So each
    // pixel's depth is cut to the console's precision: the 32 bit float's
    // last three mantissa bits dropped, and below 2^-14 (the 24 bit
    // format's denormals) steps of 2^-34. Cut down, never up, so the
    // output can be declared no greater than the rasterised depth and the
    // early depth test keeps working for the title's greater or equal
    // tests. COD3_DEPTH24=0 leaves the host's precision, to compare.
    // The draws that run no pixel program (the depth only mode) get the same
    // cut from a program of their own (render_d3d11.cpp).
    const bool depth24 = XenosHlsl::Depth24();
    if (pixel)
    {
        const bool cutDepth = depth24 && !out.writesDepth;
        if (cutDepth) hlsl += XenosHlsl::Depth20e4Source;
        // A depth declared no greater than the rasterised one needs the
        // position taken at the covered samples' centroid.
        hlsl += cutDepth ? "struct Input { noperspective centroid float4 position : SV_Position;" : "struct Input { float4 position : SV_Position;";
        for (uint32_t i = 0; i < 16; i++) hlsl += Format(" float4 o%u : TEXCOORD%u;", i, i);
        hlsl += " bool front : SV_IsFrontFace; };\n";
        hlsl += "struct Output { float4 c0 : SV_Target0; float4 c1 : SV_Target1; float4 c2 : SV_Target2; float4 c3 : SV_Target3;";
        if (out.writesDepth) hlsl += " float depth : SV_Depth;";
        else if (cutDepth) hlsl += " float depth : SV_DepthLessEqual;";
        if (alphaTest) hlsl += " uint coverage : SV_Coverage;";
        hlsl += " };\n";
        hlsl += "float4 hdrClamp(float4 v) { return float4(min(max(v.rgb, 0.0), 65000.0), saturate(v.a)); }\n";
        hlsl += "Output main(Input input) {\n";
        hlsl += "    float4 r[32];\n";
        for (uint32_t i = 0; i < 32; i++)
            hlsl += i < 16 ? Format("    r[%u] = input.o%u;\n", i, i) : Format("    r[%u] = float4(0.0, 0.0, 0.0, 0.0);\n", i);
        // The pixel parameters, when the draw asks for them: x and y the
        // pixel's position in the title's pixels (whole host pixels scaled
        // back, so a doubled resolution samples finer), back facing in x's
        // sign bit; z and w the point sprite's coordinates, nought here.
        // COD3_NOPIXELGEN=1 leaves them out again, for a comparison.
        static const bool noPixelGen = getenv("COD3_NOPIXELGEN") != nullptr;
        if (!noPixelGen)
        hlsl += "    if (pixelGen.w != 0.0) {\n"
                "        float2 at = floor(input.position.xy) * pixelGen.xy;\n"
                "        float4 generated = float4(asfloat(asuint(at.x) | (input.front ? 0u : 0x80000000u)), at.y, 0.0, 0.0);\n"
                "        uint slot = uint(pixelGen.z);\n"
                "        [unroll] for (uint i = 0; i < 32; i++) if (i == slot) r[i] = generated;\n"
                "    }\n";
        hlsl += "    float4 oC0 = float4(0.0, 0.0, 0.0, 0.0), oC1 = oC0, oC2 = oC0, oC3 = oC0, oDepth4 = oC0, oUnused = oC0;\n";
    }
    else
    {
        for (uint32_t slot : translator.vertexSlots)
        {
            hlsl += Format("ByteAddressBuffer vb%u : register(t%u);\n", slot, 32 + slot);
            VertexFetch fetch; fetch.slot = slot;
            if (translator.vertexStrides.count(slot)) fetch.stride = translator.vertexStrides[slot];
            out.vertexFetches.push_back(fetch);
        }
        hlsl += "struct Output { float4 position : SV_Position;";
        for (uint32_t i = 0; i < 16; i++) hlsl += Format(" float4 o%u : TEXCOORD%u;", i, i);
        hlsl += " };\n";
        hlsl += "Output main(uint vertexId : SV_VertexID) {\n";
        hlsl += "    float4 r[32];\n";
        // The index offset (VGT_INDX_OFFSET) added here (render_commands.cpp,
        // coverage.z), the draw's base vertex left at nought.
        hlsl += "    r[0] = float4((float)((int)vertexId + asint(coverage.z)), 0.0, 0.0, 0.0);\n";
        for (uint32_t i = 1; i < 32; i++) hlsl += Format("    r[%u] = float4(0.0, 0.0, 0.0, 0.0);\n", i);
        hlsl += "    float4 oPos = float4(0.0, 0.0, 0.0, 1.0), oPointSize = oPos, oUnused = oPos;\n";
        for (uint32_t i = 0; i < 16; i++) hlsl += Format("    float4 o%u = float4(0.0, 0.0, 0.0, 0.0);\n", i);
        hlsl += "    uint fetchIndex = 0, fetchStride = 0, fetchAddress = 0, fetchWord = 0; uint2 fetchWords = uint2(0, 0); uint4 fetchWords4 = uint4(0, 0, 0, 0);\n";
    }
    if (translator.ShowAt() >= 0) hlsl += "    float4 shown = float4(0.0, 0.0, 0.0, 0.0);\n";
    hlsl += "    float4 fetched = float4(0.0, 0.0, 0.0, 0.0); float ps = 0.0; float textureLod = 0.0; int a0 = 0; bool p0 = false;\n";
    hlsl += "    int programCounter = 0; int aL = 0; uint depth = 0; uint loopCount[4]; int loopAddress[4]; loopCount[0] = 0; loopCount[1] = 0; loopCount[2] = 0; loopCount[3] = 0; loopAddress[0] = 0; loopAddress[1] = 0; loopAddress[2] = 0; loopAddress[3] = 0;\n";
    hlsl += translator.body;

    if (pixel)
    {
        out.colourTargets = 0;
        for (uint32_t e : translator.exportsUsed) if (e < 4) out.colourTargets |= 1u << e;
        if (out.colourTargets == 0) out.colourTargets = 1;
        hlsl += "    Output output;\n";
        // The alpha test, from the draw's constants: never, less, equal,
        // less or equal, greater, not equal, greater or equal, always.
        // With MSAA, an alpha tested edge (the foliage, the fences: alpha
        // greater than the reference) is spread over the pixel's samples by
        // how far past the reference its alpha is, over the alpha's change
        // across one pixel: alpha to coverage, sharpened, so the cut-outs
        // are antialiased like geometry rather than one sample's yes or no.
        // The colour and its alpha are left as the title computed them.
        if (alphaTest)
        hlsl += "    uint coverageMask = 0xFFFFFFFFu;\n"
                "    { uint fn = flags.y; float ref = asfloat(flags.z); float a = oC0.w; bool passes = true;\n"
                "      float across = max(fwidth(a), 1.0 / 255.0);\n"
                "      if (fn == 0) passes = false; else if (fn == 1) passes = a < ref; else if (fn == 2) passes = a == ref;\n"
                "      else if (fn == 3) passes = a <= ref; else if (fn == 4) passes = a > ref; else if (fn == 5) passes = a != ref;\n"
                "      else if (fn == 6) passes = a >= ref;\n"
                "      if (coverage.x > 1u && (fn == 4u || fn == 6u)) {\n"
                "          float covered = saturate((a - ref) / across + 0.5);\n"
                "          uint samples = min(coverage.x, 16u);\n"
                "          uint count = (uint)round(covered * samples);\n"
                "          passes = count > 0u;\n"
                "          coverageMask = count >= 32u ? 0xFFFFFFFFu : ((1u << count) - 1u);\n"
                "      }\n"
                "      if (!passes) discard; }\n";
        // COD3_D3DSHOW=oN: every pixel program puts out its Nth interpolator
        // instead of its colour, for seeing what the vertex program handed
        // over. The source changes, so the cache keeps the real programs.
        // COD3_D3DSHOW=rN puts out a register instead, and rN@M the
        // register as it stood after the Mth instruction, for walking a
        // Surface light (F11, render_post.cpp): bits 24 to 30 of the flags,
        // set by render_commands.cpp only on the world's solid surfaces (a
        // depth written, no blending), brighten what they put out the way
        // more light on them would: the brightness through a curve that
        // lifts the dark most and keeps white white, the colour kept. The
        // sky, smoke, flashes and the HUD are left as they are.
        hlsl += "    { uint level = (flags.x >> 24) & 0x7Fu; if (level != 0u) { float k = float(level) / 50.0;\n"
                "        float l = dot(saturate(oC0.rgb), float3(0.2126, 0.7152, 0.0722)); float lit = l * (1.0 + k) / (1.0 + k * l);\n"
                "        oC0.rgb = saturate(oC0.rgb * (lit / max(l, 1e-4))); } }\n";
        // program and seeing where a value goes wrong. Either form takes a
        // program's hash before a colon (COD3_D3DSHOW=1234...:r11@70) so
        // only that one program is changed and the rest of the picture
        // stays as it was.
        const char* const what = translator.showWhat.empty() ? nullptr : translator.showWhat.c_str();
        // COD3_D3DSHOWSIGN=1: x alone with its sign, red above nought and
        // green below, blue where it is not a number.
        // COD3_D3DSHOWSIGN=y (or z, w) takes that lane instead of x.
        static const char* const showSign = getenv("COD3_D3DSHOWSIGN");
        const char lane = showSign != nullptr && (showSign[0] == 'y' || showSign[0] == 'z' || showSign[0] == 'w') ? showSign[0] : 'x';
        if (what != nullptr && translator.ShowAt() >= 0 && showSign != nullptr)
            hlsl += Format("    oC0 = float4(saturate(shown.%c), saturate(-shown.%c), isnan(shown.%c) ? 1.0 : 0.0, 1.0);\n", lane, lane, lane);
        else if (what != nullptr && translator.ShowAt() >= 0) hlsl += "    oC0 = float4(abs(shown.xyz), 1.0);\n";
        else if (what != nullptr && what[0] == 'o') hlsl += Format("    oC0 = float4(abs(input.%s.xyz), 1.0);\n", what);
        else if (what != nullptr && what[0] == 'r') hlsl += Format("    oC0 = float4(abs(r[%u].xyz), 1.0);\n", unsigned(strtoul(what + 1, nullptr, 10)) & 31u);
        // COD3_NANSHOW=1: a colour that is not a number, or infinite, put
        // out as magenta, for finding where one comes from.
        static const bool nanShow = getenv("COD3_NANSHOW") != nullptr;
        if (nanShow) hlsl += "    if (any(isnan(oC0)) || any(isinf(oC0))) oC0 = float4(1.0, 0.0, 1.0, 1.0);\n";
        // The HDR mode's float targets (render_resources.cpp) keep what an 8
        // bit one would have clipped. Past one is the point; below nought,
        // a not-a-number or past the half float's range is not, and the
        // alpha stays the 8 bit target's, from nought to one, for the
        // blending that reads it back. The draw says which targets
        // (coverage.y, a bit a slot); nothing changes without the mode.
        // coverage.w: the HDR mode's HUD scale for the first target, a float
        // (nought for none: render_commands.cpp, after the scene pass).
        hlsl += "    float hudScale = coverage.w != 0u ? asfloat(coverage.w) : 1.0;\n";
        hlsl += "    output.c0 = (coverage.y & 1u) != 0u ? hdrClamp(float4(oC0.rgb * hudScale, oC0.a)) : oC0; output.c1 = (coverage.y & 2u) != 0u ? hdrClamp(oC1) : oC1;\n"
                "    output.c2 = (coverage.y & 4u) != 0u ? hdrClamp(oC2) : oC2; output.c3 = (coverage.y & 8u) != 0u ? hdrClamp(oC3) : oC3;\n";
        if (alphaTest) hlsl += "    output.coverage = coverageMask;\n";
        if (out.writesDepth) hlsl += "    output.depth = oDepth4.x;\n";
        else if (depth24) hlsl += "    output.depth = Depth20e4(input.position.z);\n";
        hlsl += "    return output;\n}\n";
    }
    else
    {
        // The viewport, as PA_CL_VTE_CNTL says to apply it, folded into clip
        // space so the host's viewport is the whole target: what the console
        // would have made pixel (px, py) becomes the clip position with the
        // same w, so the interpolation stays perspective correct. The console
        // samples a pixel at its whole coordinates (Direct3D 9's centres,
        // PA_SU_VTX_CNTL bit 0 clear; flags bit 16 says otherwise), the host
        // half a pixel further on: the geometry moves by that half pixel, so
        // each host pixel sees what the console's did there. (Without it a
        // pass that samples another at exact texels, like the half size
        // smoke's depth-aware composite, picks its texels wrong.)
        hlsl += "    Output output;\n"
                "    { uint vte = flags.x; float4 p = oPos;\n"
                "      float ww = (vte & 0x100u) ? 1.0 : p.w;\n"
                "      float xs = (vte & 0x1u) ? viewportScale.x : 1.0; float xo = (vte & 0x2u) ? viewportOffset.x : 0.0;\n"
                "      float ys = (vte & 0x4u) ? viewportScale.y : 1.0; float yo = (vte & 0x8u) ? viewportOffset.y : 0.0;\n"
                "      float zs = (vte & 0x10u) ? viewportScale.z : 1.0; float zo = (vte & 0x20u) ? viewportOffset.z : 0.0;\n"
                "      float centre = (vte & 0x10000u) ? 0.0 : 0.5;\n"
                "      float W = targetSize.x, H = targetSize.y;\n"
                "      float cx = p.x * (2.0 * xs / W) + ww * (2.0 * (xo + centre) / W - 1.0);\n"
                "      float cy = -(p.y * (2.0 * ys / H) + ww * (2.0 * (yo + centre) / H - 1.0));\n"
                "      float cz = (vte & 0x200u) ? (p.z * zs + zo) * ww : (p.z * zs + zo * ww);\n"
                // The polygon offset's constant part, in depth units, as the
                // console adds it (viewportOffset.w; see render_commands.cpp).
                "      cz += viewportOffset.w * ww;\n"
                // Filling a screen wider (or narrower) than 16:9: a flat draw
                // (w of one: the HUD, a menu) squeezed into the 16:9 middle
                // (viewportScale.w across, or up and down when negative), so
                // it keeps its shape; one reaching the screen's edges (a fade,
                // a flash, the game's own full screen passes) is left to cover
                // it, unless bit 22 says to squeeze those too (no world drawn:
                // a film, a menu's picture). render_commands.cpp sets them.
                "      if (viewportScale.w != 0.0 && abs(ww - 1.0) < 1e-5) { bool all = (vte & 0x400000u) != 0; float s = abs(viewportScale.w);\n"
                "          if (viewportScale.w > 0.0) { if (all || abs(cx) < 0.999) cx *= s; } else { if (all || abs(cy) < 0.999) cy *= s; } }\n"
                "      output.position = float4(cx, cy, cz, ww); }\n";
        for (uint32_t i = 0; i < 16; i++) hlsl += Format("    output.o%u = o%u;\n", i, i);
        hlsl += "    return output;\n}\n";
        uint32_t interpolators = 0;
        for (uint32_t e : translator.exportsUsed) if (e < 16) interpolators = std::max(interpolators, e + 1);
        out.interpolators = interpolators;
    }

    out.hlsl = hlsl;
    out.ok = true;
    return out;
}
