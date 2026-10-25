#include "app/MainWindow.h"
#include "app/SelfTest.h"
#include "core/Types.h"

#include <QApplication>
#include <QSurfaceFormat>
#include <QTimer>

#include <cstring>

int main(int argc, char *argv[])
{
    // 无界面自检：验证 采集 -> 编码 -> 解码 的完整链路
    if (argc > 1 && std::strcmp(argv[1], "--selftest") == 0) {
        const int frames = argc > 2 ? std::atoi(argv[2]) : 120;
        return sc::runSelfTest(frames > 0 ? frames : 120);
    }

    // 渲染后端固定 3.3 core，着色器依赖 layout(location) 与 GL_R8
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSwapInterval(0);
    QSurfaceFormat::setDefaultFormat(format);

    qRegisterMetaType<sc::Packet>("sc::Packet");
    qRegisterMetaType<sc::Frame>("sc::Frame");

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ScreenCast"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));

    sc::MainWindow window;
    window.show();

    // 演示模式：启动后自动开播并回环拉流，可传 host 走局域网地址
    if (argc > 1 && std::strcmp(argv[1], "--demo") == 0) {
        const QString host = argc > 2 ? QString::fromLocal8Bit(argv[2])
                                      : QStringLiteral("127.0.0.1");
        QTimer::singleShot(400, &window, [&window, host] { window.startDemo(host); });
    }

    // 抓帧模式：跑 6 秒后把预览画面存盘并退出，用于验证 GL 渲染
    if (argc > 2 && std::strcmp(argv[1], "--grab") == 0) {
        const QString path = QString::fromLocal8Bit(argv[2]);
        QTimer::singleShot(400, &window, [&window] { window.startDemo(); });
        QTimer::singleShot(6000, &window, [&window, path] { window.grabPreview(path); });
    }

    // 整窗截图：跑 6 秒后保存整个窗口（含预览与统计面板），用于 README
    if (argc > 2 && std::strcmp(argv[1], "--shot") == 0) {
        const QString path = QString::fromLocal8Bit(argv[2]);
        QTimer::singleShot(400, &window, [&window] { window.startDemo(); });
        QTimer::singleShot(6000, &window, [&window, path] {
            const bool ok = window.grab().save(path);
            qInfo() << "shot" << path << (ok ? "ok" : "failed");
            QCoreApplication::quit();
        });
    }

    return app.exec();
}
