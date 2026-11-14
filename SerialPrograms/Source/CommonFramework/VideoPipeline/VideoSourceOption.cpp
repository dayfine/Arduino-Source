/*  Video Source Descriptor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/EnumStringMap.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "CommonFramework/StaticGlobals.h"

#include "Sources/VideoSource_Null.h"
#include "Sources/VideoSource_StillImage.h"
#include "Sources/VideoSource_Camera.h"
#include "Sources/VideoSource_MgbaLink.h"

#include "VideoSourceOption.h"

//#include <iostream>
//using std::cout;
//using std::endl;

namespace PokemonAutomation{


const EnumStringMap<VideoSourceType> VIDEO_TYPE_STRINGS{
    {VideoSourceType::None,             "None"},
    {VideoSourceType::StillImage,       "Still Image"},
    {VideoSourceType::VideoPlayback,    "Video Playback"},
    {VideoSourceType::Camera,           "Camera"},
    {VideoSourceType::MgbaLink,         "mGBA Link"},
};


namespace{

//  Loads the saved parameters of one source type into the cache, if present.
template <typename DescriptorType>
void load_cached_descriptor(
    std::map<VideoSourceType, std::shared_ptr<VideoSourceDescriptor>>& cache,
    const JsonObject& obj,
    VideoSourceType type
){
    const JsonValue* params = obj.get_value(VIDEO_TYPE_STRINGS.get_string(type));
    if (params == nullptr){
        return;
    }
    auto descriptor = std::make_unique<DescriptorType>();
    descriptor->load_json(*params);
    cache[type] = std::move(descriptor);
}

}  // namespace


VideoSourceOption::VideoSourceOption()
    : m_resolution(1920, 1080)
    , m_format(VideoFormat::OTHER)
    , m_fps(0)
    , m_descriptor(new VideoSourceDescriptor_Null())
{}

void VideoSourceOption::set_descriptor(std::shared_ptr<VideoSourceDescriptor> descriptor){
    m_descriptor_cache[descriptor->type] = descriptor;
    m_descriptor = std::move(descriptor);
}
std::shared_ptr<VideoSourceDescriptor> VideoSourceOption::get_descriptor_from_cache(VideoSourceType type){
    auto iter = m_descriptor_cache.find(type);
    if (iter != m_descriptor_cache.end()){
        return iter->second;
    }
    std::shared_ptr<VideoSourceDescriptor> descriptor;
    switch (type){
    case VideoSourceType::None:
        descriptor.reset(new VideoSourceDescriptor_Null());
        break;
    case VideoSourceType::StillImage:
        descriptor.reset(new VideoSourceDescriptor_StillImage());
        break;
    case VideoSourceType::Camera:
        descriptor.reset(new VideoSourceDescriptor_Camera());
        break;
    case VideoSourceType::MgbaLink:
        descriptor = std::make_shared<VideoSourceDescriptor_MgbaLink>();
        break;
    default:;
        descriptor.reset(new VideoSourceDescriptor_Null());
    }
    m_descriptor_cache[descriptor->type] = descriptor;
    return descriptor;
}



void VideoSourceOption::load_json(const JsonValue& json){
    std::shared_ptr<VideoSourceDescriptor> descriptor;
    do{
        if (json.is_null()){
            break;
        }

        const JsonObject* obj = json.to_object();
        if (obj == nullptr){
            break;
        }

        const JsonArray* res = obj->get_array("Resolution");
        if (res != nullptr && res->size() == 2){
            do{
                size_t width, height;
                if (!(*res)[0].read_integer(width)){
                    break;
                }
                if (!(*res)[1].read_integer(height)){
                    break;
                }
                m_resolution = Resolution(width, height);
//                cout << "VideoSourceOption::load_json(): " << this << " - " << width << " x " << height << endl;
            }while (false);
        }

        const std::string* format = obj->get_string("Format");
        if (format != nullptr){
            const EnumEntry* entry = VideoFormat_database().find_slug(*format);
            if (entry != nullptr){
//                cout << "loading format: " << *format << endl;
                m_format = (VideoFormat)entry->enum_value;
            }
        }

        obj->read_integer(m_fps, "Fps");

        const std::string* type = obj->get_string("SourceType");
        if (type == nullptr){
            break;
        }

#if 0
        for (const auto& item : VIDEO_TYPE_STRINGS){
            const JsonValue* params = obj->get_value(VIDEO_TYPE_STRINGS.get_string(item.first));
            if (params == nullptr){
                continue;
            }
            m_descriptor_cache[item.first] = item.second->make(*params);
        }
#endif

        load_cached_descriptor<VideoSourceDescriptor_StillImage>(m_descriptor_cache, *obj, VideoSourceType::StillImage);
        load_cached_descriptor<VideoSourceDescriptor_Camera>(m_descriptor_cache, *obj, VideoSourceType::Camera);
        if (STATIC_GLOBALS.DEVELOPER_MODE){
            load_cached_descriptor<VideoSourceDescriptor_MgbaLink>(m_descriptor_cache, *obj, VideoSourceType::MgbaLink);
        }

        auto iter = m_descriptor_cache.find(VIDEO_TYPE_STRINGS.get_enum(*type, VideoSourceType::None));
        if (iter == m_descriptor_cache.end()){
            break;
        }

        descriptor = iter->second;
    }while (false);

    if (descriptor == nullptr){
        descriptor.reset(new VideoSourceDescriptor_Null());
    }

    m_descriptor = std::move(descriptor);
}
JsonValue VideoSourceOption::to_json() const{
    if (!m_descriptor){
        return JsonValue();
    }
    JsonObject obj;
    {
        JsonArray res;
        res.push_back(m_resolution.width);
        res.push_back(m_resolution.height);
        obj["Resolution"] = std::move(res);
    }
    const EnumEntry* entry = VideoFormat_database().find(m_format);
    if (entry){
        obj["Format"] = entry->slug;
    }
    obj["Fps"] = m_fps;
    obj["SourceType"] = VIDEO_TYPE_STRINGS.get_string(m_descriptor->type);

    for (const auto& item : m_descriptor_cache){
        obj[VIDEO_TYPE_STRINGS.get_string(item.first)] = item.second->to_json();
    }

    return obj;
}


}
