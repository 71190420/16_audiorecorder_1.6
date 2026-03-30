/******************************************************************
Copyright (C) 2017 The Qt Company Ltd.
Copyright © Deng Zhimao Co., Ltd. 1990-2021. All rights reserved.
* @projectName   16_audiorecorder
* @brief         audiorecorder.h - 实时录音并直接播放
* @date          2021-05-10
*******************************************************************/
#ifndef AUDIORECORDER_H
#define AUDIORECORDER_H

#include <QMainWindow>
#include <QListWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QAudioInput>
#include <QAudioOutput>
#include <QIODevice>
#include <QTimer>
#include <QSerialPortInfo>
#include <QSerialPort>
#include <QTextBrowser>
#include "qcustomplot.h"
#include "newwindow.h"
#include "barchartmainwindow.h" // 包含QMainWindow柱状图窗口头文件



class gps;  // 告诉编译器有一个名为 gps 的类存在
/* 媒体信息结构体 */
struct MediaObjectInfo {
    /* 用于保存音频文件名 */
    QString fileName;
    /* 用于保存音频文件路径 */
    QString filePath;
};

class AudioRecorder : public QMainWindow
{
    Q_OBJECT

public:
    AudioRecorder(QWidget *parent = nullptr,QString firmware = nullptr);
    ~AudioRecorder();

    int   ycyl=0;
    int   ychz=0;

    QVector<double> findAllSolutions(double tol);

    // 新增：从PCM-TXT文件还原为可播放WAV
    bool restorePcmFromTxtToWav(const QString& txtFilePath, const QString& outputWavPath);


signals:

    // 新增：发送经纬度数据的信号（供GPS界面接收）
    void gpsDataSent(double latitude, double longitude);




    // 声明信号，用于发送最大左声道音频强度
    void maxLeftChannelLevelUpdated(qreal maxLevel);

    void leftChannelSamples(const QVector<double>& samples);
    // 新增：发送左声道强度到NewWindow
    void leftChannelLevelUpdated(qreal level);

private:

     QLineEdit *m_valueLineEdit;
    // 新增：强度更新频率控制
    int m_intensityUpdateCounter = 0;       // 强度更新计数器
    const int m_intensityUpdateThreshold = 10; // 更新阈值（可调整，3=原频率的1/3）
    gps *m_gpsWindow = nullptr;  // 现在编译器能识别 gps 类了
    double m_latitude = 0.0;
    double m_longitude = 0.0;


    //OLD
    QLabel *volumeValueLabel;  // 新增：音量值显示标签

    QSlider *vSlider;  // 新增：垂直音量滑块
    // 添加这一行声明
    qreal leftChannelLevel;  // 左声道强度值
    QProgressBar *leftLevelBar; // 左声道强度柱状图（用进度条实现）



    QPushButton* m_restorePcmBtn; // 新增：还原功能按钮

    // ---------------------- 新增1：原始音频保存相关（独立） ----------------------
    bool m_isSavingRaw = false;         // 原始数据保存开关
    QFile m_rawWavFile;                 // 原始数据WAV文件
    QByteArray m_rawBuffer;             // 原始数据缓冲区
    QPushButton *m_saveRawBtn = nullptr;// “保存原始音频”按钮
    qint64 m_rawDataSize = 0;           // 原始数据总大小

    // ---------------------- 新增2：滤波音频保存相关（独立） ----------------------
    bool m_isSavingFiltered = false;    // 滤波数据保存开关
    QFile m_filteredWavFile;            // 滤波数据WAV文件
    QByteArray m_filteredBuffer;        // 滤波数据缓冲区
    QPushButton *m_saveFilteredBtn = nullptr;// “保存滤波音频”按钮
    qint64 m_filteredDataSize = 0;      // 滤波数据总大小

