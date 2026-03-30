#include "audiorecorder.h"
#include "ui_testwidget.h"
#include <QApplication>
#include <QFile>
#include <QDebug>  // 用于调试输出

int main(int argc, char *argv[])
{

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

    // 创建AudioRecorder实例
    AudioRecorder w(nullptr, QString(argv[1]));
    // 显示窗口
    w.show();
    return a.exec();


}



