#ifndef NEWWINDOW_H
#define NEWWINDOW_H

#include <QMainWindow>
#include <complex>  // 确保包含复数标准库头文件
#include "qcustomplot.h"

#include <complex>  // 复数运算（FFT需要）
#include <valarray> // 高效数组运算

namespace Ui {
class NewWindow;
}

class NewWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit NewWindow(QWidget *parent = nullptr);
    ~NewWindow();

    void updateSpectrumBars(const QVector<double>& filteredAudioData);

    void updateWaveform(const QVector<double> &waveData);

    void setCenterFrequency(double frequency);
    static QVector<std::complex<double>> fft(const QVector<std::complex<double>>& x);

signals:
    // 声明传递归一化值的信号
    void normalizedValueChanged(double value);

    // 声明中心频率变化信号
    void centerFrequencyChanged(double newCenterFreq);





    // 新增：传递当前窗口的最大滤强信号
    void filteredIntensityUpdated(double intensity);


private slots:
    // 按钮点击事件的槽函数声明
    void on_pushButton_clicked();

    //    void on_pushButton_2_clicked();

    //    void on_pushButton_3_clicked();





    // 添加这三个函数的声明
    void onAddClicked();          // 对应+100按钮
    void onSubtractClicked();     // 对应-100按钮
    void updateLabelSize();       // 更新标签大小

    void on_pushButton_4_clicked();

    void on_pushButton_5_clicked();

private:

    // 你的强度源（你已有也行）
    QVector<double> m_intensityBuffer;   // 最近10次强度（可保留，不冲突）

    // ===== 记录任务状态 =====
    bool m_recording = false;
    QVector<double> m_recordSamples;

    // ===== 柱状图 =====
    QVector<double> m_avgHistory; // 每次记录完成后的平均值（柱子高度）
    QCPBars* m_avgBars = nullptr;
    int m_maxBars = 10;
    QVector<QCPItemText*> m_barValueLabels;  // 每根柱子的数值标签



    Ui::NewWindow *ui;
    QVector<double> m_xData;

    // 硬件选频参数
    double m_sampleRate=44100;    // 采样率
    double m_centerFreq=500;    // 中心频率

    QLabel *label_5;
    QPushButton *pushButton_2;
    QPushButton *pushButton_3;
    // 新增：从label5/7获取的滤波范围
    double m_filterMinFreq;  // 频率下限（来自label5）
    double m_filterMaxFreq;  // 频率上限（来自label7）

    // FFT和绘图参数
    const int FFT_SIZE = 512;       // FFT点数（需为2的幂）
    const double SAMPLE_RATE = 44100;// 采样率
    QCPBars *m_spectrumBars;         // 柱状频谱图对象

    int m_labelUpdateCounter = 0;  // label_3更新计数器
    const int m_labelUpdateThreshold = 3;





};

#endif // NEWWINDOW_H