    // ---------------------- 通用：WAV文件头结构体（适配16位双声道PCM） ----------------------
#pragma pack(1)
    struct WavHeader {
        // RIFF头
        char riffId[4] = {'R', 'I', 'F', 'F'};
        quint32 riffSize;               // 总文件大小 - 8
        char waveId[4] = {'W', 'A', 'V', 'E'};
        // fmt子块（PCM格式固定）
        char fmtId[4] = {'f', 'm', 't', ' '};
        quint32 fmtSize = 16;           // PCM格式子块大小为16
        quint16 audioFormat = 1;        // 1=PCM无压缩
        quint16 channelCount = 2;       // 双声道（与采集格式一致）
        quint32 sampleRate = 44100;     // 采样率（与采集格式一致）
        quint16 bitsPerSample = 16;     // 16位样本（与采集格式一致）
        // 动态计算字段（根据上面参数推导）
        quint32 byteRate = sampleRate * channelCount * (bitsPerSample / 8); // 44100*2*2=176400
        quint16 blockAlign = channelCount * (bitsPerSample / 8); // 2*2=4
        // data子块
        char dataId[4] = {'d', 'a', 't', 'a'};
        quint32 dataSize;               // 音频数据区大小
    };
#pragma pack()


    // 已有的保存相关变量（确认存在）
    QPushButton *newLeftBtn; // 新增的“保存”按钮
    //        bool m_saveFilteredLeftData = false; // 保存左声道滤波数据的标志位（1=保存，0=停止）
    //        int m_leftDataCount = 0; // 左声道数据计数
    //        QVector<double> m_leftDataBuffer; // 左声道数据缓冲区
    //        const int MAX_LEFT_DATA = 50000; // 最大保存数量
    //        int m_leftFileIndex = 0; // 文件索引（用于区分多文件）


    bool m_saveBothChannels = false;      // 双声道保存总开关
    QVector<double> m_leftDataBuffer;     // 左声道数据缓冲区
    QVector<double> m_rightDataBuffer;    // 右声道数据缓冲区
    int m_bothDataCount = 0;              // 已保存的帧数（每帧含左右各1个数据）
    int m_bothFileIndex = 0;              // 保存文件索引
    const int MAX_BOTH_DATA = 100000;      // 最大保存帧数（5万帧）


    QVector<double> m_originalLeftDataBuffer;
    QVector<double> m_originalRightDataBuffer;
    // 添加这两个函数的声明
    void saveBothFilteredData(const QVector<double>& filteredLeftData, const QVector<double>& filteredRightData,
                              const QVector<double>& originalLeftData, const QVector<double>& originalRightData);
    void saveDataToFile(const QString& fileName, const QVector<double>& leftData, const QVector<double>& rightData);

private:


    int m_spectrumUpdateCounter = 0; // 频谱更新计数器
    const int m_spectrumUpdateInterval = 3; // 每3帧更新一次（降低到1/3频率）

    //柱状图
    BarChartMainWindow *m_barChartWindow = nullptr; // 基于QMainWindow的窗口指针

    double m_globalMaxFilteredIntensity = 0.0; // 全局最大滤波强度（百分比）

    // 新增：全局最大滤强相关声明
    QLabel *globalMaxLabel;               // 显示全局最大滤强的UI标签

    //jiemian
    NewWindow *m_waveWindow;  // 添加这行声明
    NewWindow *m_newWindow = nullptr;  // 新窗口指针，初始化为nullptr

    QLabel *modeLabel;  // 新增Label的成员变量声明
    double m_normalizedValue = 0.0;  // 保存从NewWindow传递的normalized值

    //滤波
    // 新增：与NewWindow一致的滤波器和增益参数
    double m_sampleRate = 44100;
    double m_centerFreq = 500;
    double m_bandwidth = 100;
    double m_totalGain = 1.0;  // 保存与NewWindow一致的总增益

    // 滤波函数声明
    QVector<double> butterworthBandpassFilter(const QVector<double> &data);
    // 从滤波数据计算强度的函数声明
    QVector<qreal> getBufferLevelsFromFilteredData(const QVector<double> &filteredData, const QAudioFormat &format);


