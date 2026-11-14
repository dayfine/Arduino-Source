/*  Lock-On Bracket Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <memory>
#include <string>
#include <vector>
#include "CommonFramework/ImageTypes/BinaryImage.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Kernels/Waterfill/Kernels_Waterfill_Session.h"
#include "Tests/ImageUnitTest.h"
#include "PokemonLZA_LockOnBracketDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace Kernels::Waterfill;


namespace{

//  The sizes below are in pixels of a 720-row screen; `scale` (the
//  screen's rows / 720) converts them. Measured on console B's video of
//  Wild Zone 1 (2026-11-13), at 1280x720: arcs 48 to 74 rows tall, 3 or 4
//  pixels thick, the pair's middles 1.7 to 1.9 arc heights apart.
constexpr double REFERENCE_ROWS = 720;

//  An arc's pixel: each channel at least 140, and at least 30 above the
//  darkest pixel 2 to 6 pixels away on each side along its row. The arcs
//  are drawn see-through, so they take some of the grass or road's color;
//  a wide white area (sky, a wall, a window frame's edge) has no darker
//  pixel close on both sides.
constexpr int MIN_CHANNEL = 140;
constexpr int MIN_RISE = 30;
constexpr double NEAR_SIDE = 2;
constexpr double FAR_SIDE = 6;

//  The lowest the brackets' middle sits, as a share of the screen's height:
//  console B's arcs were at 0.35 to 0.61.
constexpr double MIN_MIDDLE_Y = 0.20;

struct Rgb{
    int r;
    int g;
    int b;
};

Rgb to_rgb(uint32_t pixel){
    return {(int)((pixel >> 16) & 0xff), (int)((pixel >> 8) & 0xff), (int)(pixel & 0xff)};
}

int brightness(const Rgb& c){
    return c.r + c.g + c.b;
}

//  Whether `pixel` rises above the darkest of `screen`'s pixels `near` to
//  `far` away from x on row y, toward `direction` (1 or -1).
bool rises_above_side(const ImageViewRGB32& screen, size_t x, size_t y, const Rgb& pixel, int direction, size_t near, size_t far){
    Rgb darkest{255, 255, 255};
    for (size_t k = near; k <= far; k++){
        const Rgb side = to_rgb(screen.pixel(direction > 0 ? x + k : x - k, y));
        if (brightness(side) < brightness(darkest)){
            darkest = side;
        }
    }
    return pixel.r - darkest.r >= MIN_RISE && pixel.g - darkest.g >= MIN_RISE && pixel.b - darkest.b >= MIN_RISE;
}

PackedBinaryMatrix arc_pixels(const ImageViewRGB32& screen, double scale){
    const size_t near = std::max<size_t>(1, (size_t)std::lround(NEAR_SIDE * scale));
    const size_t far = std::max(near + 1, (size_t)std::lround(FAR_SIDE * scale));
    PackedBinaryMatrix matrix(screen.width(), screen.height());
    matrix.set_zero();
    if (screen.width() <= 2 * far){
        return matrix;
    }
    for (size_t y = 0; y < screen.height(); y++){
        for (size_t x = far; x < screen.width() - far; x++){
            const Rgb pixel = to_rgb(screen.pixel(x, y));
            if (std::min({pixel.r, pixel.g, pixel.b}) < MIN_CHANNEL){
                continue;
            }
            if (rises_above_side(screen, x, y, pixel, -1, near, far) && rises_above_side(screen, x, y, pixel, 1, near, far)){
                matrix.set(x, y, true);
            }
        }
    }
    return matrix;
}


//  One arc: its box on the screen (max exclusive) and which way it opens.
struct Arc{
    size_t min_x;
    size_t max_x;
    size_t min_y;
    size_t max_y;
    //  "(": the middle further left than the ends.
    bool left;

    size_t height() const{
        return max_y - min_y;
    }
    double center_x() const{
        return (min_x + max_x) / 2.0;
    }
};

//  x = a + b t + c t^2 fitted by least squares to the middle of each row,
//  with t from -1 (top) to 1 (bottom); `rms` is the fit's error.
struct ArcFit{
    double b = 0;
    double c = 0;
    double rms = 0;
};

double det3(const double m[3][3]){
    return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
         - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
         + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

std::optional<ArcFit> fit_parabola(const std::vector<double>& ts, const std::vector<double>& xs){
    double s[5] = {};
    double y[3] = {};
    for (size_t i = 0; i < ts.size(); i++){
        double power = 1;
        for (size_t k = 0; k < 5; k++){
            s[k] += power;
            if (k < 3){
                y[k] += power * xs[i];
            }
            power *= ts[i];
        }
    }
    const double m[3][3] = {{s[0], s[1], s[2]}, {s[1], s[2], s[3]}, {s[2], s[3], s[4]}};
    const double d = det3(m);
    if (std::abs(d) < 1e-9){
        return std::nullopt;
    }
    double coefficients[3];
    for (size_t col = 0; col < 3; col++){
        double mi[3][3];
        for (size_t r = 0; r < 3; r++){
            for (size_t c = 0; c < 3; c++){
                mi[r][c] = c == col ? y[r] : m[r][c];
            }
        }
        coefficients[col] = det3(mi) / d;
    }
    ArcFit fit;
    fit.b = coefficients[1];
    fit.c = coefficients[2];
    double error = 0;
    for (size_t i = 0; i < ts.size(); i++){
        const double off = coefficients[0] + fit.b * ts[i] + fit.c * ts[i] * ts[i] - xs[i];
        error += off * off;
    }
    fit.rms = std::sqrt(error / (double)ts.size());
    return fit;
}

//  `object` as an arc, or nothing when it isn't one: a curve 36 or more
//  rows tall at most half the screen, under 0.4 of its height wide, about
//  as thick as an arc (2 to 6 pixels at its median row), present on 90% of
//  its rows, bowing by 6% or more of its height and smooth (the fit off by
//  1 pixel + 2% of its height at most). Window reflections are 1 pixel
//  thick and, on console B's 1080p frames, under 30 rows (the
//  brackets' were 42 or more); fence bars and posts don't bow.
std::optional<Arc> read_arc(const WaterfillObject& object, double scale, size_t screen_rows){
    const size_t width = object.width();
    const size_t height = object.height();
    if (height < 36 * scale || height > screen_rows / 2 || width > 0.4 * height){
        return std::nullopt;
    }
    const std::unique_ptr<Kernels::PackedBinaryMatrix_IB> matrix = object.packed_matrix();
    std::vector<double> ts;
    std::vector<double> xs;
    std::vector<size_t> thickness;
    for (size_t r = 0; r < height; r++){
        size_t count = 0;
        size_t sum = 0;
        size_t first = width;
        size_t last = 0;
        for (size_t c = 0; c < width; c++){
            if (matrix->get(c, r)){
                count++;
                sum += c;
                first = std::min(first, c);
                last = c;
            }
        }
        if (count == 0){
            continue;
        }
        ts.push_back(height > 1 ? 2.0 * r / (double)(height - 1) - 1 : 0);
        xs.push_back((double)sum / count);
        thickness.push_back(last - first + 1);
    }
    if (ts.size() < 0.9 * height){
        return std::nullopt;
    }
    std::nth_element(thickness.begin(), thickness.begin() + thickness.size() / 2, thickness.end());
    const double median_thickness = (double)thickness[thickness.size() / 2];
    if (median_thickness < 2 * scale || median_thickness > 6 * scale){
        return std::nullopt;
    }
    const std::optional<ArcFit> fit = fit_parabola(ts, xs);
    if (!fit || std::abs(fit->c) < 0.06 * height || fit->rms > scale + 0.02 * height){
        return std::nullopt;
    }
    return Arc{object.min_x, object.max_x, object.min_y, object.max_y, fit->c > 0};
}

//  Whether `left` and `right` are one pair: the shorter at least 0.7 of
//  the taller (the lower part of an arc can hide behind a lit gate) and
//  within its rows give or take 12% of its height, their middles 1.4 to 2.3
//  of its height apart.
bool arcs_pair(const Arc& left, const Arc& right){
    if (!left.left || right.left || right.min_x <= left.max_x){
        return false;
    }
    const Arc& tall = left.height() >= right.height() ? left : right;
    const Arc& short_arc = left.height() >= right.height() ? right : left;
    const double h = (double)tall.height();
    if (short_arc.height() < 0.7 * h){
        return false;
    }
    const double slack = 0.12 * h;
    if (short_arc.min_y + slack < tall.min_y || short_arc.max_y > tall.max_y + slack){
        return false;
    }
    const double apart = right.center_x() - left.center_x();
    return apart >= 1.4 * h && apart <= 2.3 * h;
}

LockOnBrackets to_brackets(const Arc& left, const Arc& right, const ImageViewRGB32& screen){
    const Arc& tall = left.height() >= right.height() ? left : right;
    const double w = (double)screen.width();
    const double h = (double)screen.height();
    LockOnBrackets out;
    out.box = ImageFloatBox(
        left.min_x / w, tall.min_y / h,
        (right.max_x - left.min_x) / w, tall.height() / h
    );
    out.x = out.box.x + out.box.width / 2;
    out.y = out.box.y + out.box.height / 2;
    out.height = out.box.height;
    return out;
}

} // namespace


std::optional<LockOnBrackets> find_lock_on_brackets(const ImageViewRGB32& screen){
    if (screen.width() == 0 || screen.height() == 0){
        return std::nullopt;
    }
    const double scale = screen.height() / REFERENCE_ROWS;
    PackedBinaryMatrix matrix = arc_pixels(screen, scale);
    std::unique_ptr<WaterfillSession> session = make_WaterfillSession(matrix);
    std::unique_ptr<WaterfillIterator> iter = session->make_iterator((size_t)std::max(1.0, 15 * scale * scale));
    std::vector<Arc> arcs;
    WaterfillObject object;
    while (iter->find_next(object, true)){
        const std::optional<Arc> arc = read_arc(object, scale, screen.height());
        if (arc){
            arcs.push_back(*arc);
        }
    }
    std::optional<LockOnBrackets> best;
    for (const Arc& left : arcs){
        for (const Arc& right : arcs){
            if (!arcs_pair(left, right)){
                continue;
            }
            const LockOnBrackets pair = to_brackets(left, right, screen);
            //  Wild Pokemon near enough to catch stand below the horizon;
            //  arcs above it are windows' reflections.
            if (pair.y < MIN_MIDDLE_Y){
                continue;
            }
            if (!best || pair.height > best->height){
                best = pair;
            }
        }
    }
    return best;
}


LockOnBracketDetector::LockOnBracketDetector(Color color, VideoOverlay* overlay)
    : m_color(color)
    , m_overlay(overlay)
{}

void LockOnBracketDetector::make_overlays(VideoOverlaySet&) const{}

bool LockOnBracketDetector::detect(const ImageViewRGB32& screen){
    m_last_detected = find_lock_on_brackets(screen);
    if (m_overlay){
        if (m_last_detected){
            m_last_detected_box.emplace(*m_overlay, m_last_detected->box, m_color);
        }else{
            m_last_detected_box.reset();
        }
    }
    return m_last_detected.has_value();
}



namespace{

//  The test frames are from console B in Wild Zone 1 (2026-11-13): the
//  wz1_gate/road/grass/none/stall frames from a video at 1280x720, the
//  ones named with a time from screenshots at 1920x1080 on the user's walk.

struct BracketCase{
    std::string name;
    struct Input{
        std::string image;
    };
    Input input;
    struct Expectation{
        //  The brackets' middle and height as shares of the screen,
        //  labelled by hand; nothing: no brackets.
        struct Shares{
            double x;
            double y;
            double height;
        };
        std::optional<Shares> brackets;
    };
    Expectation expectation;
};

const std::string FOLDER = "PokemonLZA/LockOnBracketDetector/";

const BracketCase BRACKET_CASES[] = {
    {
        .name = "the Mareep by the gate, its left arc's top over the lit gate",
        .input = {.image = "wz1_gate_mareep_04.png"},
        .expectation = {.brackets = {{0.579, 0.394, 0.100}}},
    },
    {
        .name = "the Mareep by the gate",
        .input = {.image = "wz1_gate_mareep_05.png"},
        .expectation = {.brackets = {{0.584, 0.403, 0.082}}},
    },
    {
        .name = "the Mareep by the gate, another grazing right of it",
        .input = {.image = "wz1_gate_mareep_07.png"},
        .expectation = {.brackets = {{0.584, 0.402, 0.100}}},
    },
    {
        .name = "the Mareep by the gate, smaller",
        .input = {.image = "wz1_gate_mareep_08.png"},
        .expectation = {.brackets = {{0.585, 0.405, 0.076}}},
    },
    {
        .name = "the Mareep by the gate, one arc over the gate",
        .input = {.image = "wz1_gate_mareep_10.png"},
        .expectation = {.brackets = {{0.584, 0.405, 0.097}}},
    },
    {
        .name = "a Mareep walking on the road ahead of the player",
        .input = {.image = "wz1_road_mareep_11.png"},
        .expectation = {.brackets = {{0.545, 0.431, 0.075}}},
    },
    {
        .name = "a Mareep on the road left of the player",
        .input = {.image = "wz1_road_mareep_13.png"},
        .expectation = {.brackets = {{0.313, 0.435, 0.094}}},
    },
    {
        .name = "the Mareep in the grass by the rocks",
        .input = {.image = "wz1_grass_mareep_14.png"},
        .expectation = {.brackets = {{0.736, 0.391, 0.086}}},
    },
    {
        .name = "the Mareep in the grass, building windows behind",
        .input = {.image = "wz1_grass_mareep_16.png"},
        .expectation = {.brackets = {{0.735, 0.392, 0.103}}},
    },
    {
        .name = "the Mareep in the grass, small",
        .input = {.image = "wz1_grass_mareep_17.png"},
        .expectation = {.brackets = {{0.735, 0.391, 0.081}}},
    },
    {
        .name = "the Mareep in the grass, large",
        .input = {.image = "wz1_grass_mareep_19.png"},
        .expectation = {.brackets = {{0.736, 0.391, 0.097}}},
    },
    {
        .name = "the Mareep in the grass",
        .input = {.image = "wz1_grass_mareep_20.png"},
        .expectation = {.brackets = {{0.736, 0.392, 0.079}}},
    },
    {
        .name = "a Scatterbug on the plaza's road, 1080p",
        .input = {.image = "wz1_plaza_scatterbug_174310.png"},
        .expectation = {.brackets = {{0.566, 0.473, 0.100}}},
    },
    {
        .name = "a Scatterbug on the road left of the player, 1080p",
        .input = {.image = "wz1_plaza_scatterbug_174312.png"},
        .expectation = {.brackets = {{0.392, 0.566, 0.096}}},
    },
    {
        .name = "a Scatterbug by the partner, 1080p",
        .input = {.image = "wz1_plaza_scatterbug_174319.png"},
        .expectation = {.brackets = {{0.620, 0.607, 0.082}}},
    },
    {
        .name = "a Mareep in front of the gate, 1080p",
        .input = {.image = "wz1_gate_mareep_174326.png"},
        .expectation = {.brackets = {{0.641, 0.351, 0.089}}},
    },
    {
        .name = "a Mareep fighting back in front of the gate, 1080p",
        .input = {.image = "wz1_gate_mareep_battle_174404.png"},
        .expectation = {.brackets = {{0.500, 0.494, 0.078}}},
    },
    {
        .name = "a Mareep fighting back in the grass, 1080p",
        .input = {.image = "wz1_grass_mareep_battle_174434.png"},
        .expectation = {.brackets = {{0.526, 0.565, 0.069}}},
    },
    {
        .name = "a rock ahead has brackets too, 1080p",
        .input = {.image = "wz1_plaza_rock_174305.png"},
        .expectation = {.brackets = {{0.317, 0.447, 0.099}}},
    },
    {
        .name = "none: window reflections high on a building, 1080p",
        .input = {.image = "wz1_plaza_none_174300.png"},
        .expectation = {.brackets = std::nullopt},
    },
    {
        .name = "none: window reflections at the top, 1080p",
        .input = {.image = "wz1_plaza_none_174311.png"},
        .expectation = {.brackets = std::nullopt},
    },
    {
        .name = "none: a building across the plaza, 1080p",
        .input = {.image = "wz1_plaza_none_174309.png"},
        .expectation = {.brackets = std::nullopt},
    },
    {
        .name = "none: the player running onto the road",
        .input = {.image = "wz1_none_01.png"},
        .expectation = {.brackets = std::nullopt},
    },
    {
        .name = "none: window reflections shaped like arcs",
        .input = {.image = "wz1_none_03.png"},
        .expectation = {.brackets = std::nullopt},
    },
    {
        .name = "none: a Mareep on the road with no brackets",
        .input = {.image = "wz1_none_12.png"},
        .expectation = {.brackets = std::nullopt},
    },
    {
        .name = "none: window reflections a pair's width apart",
        .input = {.image = "wz1_none_15.png"},
        .expectation = {.brackets = std::nullopt},
    },
    {
        .name = "none: Mareep in the grass, no brackets",
        .input = {.image = "wz1_none_21.png"},
        .expectation = {.brackets = std::nullopt},
    },
    {
        .name = "none: from the sidewalk by the stall",
        .input = {.image = "wz1_stall_none_002.png"},
        .expectation = {.brackets = std::nullopt},
    },
    {
        .name = "none: from the sidewalk, the vendor's prompt and the windows",
        .input = {.image = "wz1_stall_none_050.png"},
        .expectation = {.brackets = std::nullopt},
    },
    {
        .name = "none: from the sidewalk, the stall's sign",
        .input = {.image = "wz1_stall_none_073.png"},
        .expectation = {.brackets = std::nullopt},
    },
};

std::string brackets_text(const std::optional<LockOnBrackets>& brackets){
    if (!brackets){
        return "no brackets";
    }
    return std::format("brackets at ({:.3f}, {:.3f}), {:.3f} tall", brackets->x, brackets->y, brackets->height);
}

class Test_LockOnBracketDetector : public ImageUnitTest{
public:
    explicit Test_LockOnBracketDetector(const BracketCase& test_case)
        : ImageUnitTest(
            std::format("PokemonLZA::LockOnBracketDetector - {}", test_case.name),
            {FOLDER + test_case.input.image}
        )
        , m_case(test_case)
    {}

    virtual UnitTestResult run_on_images(
        Logger&, CancellableScope&,
        const std::vector<ImageRGB32>& images
    ) const override{
        const std::optional<LockOnBrackets> found = find_lock_on_brackets(images[0]);
        const auto& expected = m_case.expectation.brackets;
        if (!expected){
            if (found){
                return std::format("Expected no brackets, got {}.", brackets_text(found));
            }
            return true;
        }
        //  Within 1% of the screen of the labelled middle, and within 15%
        //  of its height.
        const bool near = found
            && std::abs(found->x - expected->x) <= 0.01
            && std::abs(found->y - expected->y) <= 0.01
            && std::abs(found->height - expected->height) <= 0.15 * expected->height;
        if (!near){
            return std::format(
                "Expected brackets at ({:.3f}, {:.3f}), {:.3f} tall; got {}.",
                expected->x, expected->y, expected->height, brackets_text(found)
            );
        }
        return true;
    }

private:
    const BracketCase& m_case;
};

} // namespace


void add_tests_LockOnBracketDetector(UnitTestDatabase& database){
    for (const BracketCase& item : BRACKET_CASES){
        database.add<Test_LockOnBracketDetector>(item);
    }
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
