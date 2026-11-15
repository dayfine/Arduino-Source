/*  Video Source (mGBA over MgbaLink)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Shows the picture of an mGBA instance running "bridge.lua" (MgbaLink v1).
 *  Each new emulated frame is composed into a 1920x1080 frame at FRLG's
 *  GAME_BOX with nearest-neighbour scaling, so detectors see console
 *  geometry. While mGBA runs no faster than 59.7275 fps, a frame is stamped
 *  with the time its frame number maps to; when it runs ahead (e.g. synced
 *  to a 60 Hz display), with the time it was read. Never later than the
 *  read time. The source reconnects itself, so it opts out of the session
 *  no-video watchdog. Developer mode only (fork lab tooling).
 *
 */

#ifndef PokemonAutomation_VideoPipeline_VideoSource_MgbaLink_H
#define PokemonAutomation_VideoPipeline_VideoSource_MgbaLink_H

#include <stdint.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Concurrency/AsyncTask.h"
#include "Common/Cpp/Concurrency/ConditionVariable.h"
#include "Common/Cpp/Concurrency/Mutex.h"
#include "Common/Cpp/Concurrency/SpinLock.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "Controllers/MgbaLink/MgbaLink_Connection.h"
#include "CommonFramework/VideoPipeline/VideoSourceDescriptor.h"
#include "CommonFramework/VideoPipeline/VideoSource.h"

namespace PokemonAutomation{


//  Where the GBA picture goes: GAME_BOX {0.09375, 0.00462963, 0.8125,
//  0.962963} of a 1920x1080 frame, i.e. x = 180..1740, y = 5..1045 (6.5x).
struct MgbaComposeGeometry{
    size_t canvas_width = 1920;
    size_t canvas_height = 1080;
    size_t box_x = 180;
    size_t box_y = 5;
    size_t box_width = 1560;
    size_t box_height = 1040;
};

//  Draws "gba_frame" into a black canvas at the box, nearest-neighbour with
//  pixel-centre sampling (as Pillow's NEAREST, which the geometry gate used):
//  canvas pixel x takes source column floor((2 (x - box_x) + 1) w / (2 box_width)).
//  Throws InternalProgramError if the box does not fit inside the canvas.
ImageRGB32 compose_mgba_frame(
    const ImageViewRGB32& gba_frame,
    const MgbaComposeGeometry& geometry = MgbaComposeGeometry()
);


//  "host:port" -> (host, port). nullopt if the port is missing or invalid,
//  or the host is not this machine ("127.0.0.1" or "localhost"): FRAME
//  names a PNG on the bridge's disk, which this app opens as a local path.
std::optional<std::pair<std::string, uint16_t>> parse_mgba_address(std::string_view address);



//  Selects an mGBA bridge by "host:port" (saved in the profile).
class VideoSourceDescriptor_MgbaLink : public VideoSourceDescriptor{
public:
    static constexpr std::string_view DEFAULT_ADDRESS = "127.0.0.1:47801";

    VideoSourceDescriptor_MgbaLink()
        : VideoSourceDescriptor(VideoSourceType::MgbaLink)
        , m_address(DEFAULT_ADDRESS)
    {}

    std::string address() const;

    virtual bool operator==(const VideoSourceDescriptor& x) const override;
    virtual std::string display_name() const override;

    virtual JsonValue to_json() const override;
    virtual void load_json(const JsonValue& json) override;

    virtual std::unique_ptr<VideoSource> make_VideoSource(
        Logger& logger,
        Resolution resolution,
        VideoFormat format,
        FramesPerSecond fps
    ) const override;

private:
    mutable SpinLock m_lock;
    std::string m_address;
};



//  Polls FRAME on its own connection and keeps the latest composed frame;
//  drops it when the connection is lost, so a dead bridge shows no video.
//
//  The bridge holds one client at a time, so this source takes its only
//  slot. While open, the connection is published in MgbaLink::link_registry()
//  under the source's address; the mGBA controller for the same address
//  sends KEYS and GRANT on it (Connection serializes commands from several
//  threads).
class VideoSource_MgbaLink : public VideoSource{
public:
    VideoSource_MgbaLink(Logger& logger, std::string address);
    ~VideoSource_MgbaLink();

    virtual Resolution current_resolution() const override{
        return Resolution(1920, 1080);
    }
    virtual VideoFormat current_format() const override{
        return VideoFormat::OTHER;
    }
    virtual FramesPerSecond current_fps() const override{
        return 60;
    }
    virtual const VideoFormatSet& supported_formats() const override{
        return m_formats;
    }

    virtual VideoSnapshot snapshot_latest_blocking() override;
    virtual VideoSnapshot snapshot_recent_nonblocking(WallClock min_time) override;

    virtual QWidget* make_display_QtWidget(QWidget* parent) override;

private:
    friend class VideoWidget_MgbaLink;

    void poll_loop();
    void run_connection(Logger& logger, const std::string& host, uint16_t port);
    void poll_connected(Logger& logger, MgbaLink::Connection& connection);
    void publish_frame(ImageRGB32 frame, WallClock timestamp);
    bool wait_or_stop(std::chrono::milliseconds duration);

private:
    Logger& m_logger;
    const std::string m_address;
    VideoFormatSet m_formats;

    Mutex m_lock;
    ConditionVariable m_cv;
    bool m_stopping = false;
    VideoSnapshot m_last;
    //  The poller's open connection, if any; the destructor aborts it so it
    //  never waits out a reply timeout.
    MgbaLink::Connection* m_connection = nullptr;

    AsyncTask m_poller;
};



}  // namespace PokemonAutomation
#endif