    bool m_saveReceivedData = false; // 用于控制是否保存数据的标志位
    // 添加这个函数声明

    qreal m_gain = 1.0;  // 放大倍数
    void saveRawDataToTxt(const QByteArray &buffer);
    // 新增的垂直滑动条及标签
    QSlider *horizontalSlider; // 原verticalSlider改为horizontalSlider
    QLabel *sliderValueLabel;      // 滑动条数值显示标签


    QLabel *valueDisplayLabel;       // 数值显示标签
    QPushButton *decreaseValueBtn;   // 减少按钮
    QPushButton *increaseValueBtn;   // 增加按钮
    int currentValue;                // 当前数值（初始255）
    QTimer *m_decreaseLongPressTimer;  // 减号按钮长按定时器
    QTimer *m_increaseLongPressTimer;  // 加号按钮长按定时器

    // 添加音量按钮声明
    QPushButton *volumeDownBtn;  // 音量减按钮
    QPushButton *volumeUpBtn;    // 音量加按钮


    QTimer *m_volumeDownLongPressTimer;  // 音量减长按定时器
    QTimer *m_volumeUpLongPressTimer;    // 音量加长按定时器



    // 音量控制相关
    QPushButton *decreaseVolumeBtn;
    QPushButton *increaseVolumeBtn;
    QLabel *volumeLabel;
    QTimer *m_decreaseVolumeTimer;
    QTimer *m_increaseVolumeTimer;
    int currentVolume; // 当前音量值 (0-127)





    QTextBrowser* testBrowser;
    // 新增：声明logBrowser成员变量
    QTextBrowser *logBrowser;
    double equation(double h);
    double solveH(double tol = 1e-6); // 在这里指定默认参数

    QSlider *hSlider; // 滑动条成员变量


    double solveBisection(double low, double high, double tol);


    QString firmware;
    QSerialPort *serialPort;  // 串口对象


    void loadFirmware();  // 加载固件
    void unloadFirmware();  // 卸载固件
    void scanSerialPort();  // 扫描串口
    void openSerialPort();  // 打开串口
    void closeSerialPort();  // 关闭串口
    void sendPushButtonClicked();  // 发送数据

    // 数据解析函数
    void extractAudioInfo(const QByteArray& data);



    /* 布局初始化 */
    void layoutInit();
    // 添加以下成员变量声明
    QLabel *leftChannelLevelLabel; // 左声道强度标签
    QPushButton *commandBt1; // 新增按钮
    QPushButton *commandBt2; // 新增按钮
    QPushButton *commandBt3; // 新增按钮



    // 在类定义内添加
    QPushButton *newBtn1;  // 新按钮1
    QPushButton *newBtn2;  // 新按钮2
    QPushButton *newBtn3;  // 新按钮3
    QPushButton *newBtn4;  // 新按钮4


    // 新增成员（关键缺失部分）
    QPushButton *decreaseBtn;  // 减小按钮
    QPushButton *increaseBtn;  // 增大按钮
    QLabel *aValueLabel;       // a值显示标签
    double a;                  // a的当前值（类成员，非全局）
    // 新增成员变量，用于保存左声道音频强度的最大值
    qreal m_maxLeftChannelLevel;
    // 新增 QLabel 用于显示最大值
    QLabel *maxLeftChannelLevelLabel;




    qreal m_maxRightChannelLevel;  // 新增的右声道最大值

    QLabel *i0Label;
    QPushButton *i0Btn;
    QLabel *i1Label;
    QPushButton *i1Btn;
    QLabel *i2Label;
    QPushButton *i2Btn;
    QLabel *hLabel;
    QPushButton *hBtn;


    // 新增变量
    double i0;
    double i1;
    double i2;

    QPushButton *commandBt4; // 新增按钮
    QPushButton *commandBt5; // 新增按钮
    QPushButton *commandBt6; // 新增按钮
    QPushButton *executeBt; // 执行 ./7 按钮
    /* 主Widget */
    QWidget *mainWidget;

    /* 录音列表（保留以备将来扩展，当前不使用） */
    QListWidget *listWidget;

