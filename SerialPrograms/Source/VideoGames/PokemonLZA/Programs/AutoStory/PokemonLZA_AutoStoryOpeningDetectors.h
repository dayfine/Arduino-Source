/*  PokemonLZA AutoStory - Opening detectors
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Screens of the new game's opening and the autosave badge:
 *  - the station platform once the cutscene ends and the player has control
 *    (PlatformArrivalDetector): the frame's top and sides against a
 *    reference of that first frame, with the player cropped out;
 *  - the "(+) Hold button to skip" badge at the top right
 *    (SkipPromptDetector);
 *  - the autosave badge, a pencil and "Saving..." at the top right
 *    (AutosaveDetector), which shows for 1-3 s after an autosave.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryOpeningDetectors_H
#define PokemonAutomation_PokemonLZA_AutoStoryOpeningDetectors_H

#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The checked-in reference files, ReferenceData/*.json, built into the
//  program (cmake/EmbedTextFiles.cmake): (name, text), by name.
//  plza-platform holds the platform reference's cell colors per region.
const std::vector<std::pair<std::string, std::string>>& autostory_reference_json();


//  How far a frame's regions are from the platform reference: each region
//  is averaged into cells, and the distance is the RMSD over its cells of
//  the summed squared channel differences (0 = the same, at most 441).
struct PlatformDistances{
    double top;
    double left;
    double right;
};

//  The regions of a frame against the platform reference.
PlatformDistances platform_distances(const ImageViewRGB32& screen);

//  Whether `distances` are the platform: top and left within their bounds,
//  and the right side within a looser one (the companion and the NPCs there
//  move). The bounds and the measurements behind them are in the .cpp.
bool platform_matches(const PlatformDistances& distances);

//  The reference as a `width` x `height` frame: each cell painted its
//  color, the player's area black. A frame the detector sees as the
//  platform, for tests.
ImageRGB32 platform_reference_image(size_t width, size_t height);

//  "top 12.3, left 4.5, right 30.1", for the log.
std::string to_string(const PlatformDistances& distances);


//  The station platform with free control, right after the opening
//  cutscene, at the default camera.
class PlatformArrivalDetector : public StaticScreenDetector{
public:
    explicit PlatformArrivalDetector(Color color = COLOR_CYAN);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

    //  The distances of the last detect().
    const PlatformDistances& last_distances() const{
        return m_last;
    }

private:
    Color m_color;
    PlatformDistances m_last{.top = 0, .left = 0, .right = 0};
};


//  The cutscene's "(+) Hold button to skip" badge at the top right: the
//  white (+) icon, white text on the badge, and the badge's dark, even
//  strip under the text.
class SkipPromptDetector : public StaticScreenDetector{
public:
    explicit SkipPromptDetector(Color color = COLOR_MAGENTA);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    ImageFloatBox m_icon;
    ImageFloatBox m_text;
    ImageFloatBox m_strip;
};


//  The autosave badge at the top right: a white pencil with a cyan glow at
//  x 1603-1630, y 73-104 on 1920x1080, and nothing white or cyan just left
//  of it. Only the icon is read, so any language matches.
class AutosaveDetector : public StaticScreenDetector{
public:
    explicit AutosaveDetector(Color color = COLOR_YELLOW);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    ImageFloatBox m_icon;
    ImageFloatBox m_left;
};


//  Registers the detectors' tests: synthetic frames always; the opening's
//  frames from the folders in PLZA_OPENING_FRAMES and PLZA_USER_OPENING_FRAMES,
//  each skipped when unset.
void add_tests_AutoStoryOpeningDetectors(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
