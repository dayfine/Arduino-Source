/*  QCameraThread
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_VideoPipeline_QCameraThread_H
#define PokemonAutomation_VideoPipeline_QCameraThread_H

#include <QCamera>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Qt/GlobalThreadPoolsQt.h"
#include "CommonFramework/Devices/LabDeviceRuntime.h"

//#include <iostream>
//using std::cout;
//using std::endl;

namespace PokemonAutomation{


// Own the camera and its device lease across the Qt event-thread lifetime.
class QCameraThread : public QObject{
public:
    // A denied input must not throw across run_on_main_thread_and_wait's Qt callback.
    static std::unique_ptr<QCameraThread> try_create(Logger& logger, QCameraDevice device, QCameraFormat format){
        try{
            return std::make_unique<QCameraThread>(logger, std::move(device), std::move(format));
        }catch (UserSetupError&){
            return nullptr;  // UserSetupError already logged the refusal.
        }
    }

    QCameraThread(
        Logger& logger,
        QCameraDevice device,
        QCameraFormat format
    )
        : m_device_lease(acquire_lab_capture(logger, LabDeviceKind::VIDEO, device.id().toHex().toStdString()))
        , m_logger(logger)
        , m_device(std::move(device))
        , m_format(std::move(format))
        , m_camera(nullptr)
    {
        m_camera = static_cast<QCamera*>(GlobalThreadPools::qt_event_threadpool().add_object(
            [this]{ return make_camera(); }
        ));
    }
    ~QCameraThread(){
        GlobalThreadPools::qt_event_threadpool().remove_object(m_camera);
    }

    QCamera& camera() const{
        return *m_camera;
    }


private:
    std::unique_ptr<QCamera> make_camera(){
        auto camera = std::make_unique<QCamera>(m_device);
        camera->setCameraFormat(m_format);
        connect(
            camera.get(), &QCamera::errorOccurred,
            this, [this](){
                if (m_camera->error() == QCamera::NoError){
                    return;
                }
                m_logger.log("QCamera error: " + m_camera->errorString().toStdString(), COLOR_RED);
            }
        );

        camera->start();
        return camera;
    }


private:
    std::unique_ptr<LabDeviceLease> m_device_lease;
    Logger& m_logger;
    QCameraDevice m_device;
    QCameraFormat m_format;
    QCamera* m_camera;
};



} // namespace PokemonAutomation
#endif
