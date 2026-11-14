/*  Video Source (mGBA over MgbaLink)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <charconv>
#include <QPainter>
#include <QTimer>
#include <QWidget>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/ImageTypes/ImageRGB32_Qt.h"
#include "CommonFramework/Tools/GlobalThreadPools.h"
#include "VideoSource_MgbaLink.h"

namespace PokemonAutomation{

using namespace MgbaLink;


namespace{

//  Re-anchor the frame clock when a frame maps further than this from the
//  time it was read (mGBA paused, or running free without GRANT pacing).
constexpr std::chrono::milliseconds MAX_CLOCK_DRIFT(500);

constexpr std::chrono::milliseconds RECONNECT_DELAY(2000);

//  One GBA frame, rounded down: polling faster only repeats a frame number.
constexpr std::chrono::milliseconds POLL_PERIOD(16);

}



ImageRGB32 compose_mgba_frame(const ImageViewRGB32& gba_frame, const MgbaComposeGeometry& geometry){
    ImageRGB32 canvas(geometry.canvas_width, geometry.canvas_height);
    canvas.fill(0xff000000);
    const size_t src_width = gba_frame.width();
    const size_t src_height = gba_frame.height();
    if (src_width == 0 || src_height == 0){
        return canvas;
    }
    std::vector<size_t> src_x(geometry.box_width);
    for (size_t x = 0; x < geometry.box_width; x++){
        src_x[x] = (2 * x + 1) * src_width / (2 * geometry.box_width);
    }
    for (size_t y = 0; y < geometry.box_height; y++){
        const size_t sy = (2 * y + 1) * src_height / (2 * geometry.box_height);
        for (size_t x = 0; x < geometry.box_width; x++){
            canvas.pixel(geometry.box_x + x, geometry.box_y + y) = gba_frame.pixel(src_x[x], sy);
        }
    }
    return canvas;
}


std::optional<std::pair<std::string, uint16_t>> parse_mgba_address(std::string_view address){
    const size_t colon = address.rfind(':');
    if (colon == std::string_view::npos || colon == 0){
        return std::nullopt;
    }
    std::string_view port_text = address.substr(colon + 1);
    uint16_t port = 0;
    const char* end = port_text.data() + port_text.size();
    std::from_chars_result result = std::from_chars(port_text.data(), end, port);
    if (port_text.empty() || result.ec != std::errc() || result.ptr != end || port == 0){
        return std::nullopt;
    }
    return std::make_pair(std::string(address.substr(0, colon)), port);
}



std::string VideoSourceDescriptor_MgbaLink::address() const{
    ReadSpinLock lg(m_lock, PA_CURRENT_FUNCTION);
    return m_address;
}
bool VideoSourceDescriptor_MgbaLink::operator==(const VideoSourceDescriptor& x) const{
    if (typeid(*this) != typeid(x)){
        return false;
    }
    return address() == static_cast<const VideoSourceDescriptor_MgbaLink&>(x).address();
}
std::string VideoSourceDescriptor_MgbaLink::display_name() const{
    return "mGBA (" + address() + ")";
}
JsonValue VideoSourceDescriptor_MgbaLink::to_json() const{
    return address();
}
void VideoSourceDescriptor_MgbaLink::load_json(const JsonValue& json){
    const std::string* address = json.to_string();
    if (address != nullptr && parse_mgba_address(*address)){
        WriteSpinLock lg(m_lock, PA_CURRENT_FUNCTION);
        m_address = *address;
    }
}
std::unique_ptr<VideoSource> VideoSourceDescriptor_MgbaLink::make_VideoSource(
    Logger& logger,
    Resolution resolution,
    VideoFormat format,
    FramesPerSecond fps
) const{
    return std::make_unique<VideoSource_MgbaLink>(logger, address());
}



VideoSource_MgbaLink::VideoSource_MgbaLink(Logger& logger, std::string address)
    : VideoSource(logger, false)
    , m_logger(logger)
    , m_address(std::move(address))
{
    m_formats[{1920, 1080}][VideoFormat::OTHER] = {60};
    m_poller = GlobalThreadPools::unlimited_realtime().dispatch_now_blocking(
        [this]{ poll_loop(); }
    );
}
VideoSource_MgbaLink::~VideoSource_MgbaLink(){
    {
        std::lock_guard<Mutex> lg(m_lock);
        m_stopping = true;
    }
    m_cv.notify_all();
    m_poller.wait_and_ignore_exceptions();
}


VideoSnapshot VideoSource_MgbaLink::snapshot_latest_blocking(){
    std::lock_guard<Mutex> lg(m_lock);
    return m_last;
}
VideoSnapshot VideoSource_MgbaLink::snapshot_recent_nonblocking(WallClock min_time){
    std::lock_guard<Mutex> lg(m_lock);
    return m_last;
}


bool VideoSource_MgbaLink::wait_or_stop(std::chrono::milliseconds duration){
    std::unique_lock<Mutex> lg(m_lock);
    return m_cv.wait_for(lg, duration, [this]{ return m_stopping; });
}

void VideoSource_MgbaLink::poll_loop(){
    std::optional<std::pair<std::string, uint16_t>> address = parse_mgba_address(m_address);
    if (!address){
        m_logger.log("MgbaLink: invalid address \"" + m_address + "\"; expected host:port.", COLOR_RED);
        return;
    }
    do{
        try{
            MgbaLink_Connection connection(
                m_logger, GlobalThreadPools::unlimited_realtime(), address->first, address->second
            );
            connection.open();
            poll_connected(connection);
        }catch (Exception& e){
            m_logger.log(e.message() + " Retrying in 2 s.", COLOR_ORANGE);
        }
    }while (!wait_or_stop(RECONNECT_DELAY));
}

void VideoSource_MgbaLink::poll_connected(MgbaLink_Connection& connection){
    std::optional<FrameClock> clock;
    uint64_t last_frame = 0;
    do{
        FrameReply reply = connection.frame();
        WallClock now = current_time();
        if (clock && reply.frame == last_frame){
            continue;
        }
        if (!clock){
            clock.emplace(now, reply.frame);
        }else if (clock->resync(now, reply.frame, MAX_CLOCK_DRIFT)){
            m_logger.log("MgbaLink: frame clock re-anchored at frame " + std::to_string(reply.frame) + ".");
        }
        last_frame = reply.frame;

        //  Read the PNG before the next FRAME overwrites it.
        ImageRGB32 gba_frame(reply.path);
        VideoSnapshot snapshot(compose_mgba_frame(gba_frame), clock->timestamp(reply.frame));
        std::lock_guard<Mutex> lg(m_lock);
        m_last = std::move(snapshot);
    }while (!wait_or_stop(POLL_PERIOD));
}



class VideoWidget_MgbaLink : public QWidget{
public:
    VideoWidget_MgbaLink(QWidget* parent, VideoSource_MgbaLink& source)
        : QWidget(parent)
        , m_source(source)
    {
        QTimer* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]{ update(); });
        timer->start(33);
    }

private:
    virtual void paintEvent(QPaintEvent* event) override{
        QWidget::paintEvent(event);
        VideoSnapshot snapshot = m_source.snapshot_latest_blocking();
        if (!snapshot.frame || snapshot.frame->width() == 0){
            return;
        }
        QPainter painter(this);
        painter.drawImage(QRect(0, 0, width(), height()), to_QImage_ref(*snapshot.frame));
    }

private:
    VideoSource_MgbaLink& m_source;
};


QWidget* VideoSource_MgbaLink::make_display_QtWidget(QWidget* parent){
    return new VideoWidget_MgbaLink(parent, *this);
}



}
