#include "audiorecorder.h"
#include <QApplication>
#include <QFile>
#include <QDebug>  // 用于调试输出

int main(int argc, char *argv[])
{
//以此为基础提交后续
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));
    QApplication a(argc, argv);
    /* 指定文件 */
    QFile file(":/style.qss");

    /* 判断文件是否存在 */
    if (file.exists() ) {
        /* 以只读的方式打开 */
        file.open(QFile::ReadOnly);
        /* 以字符串的方式保存读出的结果 */
        QString styleSheet = QLatin1String(file.readAll());
        /* 设置全局样式 */
        qApp->setStyleSheet(styleSheet);
        /* 关闭文件 */
        file.close();
    }

    // 固件名为可选参数，桌面预览时无需额外参数也能启动。
    const QString firmware = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    AudioRecorder w(nullptr, firmware);
    // 显示窗口
    w.show();
    return a.exec();


}