    /* 底部的Widget,用于存放按钮 */
    QWidget *bottomWidget;

    /* 中间的显示录制时长的Widget容器 */
    QWidget *centerWidget;

    /* 垂直布局 */
    QVBoxLayout *vBoxLayout;

    /* 录音Level布局 */
    QHBoxLayout *levelHBoxLayout;

    /* 水平布局 */
    QHBoxLayout *hBoxLayout;

    /* 录音按钮 */
    QPushButton *recorderBt;

    /* 删除按钮（保留以备将来扩展，当前不使用） */
    QPushButton *removeBt;

    /* 录音设置容器，保存录音设备的可用信息，
     * 本例使用默认的信息，即可录音 */
    QList<QVariant> devicesVar;
    QList<QVariant> codecsVar;
    QList<QVariant> containersVar;
    QList<QVariant> sampleRateVar;
    QList<QVariant> channelsVar;
    QList<QVariant> qualityVar;
    QList<QVariant> bitratesVar;

    /* 用于显示录音时长 */
    QLabel *countLabel;

    /* 用于显示录音level,最多四通道 */
    QProgressBar *progressBar[4];

    /* 清空录音level */
    void clearAudioLevels();

    /* 处理音频输入 */
    void handleAudioInput();

    /* 获取缓冲区的音频级别 */
    QVector<qreal> getBufferLevels(const QByteArray &buffer);

    /* 更新时间标签 */
    void updateProgress();

    /* 设置音频 */
    void setupAudio();

    /* 成员变量 */
    QAudioInput *m_audioInput;
    QAudioOutput *m_audioOutput;
    QIODevice *m_inputDevice;
    QIODevice *m_outputDevice;
    bool isRecording;
    QTimer *recordTimer;



private slots:

    // 新增：接收NewWindow发送的滤强数据
       void onFilteredIntensityReceived(double intensity);


    void onLevelUpdated(qreal level);

    void onRestorePcmBtnClicked();
    // 原始音频保存/播放
    void onSaveRawBtnClicked();         // 新增：“保存原始音频”按钮（开始/停止）
    void writeRawToWav(const QByteArray &rawData); // 新增：原始数据写入WAV
    void onPlayRawBtnClicked();         // 新增：播放已保存的原始音频
    // 滤波音频保存/播放
    void onSaveFilteredBtnClicked();    // 新增：“保存滤波音频”按钮（开始/停止）
    void writeFilteredToWav(const QByteArray &filteredData); // 新增：滤波数据写入WAV
    void onPlayFilteredBtnClicked();    // 新增：播放已保存的滤波音频
    // 通用工具函数
    bool initWavFile(QFile &wavFile, QByteArray &buffer, qint64 &dataSize, const QString &fileType); // 初始化WAV文件
    void finishWavFile(QFile &wavFile, qint64 dataSize, const QString &fileType); // 完成WAV文件（更新头信息）
    void playSavedWav(const QString &defaultDir, const QString &title); // 通用播放已保存WAV函数


    // 接收NewWindow的参数变化信号

    void onCenterFrequencyChanged(double newFreq);
    //jiemian

    /*读取数据 */
    void serialPortReadyRead();

    /* 点击录音按钮槽函数 */
    void recorderBtClicked();
    void executeBtClicked(); // 槽函数


    void commandBt1Clicked(); // 新增槽函数
    void commandBt2Clicked(); // 新增槽函数
    void commandBt3Clicked(); // 新增槽函数

    void commandBt6Clicked(); // 新增槽函数




    void updateHValueFromSlider(int value);
    void updateVerticalSliderValue(int value); // 垂直滑动条的槽函数

    // 其他槽函数...
    void newBtn1Clicked();  // 新增槽函数声明
    void newBtn2Clicked();  // 新增槽函数声明
    void newBtn3Clicked();  // 新增槽函数声明
    void newBtn4Clicked();  // 新增槽函数声明

};

#endif // AUDIORECORDER_H
