#ifndef GPS_H
#define GPS_H
#include <QWidget>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QList> // 必须包含QList头文件
namespace Ui {
class gps;
}

// 简化结构体：仅存储经纬度（无时间戳）
struct GpsData {
    double latitude;  // 纬度
    double longitude; // 经度
};


class gps : public QWidget
{
    Q_OBJECT

public:
    explicit gps(QWidget *parent = nullptr);
    ~gps();


public slots:

    // 关键：声明必须是两个double参数，与cpp实现完全一致
     void onGpsDataReceived(double latitude, double longitude);

private slots:
    void on_pushButton_clicked();
      void on_pushButton_2_clicked(); // 保存按钮

private:
    Ui::gps *ui;
    QWidget *m_originalWindow;  // 用于保存原窗口指针
    QList<GpsData> m_allGpsData; // 缓存所有组经纬度


};

#endif // GPS_H
