/*  Video Source (mGBA over MgbaLink)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <charconv>
#include <format>
#include <QImage>
#include <QPainter>
#include <QTimer>
#include <QVideoFrame>
#include <QWidget>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/ImageTypes/ImageRGB32_Qt.h"
#include "CommonFramework/Tools/GlobalThreadPools.h"
#include "CommonFramework/VideoPipeline/Backends/VideoFrameQt.h"
#include "VideoSource_MgbaLink.h"

namespace PokemonAutomation{

using namespace MgbaLink;


namespace{

//  Re-anchor the frame clock when a frame maps further than this before
//  the time it was read (mGBA paused, or slower than real time).
constexpr std::chrono::milliseconds MAX_CLOCK_LAG(500);

constexpr std::chrono::milliseconds RECONNECT_DELAY(2000);

//  One GBA frame, rounded down: polling faster only repeats a frame number.
constexpr std::chrono::milliseconds POLL_PERIOD(16);


//  Forwards to "logger" every line but a repeat of the last one forwarded:
//  a bridge that stays away fails the same way on every retry.
class RepeatSuppressingLogger : public Logger{
public:
    explicit RepeatSuppressingLogger(Logger& logger)
        : m_logger(logger)
    {}

    virtual void log(const std::string& msg, Color color) override{
        {
            std::lock_guard<Mutex> lg(m_lock);
            if (msg == m_last){
                return;
            }
            m_last = msg;
        }
        m_logger.log(msg, color);
    }

private:
    Logger& m_logger;
    Mutex m_lock;
    std::string m_last;
};

bool is_local_host(std::string_view host){
    return host == "127.0.0.1" || host == "localhost";
}

}  // namespace



ImageRGB32 compose_mgba_frame(const ImageViewRGB32& gba_frame, const MgbaComposeGeometry& geometry){
    if (geometry.box_x + geometry.box_width > geometry.canvas_width ||
        geometry.box_y + geometry.box_height > geometry.canvas_height
    ){
        throw InternalProgramError(
            nullptr, PA_CURRENT_FUNCTION,
            std::format(
                "mGBA box {}x{} at ({}, {}) does not fit in a {}x{} canvas.",
                geometry.box_width, geometry.box_height, geometry.box_x, geometry.box_y,
                geometry.canvas_width, geometry.canvas_height
            )
        );
    }
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
    std::from_chars_result result = std::from_chars(port_text.data(), port_text.data() + port_text.size(), port);
    if (port_text.empty() || result.ec != std::errc() || result.ptr != port_text.data() + port_text.size() || port == 0){
        return std::nullopt;
    }
    std::string_view host = address.substr(0, colon);
    if (!is_local_host(host)){
        return std::nullopt;
    }
    return std::make_pair(std::string(host), port);
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
        if (m_connection != nullptr){
            m_connection->abort();
        }
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
    if (m_last.timestamp < min_time){
        return VideoSnapshot();
    }
    return m_last;
}


bool VideoSource_MgbaLink::wait_or_stop(std::chrono::milliseconds duration){
    std::unique_lock<Mutex> lg(m_lock);
    return m_cv.wait_for(lg, duration, [this]{ return m_stopping; });
}

void VideoSource_MgbaLink::poll_loop(){
    std::optional<std::pair<std::string, uint16_t>> address = parse_mgba_address(m_address);
    if (!address){
        m_logger.log(
            "MgbaLink: invalid address \"" + m_address + "\"; expected 127.0.0.1:<port> or localhost:<port>.",
            COLOR_RED
        );
        return;
    }
    m_logger.log(std::format(
        "MgbaLink: polling {}; retrying every {} s while it is away.",
        m_address, std::chrono::duration_cast<std::chrono::seconds>(RECONNECT_DELAY).count()
    ));
    RepeatSuppressingLogger logger(m_logger);
    do{
        try{
            run_connection(logger, address->first, address->second);
        }catch (Exception&){
            //  Already logged (once) by the exception; retry.
        }
    }while (!wait_or_stop(RECONNECT_DELAY));
}

void VideoSource_MgbaLink::run_connection(Logger& logger, const std::string& host, uint16_t port){
    Connection connection(logger, GlobalThreadPools::unlimited_realtime(), host, port);
    {
        std::lock_guard<Mutex> lg(m_lock);
        if (m_stopping){
            return;
        }
        m_connection = &connection;
    }
    try{
        connection.open();
        poll_connected(logger, connection);
    }catch (...){
        std::lock_guard<Mutex> lg(m_lock);
        m_connection = nullptr;
        m_last = VideoSnapshot();
        throw;
    }
    std::lock_guard<Mutex> lg(m_lock);
    m_connection = nullptr;
    m_last = VideoSnapshot();
}

void VideoSource_MgbaLink::poll_connected(Logger& logger, Connection& connection){
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
        }
        FrameClock::Stamp stamp = clock->stamp(now, reply.frame, MAX_CLOCK_LAG);
        if (stamp.reanchored_after_lag){
            m_logger.log(std::format("MgbaLink: frame clock re-anchored at frame {} (mGBA paused or slow).", reply.frame));
        }
        last_frame = reply.frame;

        //  Read the PNG before the next FRAME overwrites it.
        ImageRGB32 gba_frame(reply.path);
        if (gba_frame.width() != reply.width || gba_frame.height() != reply.height){
            throw ConnectionException(&logger, std::format(
                "MgbaLink: FRAME {} said {}x{}, but {} is {}x{}.",
                reply.frame, reply.width, reply.height, reply.path, gba_frame.width(), gba_frame.height()
            ));
        }
        publish_frame(compose_mgba_frame(gba_frame), stamp.time);
    }while (!wait_or_stop(POLL_PERIOD));
}

void VideoSource_MgbaLink::publish_frame(ImageRGB32 frame, WallClock timestamp){
    VideoSnapshot snapshot(std::move(frame), timestamp);
    {
        std::lock_guard<Mutex> lg(m_lock);
        m_last = snapshot;
    }

    //  For the FPS readout and stream history. The QImage shares the
    //  snapshot's pixels and keeps them alive.
    const ImageRGB32& pixels = *snapshot.frame;
    QImage image(
        (const uchar*)pixels.data(),
        (int)pixels.width(),
        (int)pixels.height(),
        (qsizetype)pixels.bytes_per_row(),
        QImage::Format_ARGB32,
        [](void* owner){ delete static_cast<std::shared_ptr<const ImageRGB32>*>(owner); },
        new std::shared_ptr<const ImageRGB32>(snapshot.frame)
    );
    report_source_frame(std::make_shared<VideoFrame>(timestamp, QVideoFrame(image)));
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
        m_source.report_rendered_frame(current_time());
    }

private:
    VideoSource_MgbaLink& m_source;
};


QWidget* VideoSource_MgbaLink::make_display_QtWidget(QWidget* parent){
    return new VideoWidget_MgbaLink(parent, *this);
}



}  // namespace PokemonAutomation
