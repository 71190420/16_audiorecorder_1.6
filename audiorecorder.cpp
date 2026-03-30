#include "audiorecorder.h"
#include <QDebug>
#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QByteArray>
#include <QStyle>
#include <QApplication>
#include <QProcess>
#include "newwindow.h"  // 包含新窗口类的头文件
#include <QtMath>  // 包含Qt数学函数库，提供qSqrt等函数
#include <QFile>

#include "gps.h"
AudioRecorder::AudioRecorder(QWidget *parent, QString firmware)
    : QMainWindow(parent),
      m_audioInput(nullptr),
      m_audioOutput(nullptr),
      m_inputDevice(nullptr),
      m_outputDevice(nullptr),
      isRecording(false),
      recordTimer(nullptr),
      a(0.5),
      m_maxLeftChannelLevel(0),
      firmware(firmware),
      serialPort(new QSerialPort(this)),
      // 初始化新增的计数器
      m_intensityUpdateCounter(0)
{
    /* 初始化布局 */
    layoutInit();

    /* 设置音频 */
    setupAudio();

    system("echo 255 > /sys/devices/platform/soc/40015000.i2c/i2c-2/2-002c/rdac1");  // 执行第一个命令
    system("echo 255 > /sys/devices/platform/soc/40015000.i2c/i2c-2/2-002c/rdac0");  // 执行第一个命令

    system("amixer -c 0 cset numid=20 0");  // 执行第一个命令
    system("amixer -c 0 cset numid=21 0");  // 执行第一个命令

    /* 连接按钮 */
    connect(recorderBt, &QPushButton::clicked, this, &AudioRecorder::recorderBtClicked);

    // 连接新按钮的信号与槽
    connect(commandBt1, &QPushButton::clicked, this, &AudioRecorder::commandBt1Clicked);
    connect(commandBt2, &QPushButton::clicked, this, &AudioRecorder::commandBt2Clicked);
    connect(commandBt3, &QPushButton::clicked, this, &AudioRecorder::commandBt3Clicked);
    connect(commandBt6, &QPushButton::clicked, this, &AudioRecorder::commandBt6Clicked);




    connect(executeBt, &QPushButton::clicked, this, &AudioRecorder::executeBtClicked);  // 这一行是新增的
    // 连接新按钮的信号与槽（示例）
    connect(newBtn1, &QPushButton::clicked, this, &AudioRecorder::newBtn1Clicked);
    connect(newBtn2, &QPushButton::clicked, this, &AudioRecorder::newBtn2Clicked);
    connect(newBtn3, &QPushButton::clicked, this, &AudioRecorder::newBtn3Clicked);
    connect(newBtn4, &QPushButton::clicked, this, &AudioRecorder::newBtn4Clicked);

    // 连接滑动条信号（示例：更新h值显示）
    connect(hSlider, &QSlider::valueChanged, this, &AudioRecorder::updateHValueFromSlider);

    this->firmware = firmware;

    qDebug() << "固件名字" << firmware << endl;


    serialPort = new QSerialPort(this);


    connect(serialPort, SIGNAL(readyRead()),this, SLOT(serialPortReadyRead()));

    // 连接信号槽
    connect(serialPort, &QSerialPort::readyRead, this, &AudioRecorder::serialPortReadyRead);


    // 在AudioRecorder的构造函数中添加
    connect(this, &AudioRecorder::leftChannelLevelUpdated,
            this, &AudioRecorder::onLevelUpdated);  // 先连接到自身槽函数

}

// 新增槽函数，转发信号到已创建的窗口
void AudioRecorder::onLevelUpdated(qreal level) {
    if (m_newWindow) {  // 若窗口已创建，直接转发
        m_newWindow->setLeftChannelLevel(level);
    }
}




AudioRecorder::~AudioRecorder()
{
    if (isRecording) {
        m_audioInput->stop();
        m_audioOutput->stop();
    }
    delete m_audioInput;
    delete m_audioOutput;
    if (recordTimer) {
        recordTimer->stop();
        delete recordTimer;
    }

    delete m_decreaseLongPressTimer;    // 释放减号定时器
    delete m_increaseLongPressTimer;    // 释放加号定时器


    // ---------------------- 新增：原始音频文件清理 ----------------------
    if (m_rawWavFile.isOpen()) {
        finishWavFile(m_rawWavFile, m_rawDataSize, "raw");
    }

    // ---------------------- 新增：滤波音频文件清理 ----------------------
    if (m_filteredWavFile.isOpen()) {
        finishWavFile(m_filteredWavFile, m_filteredDataSize, "filtered");
    }



}


#include <QRegExp> // 用于解析TXT文件格式

// 从PCM-TXT文件还原为可播放WAV
bool AudioRecorder::restorePcmFromTxtToWav(const QString& txtFilePath, const QString& outputWavPath)
{
    // ---------------------- 1. 验证文件与参数 ----------------------
    QFile txtFile(txtFilePath);
    if (!txtFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        testBrowser->append("PCM-TXT文件打开失败：" + txtFile.errorString());
        return false;
    }

    // 初始化WAV文件（复用你已有的WAV头逻辑）
    QFile wavFile(outputWavPath);
    QByteArray wavBuffer;
    qint64 wavDataSize = 0;

    // 关键：WAV头参数必须与你保存PCM时的格式一致（44100Hz、16位、双声道）
    WavHeader wavHeader;
    wavHeader.sampleRate = 44100;       // 与setupAudio中的采样率一致
    wavHeader.channelCount = 2;         // 双声道
    wavHeader.bitsPerSample = 16;       // 16位采样
    wavHeader.byteRate = wavHeader.sampleRate * wavHeader.channelCount * (wavHeader.bitsPerSample / 8); // 计算字节率
    wavHeader.blockAlign = wavHeader.channelCount * (wavHeader.bitsPerSample / 8); // 计算块对齐
    wavHeader.audioFormat = 1;          // PCM格式（1表示线性PCM）
    wavHeader.dataSize = 0;             // 初始数据大小为0，后续更新
    wavHeader.riffSize = 36 + wavHeader.dataSize; // WAV头固定计算方式

    // 打开WAV文件并写入初始头
    if (!wavFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        testBrowser->append("WAV输出文件创建失败：" + wavFile.errorString());
        txtFile.close();
        return false;
    }
    wavFile.write(reinterpret_cast<const char*>(&wavHeader), sizeof(WavHeader));
    testBrowser->append("开始还原PCM→WAV：" + txtFilePath + " → " + outputWavPath);

    // ---------------------- 2. 解析TXT文件中的PCM数据 ----------------------
    QTextStream txtIn(&txtFile);
    QString line;
    QRegExp pcmRegExp("帧\\s+\\d+:\\s+声道0=(-?\\d+)\\s+声道1=(-?\\d+)"); // 匹配TXT行格式："帧 X: 声道0=值 声道1=值"
    pcmRegExp.setMinimal(true);

    while (!txtIn.atEnd()) {
        line = txtIn.readLine().trimmed();
        if (line.isEmpty() || !pcmRegExp.exactMatch(line)) {
            continue; // 跳过空行或格式错误的行
        }

        // 提取左右声道的PCM数值（16位有符号整数）
        qint16 leftSample = pcmRegExp.cap(1).toInt();  // 左声道值
        qint16 rightSample = pcmRegExp.cap(2).toInt(); // 右声道值

        // 边界检查：确保数值在16位有符号整数范围内（-32768 ~ 32767）
        leftSample = qBound(static_cast<qint16>(-32768), leftSample, static_cast<qint16>(32767));
        rightSample = qBound(static_cast<qint16>(-32768), rightSample, static_cast<qint16>(32767));

        // ---------------------- 3. 转换为二进制PCM并写入WAV ----------------------
        // 16位PCM为小端字节序（与你保存时的格式一致）
        char leftBytes[2], rightBytes[2];
        memcpy(leftBytes, &leftSample, 2);  // 左声道转换为2字节
        memcpy(rightBytes, &rightSample, 2); // 右声道转换为2字节

        // 写入WAV文件（左声道→右声道，符合双声道PCM顺序）
        wavFile.write(leftBytes, 2);
        wavFile.write(rightBytes, 2);
        wavDataSize += 4; // 每帧（左右声道）占4字节（16位×2）
    }

    // ---------------------- 4. 更新WAV头的实际数据大小 ----------------------
    wavHeader.dataSize = wavDataSize;
    wavHeader.riffSize = 36 + wavDataSize;
    wavFile.seek(4);  // 更新riffSize（WAV头第4字节开始）
    wavFile.write(reinterpret_cast<const char*>(&wavHeader.riffSize), 4);
    wavFile.seek(40); // 更新dataSize（WAV头第40字节开始）
    wavFile.write(reinterpret_cast<const char*>(&wavHeader.dataSize), 4);

    // ---------------------- 5. 清理资源 ----------------------
    txtFile.close();
    wavFile.close();
    testBrowser->append("PCM还原完成！WAV文件：" + outputWavPath + "（大小：" + QString::number(wavDataSize + 44) + "字节）");
    return true;
}


void AudioRecorder::onRestorePcmBtnClicked()
{

}



bool AudioRecorder::initWavFile(QFile &wavFile, QByteArray &buffer, qint64 &dataSize, const QString &fileType)
{
    // 1. 创建专属保存目录（原始数据→RawAudio，滤波数据→FilteredAudio）
    QString dirName = (fileType == "raw") ? "RawAudio" : "FilteredAudio";
    QDir saveDir(QCoreApplication::applicationDirPath() + "/" + dirName);
    if (!saveDir.exists()) {
        if (!saveDir.mkpath(saveDir.absolutePath())) {
            testBrowser->append(dirName + "目录创建失败");
            return false;
        }
        testBrowser->append("创建" + dirName + "目录：" + saveDir.absolutePath());
    }

    // 2. 生成唯一文件名（时间戳+类型标识）
    QString fileName = saveDir.absolutePath() + "/"
            + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_")
            + fileType + ".wav";

    // 3. 打开WAV文件（只写+截断）
    wavFile.setFileName(fileName);
    if (!wavFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        testBrowser->append(fileType + "WAV文件打开失败：" + wavFile.errorString());
        return false;
    }

    // 4. 写入初始WAV头（dataSize和riffSize初始为0，停止时更新）
    WavHeader wavHeader;
    wavHeader.dataSize = 0;
    wavHeader.riffSize = 36 + wavHeader.dataSize; // 36 = 44（总头大小）- 8（RIFF标识）
    wavFile.write(reinterpret_cast<const char*>(&wavHeader), sizeof(WavHeader));

    // 5. 重置缓冲区和数据大小计数
    buffer.clear();
    dataSize = 0;

    testBrowser->append("开始保存" + fileType + "音频：" + fileName);
    return true;
}

void AudioRecorder::finishWavFile(QFile &wavFile, qint64 dataSize, const QString &fileType)
{
    if (!wavFile.isOpen()) return;

    // 1. 写入缓冲区中剩余的数据
    QByteArray *buffer = (fileType == "raw") ? &m_rawBuffer : &m_filteredBuffer;
    if (!buffer->isEmpty()) {
        qint64 written = wavFile.write(*buffer);
        if (written > 0) {
            dataSize += written;
        }
        buffer->clear();
    }

    // 2. 更新WAV头的dataSize和riffSize（关键：否则文件无法播放）
    WavHeader wavHeader;
    wavHeader.dataSize = dataSize;
    wavHeader.riffSize = 36 + wavHeader.dataSize;

    wavFile.seek(4);  // riffSize在第4字节
    wavFile.write(reinterpret_cast<const char*>(&wavHeader.riffSize), 4);
    wavFile.seek(40); // dataSize在第40字节
    wavFile.write(reinterpret_cast<const char*>(&wavHeader.dataSize), 4);

    // 3. 关闭文件并更新日志
    QString filePath = wavFile.fileName();
    wavFile.close();
    testBrowser->append(fileType + "音频保存完成：" + filePath
                        + "（总大小：" + QString::number(dataSize + 44) + "字节）");
}


void AudioRecorder::playSavedWav(const QString &defaultDir, const QString &title)
{
    // 1. 打开文件选择器
    QString wavPath = QFileDialog::getOpenFileName(
                this, title,
                QCoreApplication::applicationDirPath() + "/" + defaultDir,
                "WAV音频文件 (*.wav);;所有文件 (*.*)");
    if (wavPath.isEmpty()) return;

    // 2. 使用QFile指针避免复制（关键修正）
    QFile *playFile = new QFile(wavPath);
    if (!playFile->open(QIODevice::ReadOnly)) {
        testBrowser->append("文件打开失败：" + playFile->errorString());
        delete playFile; // 记得释放
        return;
    }

    // 3. 解析WAV头
    WavHeader wavHeader;
    if (playFile->read(reinterpret_cast<char*>(&wavHeader), sizeof(WavHeader)) != sizeof(WavHeader) ||
            memcmp(wavHeader.riffId, "RIFF", 4) != 0 ||
            memcmp(wavHeader.waveId, "WAVE", 4) != 0 ||
            wavHeader.audioFormat != 1) {
        testBrowser->append("不是合法的WAV文件");
        playFile->close();
        delete playFile;
        return;
    }

    // 4. 初始化播放格式
    QAudioFormat playFormat;
    playFormat.setSampleRate(wavHeader.sampleRate);
    playFormat.setChannelCount(wavHeader.channelCount);
    playFormat.setSampleSize(wavHeader.bitsPerSample);
    playFormat.setCodec("audio/pcm");
    playFormat.setByteOrder(QAudioFormat::LittleEndian);
    playFormat.setSampleType(QAudioFormat::SignedInt);

    // 5. 检查播放设备兼容性
    QAudioDeviceInfo devInfo = QAudioDeviceInfo::defaultOutputDevice();
    if (!devInfo.isFormatSupported(playFormat)) {
        testBrowser->append("播放设备不支持该格式，尝试兼容...");
        playFormat = devInfo.nearestFormat(playFormat);
    }

    // 6. 启动播放设备（使用指针管理生命周期）
    QAudioOutput *playOutput = new QAudioOutput(devInfo, playFormat, this);
    QIODevice *playDev = playOutput->start();
    if (!playDev) {
        testBrowser->append("播放设备初始化失败");
        delete playOutput;
        playFile->close();
        delete playFile;
        return;
    }

    // 7. 定时器控制播放速度（lambda捕获指针，避免复制）
    QTimer *playTimer = new QTimer(this);
    playTimer->setInterval(50);
    // 关键修正：捕获指针，使用[=]按值捕获指针（而非对象）
    connect(playTimer, &QTimer::timeout, this, [=]() mutable {
        if (!playFile->isOpen() || playFile->atEnd()) {
            // 播放完成，清理资源
            playTimer->stop();
            playOutput->stop();
            playFile->close();
            // 释放动态分配的资源
            delete playTimer;
            delete playOutput;
            delete playFile;
            testBrowser->append(title + "完成：" + wavPath);
            return;
        }
        // 读取并播放数据
        int readSize = wavHeader.byteRate * 0.05;
        QByteArray data = playFile->read(readSize);
        if (!data.isEmpty()) {
            playDev->write(data);
        }
    });

    // 8. 启动播放
    playTimer->start();
    testBrowser->append("开始" + title + "：" + wavPath);
}

void AudioRecorder::onSaveRawBtnClicked()
{
    m_isSavingRaw = !m_isSavingRaw;

    if (m_isSavingRaw) {
        // 开始保存：初始化WAV文件
        if (!initWavFile(m_rawWavFile, m_rawBuffer, m_rawDataSize, "raw")) {
            m_isSavingRaw = false;
            return;
        }
        m_saveRawBtn->setText("停止保存原始");
    } else {
        // 停止保存：更新WAV头并关闭文件
        finishWavFile(m_rawWavFile, m_rawDataSize, "raw");
        m_saveRawBtn->setText("保存原始音频");
    }
}

void AudioRecorder::writeRawToWav(const QByteArray &rawData)
{
    if (!m_isSavingRaw || !m_rawWavFile.isOpen()) return;

    // 1. 原始数据加入缓冲区
    m_rawBuffer.append(rawData);

    // 2. 缓冲区满（4KB）时批量写入文件，减少IO次数
    const int BUFFER_SIZE = 4096;
    if (m_rawBuffer.size() >= BUFFER_SIZE) {
        qint64 written = m_rawWavFile.write(m_rawBuffer);
        if (written < 0) {
            testBrowser->append("原始音频写入失败：" + m_rawWavFile.errorString());
            m_isSavingRaw = false;
            m_rawWavFile.close();
            m_saveRawBtn->setText("保存原始音频");
            return;
        }
        m_rawDataSize += written;
        m_rawBuffer.remove(0, written);
    }
}



void AudioRecorder::onFilteredIntensityReceived(double intensity) {
    qDebug() << "AudioRecorder转发滤波强度到BarChart：" << intensity;  // 新增调试打印
    if (m_barChartWindow) {
        m_barChartWindow->setGlobalMaxFilteredData(intensity);
    } else {
        qDebug() << "BarChartMainWindow尚未创建，无法转发";  // 新增调试打印
    }
}


void AudioRecorder::onPlayRawBtnClicked()
{
    // 调用通用播放函数，默认路径为RawAudio，标题为“选择原始音频文件”
    playSavedWav("RawAudio", "选择原始音频文件");
}
void AudioRecorder::onSaveFilteredBtnClicked()
{
    m_isSavingFiltered = !m_isSavingFiltered;

    if (m_isSavingFiltered) {
        // 开始保存：初始化WAV文件
        if (!initWavFile(m_filteredWavFile, m_filteredBuffer, m_filteredDataSize, "filtered")) {
            m_isSavingFiltered = false;
            return;
        }
        m_saveFilteredBtn->setText("停止保存滤波");
    } else {
        // 停止保存：更新WAV头并关闭文件
        finishWavFile(m_filteredWavFile, m_filteredDataSize, "filtered");
        m_saveFilteredBtn->setText("保存滤波音频");
    }
}
void AudioRecorder::writeFilteredToWav(const QByteArray &filteredData)
{
    if (!m_isSavingFiltered || !m_filteredWavFile.isOpen()) return;

    // 1. 滤波数据加入缓冲区
    m_filteredBuffer.append(filteredData);

    // 2. 缓冲区满（4KB）时批量写入文件
    const int BUFFER_SIZE = 4096;
    if (m_filteredBuffer.size() >= BUFFER_SIZE) {
        qint64 written = m_filteredWavFile.write(m_filteredBuffer);
        if (written < 0) {
            testBrowser->append("滤波音频写入失败：" + m_filteredWavFile.errorString());
            m_isSavingFiltered = false;
            m_filteredWavFile.close();
            m_saveFilteredBtn->setText("保存滤波音频");
            return;
        }
        m_filteredDataSize += written;
        m_filteredBuffer.remove(0, written);
    }
}
void AudioRecorder::onPlayFilteredBtnClicked()
{
    // 调用通用播放函数，默认路径为FilteredAudio，标题为“选择滤波音频文件”
    playSavedWav("FilteredAudio", "选择滤波音频文件");
}




//void AudioRecorder::layoutInit()
//{
//    this->setGeometry(100, 100, 800, 480);
//    this->setWindowTitle("音频调节工具");

//    mainWidget = new QWidget(this);
//    setCentralWidget(mainWidget);

//    QVBoxLayout *vBoxLayout = new QVBoxLayout(mainWidget);
//    vBoxLayout->setContentsMargins(5, 5, 5, 5);
//    vBoxLayout->setSpacing(8);

//    QWidget *topControlWidget = new QWidget();
//    QHBoxLayout *topHBox = new QHBoxLayout(topControlWidget);
//    topHBox->setSpacing(5);

//    QWidget *leftControlWidget = new QWidget();
//    QHBoxLayout *leftHBox = new QHBoxLayout(leftControlWidget);
//    leftHBox->setSpacing(5);

//    recorderBt = new QPushButton("启动", this);
//    recorderBt->setFixedSize(90, 32);
//    leftHBox->addWidget(recorderBt);

//    executeBt = new QPushButton("返回", this);
//    executeBt->setFixedSize(90, 32);
//    leftHBox->addWidget(executeBt);

//    topHBox->addWidget(leftControlWidget);

//    QWidget *aControlWidget = new QWidget();
//    QHBoxLayout *aControlLayout = new QHBoxLayout(aControlWidget);
//    aControlLayout->setSpacing(2);

//    // ========== 重点优化：valueControlWidget 布局 ==========
//    QWidget *valueControlWidget = new QWidget();
//    QHBoxLayout *valueHBox = new QHBoxLayout(valueControlWidget);
//    valueHBox->setContentsMargins(10, 0, 10, 0); // 增加左右边距，避免贴边
//    valueHBox->setSpacing(8); // 增大控件间距，更透气
//    valueHBox->setAlignment(Qt::AlignCenter); // 整体居中对齐

//    // 数值显示标签优化
//    valueDisplayLabel = new QLabel("255", this);
//    valueDisplayLabel->setFixedSize(50, 35); // 调整尺寸，匹配输入框高度
//    valueDisplayLabel->setStyleSheet(R"(
//        QLabel {
//            background: white;
//            border: 1px solid #ccc;
//            border-radius: 4px; /* 圆角更美观 */
//            text-align: center;
//            font-size: 14px;
//            font-weight: 500;
//        }
//    )");
//    valueDisplayLabel->setAlignment(Qt::AlignCenter); // 文字居中
//    valueHBox->addWidget(valueDisplayLabel);

//    // 优化输入框样式和布局
//    m_valueLineEdit = new QLineEdit(this);
//    m_valueLineEdit->setFixedSize(120, 35); // 调整宽度，更协调
//    m_valueLineEdit->setStyleSheet(R"(
//        QLineEdit {
//            background: white;
//            border: 1px solid #ccc;
//            border-radius: 4px; /* 圆角 */
//            padding: 0 8px; /* 内边距，避免文字贴边 */
//            text-align: center;
//            font-size: 14px;
//            font-weight: 500;
//            color: #333;
//        }
//        QLineEdit:hover {
//            border-color: #66afe9; /* hover 高亮边框 */
//        }
//        QLineEdit:focus {
//            border-color: #4ecdc4; /* 聚焦高亮 */
//            outline: none;
//        }
//    )");
//    m_valueLineEdit->setPlaceholderText("输入调节值 (0-255)"); // 更明确的提示
//    m_valueLineEdit->setAlignment(Qt::AlignCenter); // 文字居中
//    valueHBox->addWidget(m_valueLineEdit);

//    // 优化增减按钮样式和尺寸
//    decreaseValueBtn = new QPushButton("-", this);
//    decreaseValueBtn->setFixedSize(40, 35); // 调整尺寸，匹配输入框高度
//    decreaseValueBtn->setStyleSheet(R"(
//        QPushButton {
//            background-color: #ff6b6b;
//            color: white;
//            border: none;
//            border-radius: 4px; /* 圆角 */
//            font-size: 16px;
//            font-weight: bold;
//        }
//        QPushButton:hover {
//            background-color: #ff5252; /* hover 加深颜色 */
//        }
//        QPushButton:pressed {
//            background-color: #d32f2f; /* 按下加深 */
//        }
//    )");
//    valueHBox->addWidget(decreaseValueBtn);

//    increaseValueBtn = new QPushButton("+", this);
//    increaseValueBtn->setFixedSize(40, 35); // 调整尺寸，匹配输入框高度
//    increaseValueBtn->setStyleSheet(R"(
//        QPushButton {
//            background-color: #4ecdc4;
//            color: white;
//            border: none;
//            border-radius: 4px; /* 圆角 */
//            font-size: 16px;
//            font-weight: bold;
//        }
//        QPushButton:hover {
//            background-color: #26a69a; /* hover 加深颜色 */
//        }
//        QPushButton:pressed {
//            background-color: #00897b; /* 按下加深 */
//        }
//    )");
//    valueHBox->addWidget(increaseValueBtn);

//    // 给valueControlWidget添加轻微边框和背景，增强视觉区分
//    valueControlWidget->setStyleSheet(R"(
//        QWidget {
//            background-color: #f9f9f9;
//            border: 1px solid #eee;
//            border-radius: 6px;
//            padding: 5px;
//        }
//    )");
//    topHBox->addWidget(valueControlWidget);
//    // ========== valueControlWidget 优化结束 ==========

//    vBoxLayout->addWidget(topControlWidget);

//    QWidget *functionWidget = new QWidget();
//    QGridLayout *functionGrid = new QGridLayout(functionWidget);
//    functionGrid->setHorizontalSpacing(5);
//    functionGrid->setVerticalSpacing(8);

//    QString textStyle = "QLabel { font-size: 20px; background: white; border: 1px solid #999; border-radius: 2px; text-align: center; }";

//    newBtn1 = new QPushButton("远程", this);
//    newBtn1->setFixedSize(80, 22);
//    functionGrid->addWidget(newBtn1, 2, 0);

//    newBtn2 = new QPushButton("获取", this);
//    newBtn2->setFixedSize(80, 22);
//    functionGrid->addWidget(newBtn2, 2, 1);

//    newBtn3 = new QPushButton("频率", this);
//    newBtn3->setFixedSize(80, 22);
//    functionGrid->addWidget(newBtn3, 2, 2);

//    newBtn4 = new QPushButton("delete", this);
//    newBtn4->setFixedSize(80, 22);
//    functionGrid->addWidget(newBtn4, 2, 3);

//    QWidget *volumeControlWidget = new QWidget();
//    QHBoxLayout *volumeHBox = new QHBoxLayout(volumeControlWidget);
//    volumeHBox->setContentsMargins(0, 0, 0, 0);
//    volumeHBox->setSpacing(5);

//    volumeDownBtn = new QPushButton("-", this);
//    volumeDownBtn->setFixedSize(30, 25);
//    volumeHBox->addWidget(volumeDownBtn);

//    hSlider = new QSlider(Qt::Horizontal, this);
//    hSlider->setRange(0, 127);
//    hSlider->setValue(64);
//    hSlider->setTickPosition(QSlider::NoTicks);
//    hSlider->setStyleSheet(R"(
//                           QSlider::groove:horizontal { height: 12px; background: #f0f0f0; border-radius: 6px; }
//                           QSlider::handle:horizontal { width: 20px; background: #ff5555; margin: -4px 0; border-radius: 10px; }
//                           )");
//    volumeHBox->addWidget(hSlider);

//    volumeUpBtn = new QPushButton("+", this);
//    volumeUpBtn->setFixedSize(30, 25);
//    volumeHBox->addWidget(volumeUpBtn);

//    functionGrid->addWidget(volumeControlWidget, 3, 0, 1, 4);

//    testBrowser = new QTextBrowser(this);
//    testBrowser->setStyleSheet(R"(
//                               border: 1px solid #ccc;
//                               border-radius: 4px;
//                               background-color: #f8f8f8;
//                               font-size: 20px;
//                               padding: 8px;
//                               )");
//    testBrowser->setMinimumHeight(80);
//    testBrowser->setMaximumHeight(100);
//    testBrowser->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
//    testBrowser->setReadOnly(true);

//    QString defaultText = "当前音量: 64/127\n";
//    testBrowser->setPlainText(defaultText);

//    functionGrid->setRowStretch(4, 1);
//    functionGrid->addWidget(testBrowser, 4, 0, 1, 4);

//    vBoxLayout->addWidget(functionWidget);

//    QWidget *bottomWidget = new QWidget();
//    QHBoxLayout *bottomHBox = new QHBoxLayout(bottomWidget);
//    bottomHBox->setSpacing(8);
//    bottomHBox->setContentsMargins(0, 0, 0, 0);

//    QWidget *infoWidget = new QWidget();
//    QVBoxLayout *infoVBox = new QVBoxLayout(infoWidget);
//    infoVBox->setSpacing(5);

//    leftChannelLevelLabel = new QLabel("L: 0%", this);
//    leftChannelLevelLabel->setStyleSheet("QLabel { font-size: 20px; }");
//    infoVBox->addWidget(leftChannelLevelLabel, 0, Qt::AlignLeft);

//    maxLeftChannelLevelLabel = new QLabel("Max: 0%", this);
//    maxLeftChannelLevelLabel->setStyleSheet("QLabel { font-size: 20px; color: #666; }");
//    infoVBox->addWidget(maxLeftChannelLevelLabel, 0, Qt::AlignLeft);

//    modeLabel = new QLabel("滤Max:", this);
//    modeLabel->setStyleSheet("QLabel { font-size: 20px; color: #2c3e50; }");
//    infoVBox->addWidget(modeLabel, 0, Qt::AlignLeft);

//    globalMaxLabel = new QLabel("全局最大滤强: 0%", this);
//    globalMaxLabel->setStyleSheet("QLabel { font-size: 20px; color: #e74c3c; }");
//    infoVBox->addWidget(globalMaxLabel);

//    countLabel = new QLabel("0s", this);
//    countLabel->setStyleSheet("QLabel { font-size: 20px; font-weight: bold; }");
//    infoVBox->addWidget(countLabel);

//    bottomHBox->addWidget(infoWidget);

//    QWidget *commandWidget = new QWidget();
//    QVBoxLayout *commandVBox = new QVBoxLayout(commandWidget);
//    commandVBox->setSpacing(5);
//    commandVBox->setContentsMargins(0, 0, 0, 0);

//    QWidget *topCommandWidget = new QWidget();
//    QHBoxLayout *topCommandHBox = new QHBoxLayout(topCommandWidget);
//    topCommandHBox->setSpacing(5);

//    newLeftBtn = new QPushButton("保存", this);
//    newLeftBtn->setFixedSize(75, 32);
//    topCommandHBox->addWidget(newLeftBtn);
//    connect(newLeftBtn, &QPushButton::clicked, this, [=]() {
//        m_saveBothChannels = !m_saveBothChannels;
//        if (m_saveBothChannels) {
//            newLeftBtn->setText("保存");
//            qDebug() << "开始同时保存左右声道滤波数据（目标5万帧）";
//            m_leftDataBuffer.clear();
//            m_rightDataBuffer.clear();
//            m_bothDataCount = 0;
//        } else {
//            newLeftBtn->setText("保存");
//            qDebug() << "手动停止双声道数据保存";
//        }
//    });

//    m_saveRawBtn = new QPushButton("保原", this);
//    m_saveRawBtn->setFixedSize(50, 32);
//    m_saveRawBtn->setStyleSheet(R"(
//        QPushButton {
//            background-color: #3498db;
//            color: white;
//            border: none;
//            border-radius: 4px;
//        }
//        QPushButton:hover {
//            background-color: #2980b9;
//        }
//    )");
//    topCommandHBox->addWidget(m_saveRawBtn);

//    QPushButton *m_playRawBtn = new QPushButton("播原", this);
//    m_playRawBtn->setFixedSize(50, 32);
//    m_playRawBtn->setStyleSheet(R"(
//        QPushButton {
//            background-color: #2980b9;
//            color: white;
//            border: none;
//            border-radius: 4px;
//        }
//        QPushButton:hover {
//            background-color: #1f618d;
//        }
//    )");
//    topCommandHBox->addWidget(m_playRawBtn);

//    m_saveFilteredBtn = new QPushButton("保滤", this);
//    m_saveFilteredBtn->setFixedSize(50, 32);
//    m_saveFilteredBtn->setStyleSheet(R"(
//        QPushButton {
//            background-color: #2ecc71;
//            color: white;
//            border: none;
//            border-radius: 4px;
//        }
//        QPushButton:hover {
//            background-color: #27ae60;
//        }
//    )");
//    topCommandHBox->addWidget(m_saveFilteredBtn);

//    QPushButton *m_playFilteredBtn = new QPushButton("播滤", this);
//    m_playFilteredBtn->setFixedSize(50, 32);
//    m_playFilteredBtn->setStyleSheet(R"(
//        QPushButton {
//            background-color: #27ae60;
//            color: white;
//            border: none;
//            border-radius: 4px;
//        }
//        QPushButton:hover {
//            background-color: #219653;
//        }
//    )");
//    topCommandHBox->addWidget(m_playFilteredBtn);

//    commandVBox->addWidget(topCommandWidget);

//    QWidget *bottomCommandWidget = new QWidget();
//    QHBoxLayout *bottomCommandHBox = new QHBoxLayout(bottomCommandWidget);
//    bottomCommandHBox->setSpacing(5);

//    m_restorePcmBtn = new QPushButton("GPS", this);
//    m_restorePcmBtn->setFixedSize(75, 32);
//    m_restorePcmBtn->setStyleSheet(R"(
//        QPushButton {
//            background-color: #f39c12;
//            color: white;
//            border: none;
//            border-radius: 4px;
//        }
//        QPushButton:hover {
//            background-color: #e67e22;
//        }
//    )");
//    bottomCommandHBox->addWidget(m_restorePcmBtn);

//    // 复用GPS按钮，改为「保存日志」功能（layoutInit()函数内）
//    connect(m_restorePcmBtn, &QPushButton::clicked, this, [=]() {
//        // 1. 获取textBrowser中的所有日志内容（包括历史记录）
//        QString allLogText = testBrowser->toPlainText();

//        // 2. 处理空日志情况
//        if (allLogText.isEmpty()) {
//            testBrowser->append("[日志保存]：日志为空，无需保存！");
//            testBrowser->moveCursor(QTextCursor::End);
//            return;
//        }

//        // 3. 创建日志保存目录（LogFiles），不存在则自动创建
//        QString logDirPath = QCoreApplication::applicationDirPath() + "/LogFiles";
//        QDir logDir(logDirPath);
//        if (!logDir.exists()) {
//            if (logDir.mkpath(logDirPath)) {
//                testBrowser->append("[日志保存]：创建日志目录成功：" + logDirPath);
//            } else {
//                testBrowser->append("[日志保存失败]：创建日志目录失败！");
//                testBrowser->moveCursor(QTextCursor::End);
//                return;
//            }
//        }

//        // 4. 生成带时间戳的文件名（避免覆盖旧日志）
//        QString timeStamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
//        QString logFileName = "AudioLog_" + timeStamp + ".txt";
//        QString logFilePath = logDirPath + "/" + logFileName;

//        // 5. 写入日志文件
//        QFile logFile(logFilePath);
//        if (logFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
//            QTextStream out(&logFile);
//            // 写入文件头（增强可读性）
//            out << "===== 音频工具日志文件 =====\n";
//            out << "生成时间：" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
//            out << "日志总行数：" << allLogText.count("\n") + 1 << "\n";
//            out << "===========================\n\n";
//            // 写入所有日志内容
//            out << allLogText;
//            // 关闭文件
//            logFile.close();

//            // 6. 保存成功反馈
//            testBrowser->append("\n[日志保存成功]：" + logFilePath);
//            testBrowser->append("----------------------------------");
//            testBrowser->moveCursor(QTextCursor::End);
//        } else {
//            // 保存失败反馈
//            testBrowser->append("[日志保存失败]：无法打开文件：" + logFile.errorString());
//            testBrowser->append("----------------------------------");
//            testBrowser->moveCursor(QTextCursor::End);
//        }
//    });

//    QPushButton *barChartBtn = new QPushButton("柱状图", this);
//    barChartBtn->setFixedSize(75, 32);
//    barChartBtn->setStyleSheet(R"(
//        QPushButton {
//            background-color: #9b59b6;
//            color: white;
//            border: none;
//            border-radius: 4px;
//        }
//        QPushButton:hover {
//            background-color: #8e44ad;
//        }
//    )");
//    bottomCommandHBox->addWidget(barChartBtn);

//    // 在layoutInit()函数中，找到barChartBtn的点击事件连接
//    connect(barChartBtn, &QPushButton::clicked, this, [=]() {
//        if (!m_barChartWindow) {
//            m_barChartWindow = new BarChartMainWindow(this);
//            connect(m_barChartWindow, &BarChartMainWindow::destroyed, this, [=]() {
//                this->show();
//                m_barChartWindow = nullptr;
//            });

//            // 新增：连接NewWindow的滤波强度信号到BarChartMainWindow
//            if (m_newWindow) {  // 若NewWindow已创建
//                connect(m_newWindow, &NewWindow::filteredIntensityUpdated,
//                        m_barChartWindow, &BarChartMainWindow::setGlobalMaxFilteredData);
//            }
//        }

//        // 原有的清除全局最大值连接（保留）
//        connect(m_barChartWindow, &BarChartMainWindow::clearGlobalMaxFilteredIntensity,
//                this, [=]() {
//            m_globalMaxFilteredIntensity = 0.0;
//            globalMaxLabel->setText("全局最大滤强: 0%");
//        });

//        m_barChartWindow->show();
//        this->hide();
//    });

//    commandBt1 = new QPushButton("泄漏", this);
//    commandBt1->setFixedSize(75, 32);
//    commandBt1->setStyleSheet(R"(
//        QPushButton {
//            background-color: #e74c3c;
//            color: white;
//            border: none;
//            border-radius: 4px;
//        }
//        QPushButton:hover {
//            background-color: #c0392b;
//        }
//    )");
//    bottomCommandHBox->addWidget(commandBt1);

//    commandBt2 = new QPushButton("清Max", this);
//    commandBt2->setFixedSize(75, 32);
//    commandBt2->setStyleSheet(R"(
//        QPushButton {
//            background-color: #95a5a6;
//            color: white;
//            border: none;
//            border-radius: 4px;
//        }
//        QPushButton:hover {
//            background-color: #7f8c8d;
//        }
//    )");
//    bottomCommandHBox->addWidget(commandBt2);

//    commandBt3 = new QPushButton("频+", this);
//    commandBt3->setFixedSize(55, 32);
//    commandBt3->setStyleSheet(R"(
//        QPushButton {
//            background-color: #1abc9c;
//            color: white;
//            border: none;
//            border-radius: 4px;
//        }
//        QPushButton:hover {
//            background-color: #16a085;
//        }
//    )");
//    bottomCommandHBox->addWidget(commandBt3);

//    commandBt6 = new QPushButton("频-", this);
//    commandBt6->setFixedSize(55, 32);
//    commandBt6->setStyleSheet(R"(
//        QPushButton {
//            background-color: #16a085;
//            color: white;
//            border: none;
//            border-radius: 4px;
//        }
//        QPushButton:hover {
//            background-color: #138d75;
//        }
//    )");
//    bottomCommandHBox->addWidget(commandBt6);

//    commandVBox->addWidget(bottomCommandWidget);

//    bottomHBox->addWidget(commandWidget);

//    connect(m_saveRawBtn, &QPushButton::clicked, this, &AudioRecorder::onSaveRawBtnClicked);
//    connect(m_playRawBtn, &QPushButton::clicked, this, &AudioRecorder::onPlayRawBtnClicked);
//    connect(m_saveFilteredBtn, &QPushButton::clicked, this, &AudioRecorder::onSaveFilteredBtnClicked);
//    connect(m_playFilteredBtn, &QPushButton::clicked, this, &AudioRecorder::onPlayFilteredBtnClicked);
//    connect(recorderBt, &QPushButton::clicked, this, &AudioRecorder::recorderBtClicked);
//    connect(commandBt1, &QPushButton::clicked, this, &AudioRecorder::commandBt1Clicked);
//    connect(commandBt2, &QPushButton::clicked, this, &AudioRecorder::commandBt2Clicked);
//    connect(commandBt3, &QPushButton::clicked, this, &AudioRecorder::commandBt3Clicked);
//    connect(commandBt6, &QPushButton::clicked, this, &AudioRecorder::commandBt6Clicked);

//    connect(executeBt, &QPushButton::clicked, this, &AudioRecorder::executeBtClicked);
//    connect(newBtn1, &QPushButton::clicked, this, &AudioRecorder::newBtn1Clicked);
//    connect(newBtn2, &QPushButton::clicked, this, &AudioRecorder::newBtn2Clicked);
//    connect(newBtn3, &QPushButton::clicked, this, &AudioRecorder::newBtn3Clicked);
//    connect(newBtn4, &QPushButton::clicked, this, &AudioRecorder::newBtn4Clicked);
//    connect(hSlider, &QSlider::valueChanged, this, &AudioRecorder::updateHValueFromSlider);

//    vBoxLayout->addWidget(bottomWidget);
//    vBoxLayout->addStretch(0);

//    m_decreaseLongPressTimer = new QTimer(this);
//    m_decreaseLongPressTimer->setInterval(50);
//    m_decreaseLongPressTimer->setSingleShot(false);

//    m_increaseLongPressTimer = new QTimer(this);
//    m_increaseLongPressTimer->setInterval(50);
//    m_increaseLongPressTimer->setSingleShot(false);

//    m_volumeDownLongPressTimer = new QTimer(this);
//    m_volumeDownLongPressTimer->setInterval(100);
//    m_volumeDownLongPressTimer->setSingleShot(false);

//    m_volumeUpLongPressTimer = new QTimer(this);
//    m_volumeUpLongPressTimer->setInterval(100);
//    m_volumeUpLongPressTimer->setSingleShot(false);

//    currentValue = 255;

//    connect(decreaseValueBtn, &QPushButton::pressed, this, [=]() {
//        if (currentValue > 0) {
//            currentValue--;
//            valueDisplayLabel->setText(QString::number(currentValue));
//            updateVerticalSliderValue(currentValue);
//        }
//        m_decreaseLongPressTimer->start();
//    });
//    connect(decreaseValueBtn, &QPushButton::released, m_decreaseLongPressTimer, &QTimer::stop);
//    connect(m_decreaseLongPressTimer, &QTimer::timeout, this, [=]() {
//        if (currentValue > 0) {
//            currentValue--;
//            valueDisplayLabel->setText(QString::number(currentValue));
//            updateVerticalSliderValue(currentValue);
//        } else {
//            m_decreaseLongPressTimer->stop();
//        }
//    });

//    connect(increaseValueBtn, &QPushButton::pressed, this, [=]() {
//        if (currentValue < 255) {
//            currentValue++;
//            valueDisplayLabel->setText(QString::number(currentValue));
//            updateVerticalSliderValue(currentValue);
//        }
//        m_increaseLongPressTimer->start();
//    });
//    connect(increaseValueBtn, &QPushButton::released, m_increaseLongPressTimer, &QTimer::stop);
//    connect(m_increaseLongPressTimer, &QTimer::timeout, this, [=]() {
//        if (currentValue < 255) {
//            currentValue++;
//            valueDisplayLabel->setText(QString::number(currentValue));
//            updateVerticalSliderValue(currentValue);
//        } else {
//            m_increaseLongPressTimer->stop();
//        }
//    });

//    connect(volumeDownBtn, &QPushButton::pressed, this, [=]() {
//        int currentVolume = hSlider->value();
//        if (currentVolume > 0) {
//            hSlider->setValue(currentVolume - 1);
//        }
//        m_volumeDownLongPressTimer->start();
//    });
//    connect(volumeDownBtn, &QPushButton::released, m_volumeDownLongPressTimer, &QTimer::stop);
//    connect(m_volumeDownLongPressTimer, &QTimer::timeout, this, [=]() {
//        int currentVolume = hSlider->value();
//        if (currentVolume > 0) {
//            hSlider->setValue(currentVolume - 1);
//        } else {
//            m_volumeDownLongPressTimer->stop();
//        }
//    });

//    connect(volumeUpBtn, &QPushButton::pressed, this, [=]() {
//        int currentVolume = hSlider->value();
//        if (currentVolume < 127) {
//            hSlider->setValue(currentVolume + 1);
//        }
//        m_volumeUpLongPressTimer->start();
//    });
//    connect(volumeUpBtn, &QPushButton::released, m_volumeUpLongPressTimer, &QTimer::stop);
//    connect(m_volumeUpLongPressTimer, &QTimer::timeout, this, [=]() {
//        int currentVolume = hSlider->value();
//        if (currentVolume < 127) {
//            hSlider->setValue(currentVolume + 1);
//        } else {
//            m_volumeUpLongPressTimer->stop();
//        }
//    });

//    connect(hSlider, &QSlider::valueChanged, this, &AudioRecorder::updateHValueFromSlider);
//}


void AudioRecorder::layoutInit()
{
    this->setGeometry(100, 100, 800, 480);
    this->setWindowTitle("音频调节工具");

    mainWidget = new QWidget(this);
    setCentralWidget(mainWidget);

    QVBoxLayout *vBoxLayout = new QVBoxLayout(mainWidget);
    vBoxLayout->setContentsMargins(5, 5, 5, 5);
    vBoxLayout->setSpacing(8);

    // ========== 顶部控制行（topControlWidget）的创建逻辑保持不变 ==========
    QWidget *topControlWidget = new QWidget();
    QHBoxLayout *topHBox = new QHBoxLayout(topControlWidget);
    topHBox->setSpacing(5);

    QWidget *leftControlWidget = new QWidget();
    QHBoxLayout *leftHBox = new QHBoxLayout(leftControlWidget);
    leftHBox->setSpacing(5);

    recorderBt = new QPushButton("启动", this);
    recorderBt->setFixedSize(90, 32);
    leftHBox->addWidget(recorderBt);

    executeBt = new QPushButton("返回", this);
    executeBt->setFixedSize(90, 32);
    leftHBox->addWidget(executeBt);

    topHBox->addWidget(leftControlWidget);

    QWidget *aControlWidget = new QWidget();
    QHBoxLayout *aControlLayout = new QHBoxLayout(aControlWidget);
    aControlLayout->setSpacing(2);

    // ========== 重点优化：valueControlWidget 布局（保持不变） ==========
    QWidget *valueControlWidget = new QWidget();
    QHBoxLayout *valueHBox = new QHBoxLayout(valueControlWidget);
    valueHBox->setContentsMargins(10, 0, 10, 0);
    valueHBox->setSpacing(8);
    valueHBox->setAlignment(Qt::AlignCenter);

    valueDisplayLabel = new QLabel("255", this);
    valueDisplayLabel->setFixedSize(50, 35);
    valueDisplayLabel->setStyleSheet(R"(
        QLabel {
            background: white;
            border: 1px solid #ccc;
            border-radius: 4px;
            text-align: center;
            font-size: 14px;
            font-weight: 500;
        }
    )");
    valueDisplayLabel->setAlignment(Qt::AlignCenter);
    valueHBox->addWidget(valueDisplayLabel);

    m_valueLineEdit = new QLineEdit(this);
    m_valueLineEdit->setFixedSize(120, 35);
    m_valueLineEdit->setStyleSheet(R"(
        QLineEdit {
            background: white;
            border: 1px solid #ccc;
            border-radius: 4px;
            padding: 0 8px;
            text-align: center;
            font-size: 14px;
            font-weight: 500;
            color: #333;
        }
        QLineEdit:hover {
            border-color: #66afe9;
        }
        QLineEdit:focus {
            border-color: #4ecdc4;
            outline: none;
        }
    )");
    m_valueLineEdit->setPlaceholderText("输入调节值 (0-255)");
    m_valueLineEdit->setAlignment(Qt::AlignCenter);
    valueHBox->addWidget(m_valueLineEdit);

    decreaseValueBtn = new QPushButton("-", this);
    decreaseValueBtn->setFixedSize(40, 35);
    decreaseValueBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #ff6b6b;
            color: white;
            border: none;
            border-radius: 4px;
            font-size: 16px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #ff5252;
        }
        QPushButton:pressed {
            background-color: #d32f2f;
        }
    )");
    valueHBox->addWidget(decreaseValueBtn);

    increaseValueBtn = new QPushButton("+", this);
    increaseValueBtn->setFixedSize(40, 35);
    increaseValueBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #4ecdc4;
            color: white;
            border: none;
            border-radius: 4px;
            font-size: 16px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #26a69a;
        }
        QPushButton:pressed {
            background-color: #00897b;
        }
    )");
    valueHBox->addWidget(increaseValueBtn);

    valueControlWidget->setStyleSheet(R"(
        QWidget {
            background-color: #f9f9f9;
            border: 1px solid #eee;
            border-radius: 6px;
            padding: 5px;
        }
    )");
    topHBox->addWidget(valueControlWidget);
    // ========== valueControlWidget 优化结束 ==========

    // ========== 关键修改：给topControlWidget添加顶部空白间距 ==========
    // 方式1：通过布局的addSpacing添加固定空白（推荐，可自定义距离，比如30px）
    vBoxLayout->addSpacing(30); // 顶部添加30px空白，topControlWidget会向下移动
    // 方式2（可选）：调整topControlWidget自身的上边距
    // topControlWidget->setContentsMargins(0, 30, 0, 0);

    // 将topControlWidget添加到垂直布局
    vBoxLayout->addWidget(topControlWidget);

    // ========== 以下代码全部保持不变 ==========
    QWidget *functionWidget = new QWidget();
    QGridLayout *functionGrid = new QGridLayout(functionWidget);
    functionGrid->setHorizontalSpacing(5);
    functionGrid->setVerticalSpacing(8);

    QString textStyle = "QLabel { font-size: 20px; background: white; border: 1px solid #999; border-radius: 2px; text-align: center; }";

    newBtn1 = new QPushButton("远程", this);
    newBtn1->setFixedSize(80, 22);
    functionGrid->addWidget(newBtn1, 2, 0);

    newBtn2 = new QPushButton("获取", this);
    newBtn2->setFixedSize(80, 22);
    functionGrid->addWidget(newBtn2, 2, 1);

    newBtn3 = new QPushButton("频率", this);
    newBtn3->setFixedSize(80, 22);
    functionGrid->addWidget(newBtn3, 2, 2);

    newBtn4 = new QPushButton("delete", this);
    newBtn4->setFixedSize(80, 22);
    functionGrid->addWidget(newBtn4, 2, 3);

    QWidget *volumeControlWidget = new QWidget();
    QHBoxLayout *volumeHBox = new QHBoxLayout(volumeControlWidget);
    volumeHBox->setContentsMargins(0, 0, 0, 0);
    volumeHBox->setSpacing(5);

    volumeDownBtn = new QPushButton("-", this);
    volumeDownBtn->setFixedSize(80, 60);
    volumeHBox->addWidget(volumeDownBtn);

    hSlider = new QSlider(Qt::Horizontal, this);
    hSlider->setRange(0, 127);
    hSlider->setValue(64);
    hSlider->setTickPosition(QSlider::NoTicks);
    hSlider->setStyleSheet(R"(
                           QSlider::groove:horizontal { height: 12px; background: #f0f0f0; border-radius: 6px; }
                           QSlider::handle:horizontal { width: 20px; background: #ff5555; margin: -4px 0; border-radius: 10px; }
                           )");
    volumeHBox->addWidget(hSlider);

    volumeUpBtn = new QPushButton("+", this);
    volumeUpBtn->setFixedSize(80, 60);
    volumeHBox->addWidget(volumeUpBtn);

    functionGrid->addWidget(volumeControlWidget, 3, 0, 1, 4);

    testBrowser = new QTextBrowser(this);
    testBrowser->setStyleSheet(R"(
                               border: 1px solid #ccc;
                               border-radius: 4px;
                               background-color: #f8f8f8;
                               font-size: 20px;
                               padding: 8px;
                               )");
    testBrowser->setMinimumHeight(80);
    testBrowser->setMaximumHeight(100);
    testBrowser->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    testBrowser->setReadOnly(true);

    QString defaultText = "当前音量: 64/127\n";
    testBrowser->setPlainText(defaultText);

    functionGrid->setRowStretch(4, 1);
    functionGrid->addWidget(testBrowser, 4, 0, 1, 4);

    vBoxLayout->addWidget(functionWidget);

    QWidget *bottomWidget = new QWidget();
    QHBoxLayout *bottomHBox = new QHBoxLayout(bottomWidget);
    bottomHBox->setSpacing(8);
    bottomHBox->setContentsMargins(0, 0, 0, 0);

    QWidget *infoWidget = new QWidget();
    QVBoxLayout *infoVBox = new QVBoxLayout(infoWidget);
    infoVBox->setSpacing(5);

    leftChannelLevelLabel = new QLabel("L: 0%", this);
    leftChannelLevelLabel->setStyleSheet("QLabel { font-size: 20px; }");
    infoVBox->addWidget(leftChannelLevelLabel, 0, Qt::AlignLeft);

    maxLeftChannelLevelLabel = new QLabel("Max: 0%", this);
    maxLeftChannelLevelLabel->setStyleSheet("QLabel { font-size: 20px; color: #666; }");
    infoVBox->addWidget(maxLeftChannelLevelLabel, 0, Qt::AlignLeft);

    modeLabel = new QLabel("滤Max:", this);
    modeLabel->setStyleSheet("QLabel { font-size: 20px; color: #2c3e50; }");
    infoVBox->addWidget(modeLabel, 0, Qt::AlignLeft);

    globalMaxLabel = new QLabel("全局最大滤强: 0%", this);
    globalMaxLabel->setStyleSheet("QLabel { font-size: 20px; color: #e74c3c; }");
    infoVBox->addWidget(globalMaxLabel);

    countLabel = new QLabel("0s", this);
    countLabel->setStyleSheet("QLabel { font-size: 20px; font-weight: bold; }");
    infoVBox->addWidget(countLabel);

    bottomHBox->addWidget(infoWidget);

    QWidget *commandWidget = new QWidget();
    QVBoxLayout *commandVBox = new QVBoxLayout(commandWidget);
    commandVBox->setSpacing(5);
    commandVBox->setContentsMargins(0, 0, 0, 0);

    QWidget *topCommandWidget = new QWidget();
    QHBoxLayout *topCommandHBox = new QHBoxLayout(topCommandWidget);
    topCommandHBox->setSpacing(5);

    newLeftBtn = new QPushButton("保存", this);
    newLeftBtn->setFixedSize(75, 32);
    topCommandHBox->addWidget(newLeftBtn);
    connect(newLeftBtn, &QPushButton::clicked, this, [=]() {
        m_saveBothChannels = !m_saveBothChannels;
        if (m_saveBothChannels) {
            newLeftBtn->setText("保存");
            qDebug() << "开始同时保存左右声道滤波数据（目标5万帧）";
            m_leftDataBuffer.clear();
            m_rightDataBuffer.clear();
            m_bothDataCount = 0;
        } else {
            newLeftBtn->setText("保存");
            qDebug() << "手动停止双声道数据保存";
        }
    });

    m_saveRawBtn = new QPushButton("保原", this);
    m_saveRawBtn->setFixedSize(50, 32);
    m_saveRawBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #3498db;
            color: white;
            border: none;
            border-radius: 4px;
        }
        QPushButton:hover {
            background-color: #2980b9;
        }
    )");
    topCommandHBox->addWidget(m_saveRawBtn);

    QPushButton *m_playRawBtn = new QPushButton("播原", this);
    m_playRawBtn->setFixedSize(50, 32);
    m_playRawBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #2980b9;
            color: white;
            border: none;
            border-radius: 4px;
        }
        QPushButton:hover {
            background-color: #1f618d;
        }
    )");
    topCommandHBox->addWidget(m_playRawBtn);

    m_saveFilteredBtn = new QPushButton("保滤", this);
    m_saveFilteredBtn->setFixedSize(50, 32);
    m_saveFilteredBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #2ecc71;
            color: white;
            border: none;
            border-radius: 4px;
        }
        QPushButton:hover {
            background-color: #27ae60;
        }
    )");
    topCommandHBox->addWidget(m_saveFilteredBtn);

    QPushButton *m_playFilteredBtn = new QPushButton("播滤", this);
    m_playFilteredBtn->setFixedSize(50, 32);
    m_playFilteredBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #27ae60;
            color: white;
            border: none;
            border-radius: 4px;
        }
        QPushButton:hover {
            background-color: #219653;
        }
    )");
    topCommandHBox->addWidget(m_playFilteredBtn);

    commandVBox->addWidget(topCommandWidget);

    QWidget *bottomCommandWidget = new QWidget();
    QHBoxLayout *bottomCommandHBox = new QHBoxLayout(bottomCommandWidget);
    bottomCommandHBox->setSpacing(5);

    m_restorePcmBtn = new QPushButton("GPS", this);
    m_restorePcmBtn->setFixedSize(75, 32);
    m_restorePcmBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #f39c12;
            color: white;
            border: none;
            border-radius: 4px;
        }
        QPushButton:hover {
            background-color: #e67e22;
        }
    )");
    bottomCommandHBox->addWidget(m_restorePcmBtn);

    connect(m_restorePcmBtn, &QPushButton::clicked, this, [=]() {
        QString allLogText = testBrowser->toPlainText();
        if (allLogText.isEmpty()) {
            testBrowser->append("[日志保存]：日志为空，无需保存！");
            testBrowser->moveCursor(QTextCursor::End);
            return;
        }
        QString logDirPath = QCoreApplication::applicationDirPath() + "/LogFiles";
        QDir logDir(logDirPath);
        if (!logDir.exists()) {
            if (logDir.mkpath(logDirPath)) {
                testBrowser->append("[日志保存]：创建日志目录成功：" + logDirPath);
            } else {
                testBrowser->append("[日志保存失败]：创建日志目录失败！");
                testBrowser->moveCursor(QTextCursor::End);
                return;
            }
        }
        QString timeStamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        QString logFileName = "AudioLog_" + timeStamp + ".txt";
        QString logFilePath = logDirPath + "/" + logFileName;

        QFile logFile(logFilePath);
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            QTextStream out(&logFile);
            out << "===== 音频工具日志文件 =====\n";
            out << "生成时间：" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
            out << "日志总行数：" << allLogText.count("\n") + 1 << "\n";
            out << "===========================\n\n";
            out << allLogText;
            logFile.close();

            testBrowser->append("\n[日志保存成功]：" + logFilePath);
            testBrowser->append("----------------------------------");
            testBrowser->moveCursor(QTextCursor::End);
        } else {
            testBrowser->append("[日志保存失败]：无法打开文件：" + logFile.errorString());
            testBrowser->append("----------------------------------");
            testBrowser->moveCursor(QTextCursor::End);
        }
    });

    QPushButton *barChartBtn = new QPushButton("柱状图", this);
    barChartBtn->setFixedSize(75, 32);
    barChartBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #9b59b6;
            color: white;
            border: none;
            border-radius: 4px;
        }
        QPushButton:hover {
            background-color: #8e44ad;
        }
    )");
    bottomCommandHBox->addWidget(barChartBtn);

    connect(barChartBtn, &QPushButton::clicked, this, [=]() {
        if (!m_barChartWindow) {
            m_barChartWindow = new BarChartMainWindow(this);
            connect(m_barChartWindow, &BarChartMainWindow::destroyed, this, [=]() {
                this->show();
                m_barChartWindow = nullptr;
            });
            if (m_newWindow) {
                connect(m_newWindow, &NewWindow::filteredIntensityUpdated,
                        m_barChartWindow, &BarChartMainWindow::setGlobalMaxFilteredData);
            }
        }
        connect(m_barChartWindow, &BarChartMainWindow::clearGlobalMaxFilteredIntensity,
                this, [=]() {
            m_globalMaxFilteredIntensity = 0.0;
            globalMaxLabel->setText("全局最大滤强: 0%");
        });
        m_barChartWindow->show();
        this->hide();
    });

    commandBt1 = new QPushButton("泄漏", this);
    commandBt1->setFixedSize(75, 32);
    commandBt1->setStyleSheet(R"(
        QPushButton {
            background-color: #e74c3c;
            color: white;
            border: none;
            border-radius: 4px;
        }
        QPushButton:hover {
            background-color: #c0392b;
        }
    )");
    bottomCommandHBox->addWidget(commandBt1);

    commandBt2 = new QPushButton("清Max", this);
    commandBt2->setFixedSize(75, 32);
    commandBt2->setStyleSheet(R"(
        QPushButton {
            background-color: #95a5a6;
            color: white;
            border: none;
            border-radius: 4px;
        }
        QPushButton:hover {
            background-color: #7f8c8d;
        }
    )");
    bottomCommandHBox->addWidget(commandBt2);

    commandBt3 = new QPushButton("频+", this);
    commandBt3->setFixedSize(55, 32);
    commandBt3->setStyleSheet(R"(
        QPushButton {
            background-color: #1abc9c;
            color: white;
            border: none;
            border-radius: 4px;
        }
        QPushButton:hover {
            background-color: #16a085;
        }
    )");
    bottomCommandHBox->addWidget(commandBt3);

    commandBt6 = new QPushButton("频-", this);
    commandBt6->setFixedSize(55, 32);
    commandBt6->setStyleSheet(R"(
        QPushButton {
            background-color: #16a085;
            color: white;
            border: none;
            border-radius: 4px;
        }
        QPushButton:hover {
            background-color: #138d75;
        }
    )");
    bottomCommandHBox->addWidget(commandBt6);

    commandVBox->addWidget(bottomCommandWidget);

    bottomHBox->addWidget(commandWidget);

    connect(m_saveRawBtn, &QPushButton::clicked, this, &AudioRecorder::onSaveRawBtnClicked);
    connect(m_playRawBtn, &QPushButton::clicked, this, &AudioRecorder::onPlayRawBtnClicked);
    connect(m_saveFilteredBtn, &QPushButton::clicked, this, &AudioRecorder::onSaveFilteredBtnClicked);
    connect(m_playFilteredBtn, &QPushButton::clicked, this, &AudioRecorder::onPlayFilteredBtnClicked);
    connect(recorderBt, &QPushButton::clicked, this, &AudioRecorder::recorderBtClicked);
    connect(commandBt1, &QPushButton::clicked, this, &AudioRecorder::commandBt1Clicked);
    connect(commandBt2, &QPushButton::clicked, this, &AudioRecorder::commandBt2Clicked);
    connect(commandBt3, &QPushButton::clicked, this, &AudioRecorder::commandBt3Clicked);
    connect(commandBt6, &QPushButton::clicked, this, &AudioRecorder::commandBt6Clicked);

    connect(executeBt, &QPushButton::clicked, this, &AudioRecorder::executeBtClicked);
    connect(newBtn1, &QPushButton::clicked, this, &AudioRecorder::newBtn1Clicked);
    connect(newBtn2, &QPushButton::clicked, this, &AudioRecorder::newBtn2Clicked);
    connect(newBtn3, &QPushButton::clicked, this, &AudioRecorder::newBtn3Clicked);
    connect(newBtn4, &QPushButton::clicked, this, &AudioRecorder::newBtn4Clicked);
    connect(hSlider, &QSlider::valueChanged, this, &AudioRecorder::updateHValueFromSlider);

    vBoxLayout->addWidget(bottomWidget);
    vBoxLayout->addStretch(0);

    m_decreaseLongPressTimer = new QTimer(this);
    m_decreaseLongPressTimer->setInterval(50);
    m_decreaseLongPressTimer->setSingleShot(false);

    m_increaseLongPressTimer = new QTimer(this);
    m_increaseLongPressTimer->setInterval(50);
    m_increaseLongPressTimer->setSingleShot(false);

    m_volumeDownLongPressTimer = new QTimer(this);
    m_volumeDownLongPressTimer->setInterval(50);
    m_volumeDownLongPressTimer->setSingleShot(false);

    m_volumeUpLongPressTimer = new QTimer(this);
    m_volumeUpLongPressTimer->setInterval(50);
    m_volumeUpLongPressTimer->setSingleShot(false);

    currentValue = 255;

    connect(decreaseValueBtn, &QPushButton::pressed, this, [=]() {
        if (currentValue > 0) {
            currentValue--;
            valueDisplayLabel->setText(QString::number(currentValue));
            updateVerticalSliderValue(currentValue);
        }
        m_decreaseLongPressTimer->start();
    });
    connect(decreaseValueBtn, &QPushButton::released, m_decreaseLongPressTimer, &QTimer::stop);
    connect(m_decreaseLongPressTimer, &QTimer::timeout, this, [=]() {
        if (currentValue > 0) {
            currentValue--;
            valueDisplayLabel->setText(QString::number(currentValue));
            updateVerticalSliderValue(currentValue);
        } else {
            m_decreaseLongPressTimer->stop();
        }
    });

    connect(increaseValueBtn, &QPushButton::pressed, this, [=]() {
        if (currentValue < 255) {
            currentValue++;
            valueDisplayLabel->setText(QString::number(currentValue));
            updateVerticalSliderValue(currentValue);
        }
        m_increaseLongPressTimer->start();
    });
    connect(increaseValueBtn, &QPushButton::released, m_increaseLongPressTimer, &QTimer::stop);
    connect(m_increaseLongPressTimer, &QTimer::timeout, this, [=]() {
        if (currentValue < 255) {
            currentValue++;
            valueDisplayLabel->setText(QString::number(currentValue));
            updateVerticalSliderValue(currentValue);
        } else {
            m_increaseLongPressTimer->stop();
        }
    });

    connect(volumeDownBtn, &QPushButton::pressed, this, [=]() {
        int currentVolume = hSlider->value();
        if (currentVolume > 0) {
            hSlider->setValue(currentVolume - 5);
        }
        m_volumeDownLongPressTimer->start();
    });
    connect(volumeDownBtn, &QPushButton::released, m_volumeDownLongPressTimer, &QTimer::stop);
    connect(m_volumeDownLongPressTimer, &QTimer::timeout, this, [=]() {
        int currentVolume = hSlider->value();
        if (currentVolume > 0) {
            hSlider->setValue(currentVolume - 5);
        } else {
            m_volumeDownLongPressTimer->stop();
        }
    });

    connect(volumeUpBtn, &QPushButton::pressed, this, [=]() {
        int currentVolume = hSlider->value();
        if (currentVolume < 127) {
            hSlider->setValue(currentVolume + 5);
        }
        m_volumeUpLongPressTimer->start();
    });
    connect(volumeUpBtn, &QPushButton::released, m_volumeUpLongPressTimer, &QTimer::stop);
    connect(m_volumeUpLongPressTimer, &QTimer::timeout, this, [=]() {
        int currentVolume = hSlider->value();
        if (currentVolume < 127) {
            hSlider->setValue(currentVolume + 5);
        } else {
            m_volumeUpLongPressTimer->stop();
        }
    });

    connect(hSlider, &QSlider::valueChanged, this, &AudioRecorder::updateHValueFromSlider);
}



void AudioRecorder::setupAudio()
{
    // 定义音频格式
    QAudioFormat format;
    format.setSampleRate(44100);                  // 采样率
    format.setChannelCount(2);                    // 声道数
    format.setSampleSize(16);                     // 每个样本的位数
    format.setCodec("audio/pcm");                 // 编码格式
    format.setByteOrder(QAudioFormat::LittleEndian);// 字节序
    format.setSampleType(QAudioFormat::SignedInt); // 样本类型

    // 检查系统是否支持该格式
    QAudioDeviceInfo inputInfo = QAudioDeviceInfo::defaultInputDevice();
    if (!inputInfo.isFormatSupported(format)) {
        qWarning() << "默认音频输入格式不受支持，尝试使用最接近的格式。";
        format = inputInfo.nearestFormat(format);
    }

    QAudioDeviceInfo outputInfo = QAudioDeviceInfo::defaultOutputDevice();
    if (!outputInfo.isFormatSupported(format)) {
        qWarning() << "默认音频输出格式不受支持，尝试使用最接近的格式。";
        format = outputInfo.nearestFormat(format);
    }

    // 创建音频输入和输出对象
    m_audioInput = new QAudioInput(inputInfo, format, this);
    m_audioOutput = new QAudioOutput(outputInfo, format, this);

    // 设置缓冲大小和通知间隔
    m_audioInput->setBufferSize(4096);
    m_audioInput->setNotifyInterval(5000); // 毫秒

    // 创建定时器用于更新时间
    recordTimer = new QTimer(this);
    connect(recordTimer, &QTimer::timeout, this, &AudioRecorder::updateProgress);
}

void AudioRecorder::recorderBtClicked()
{
    if (!isRecording) {
        // 开始录音和播放
        m_inputDevice = m_audioInput->start();
        if (!m_inputDevice) {
            qWarning() << "无法启动音频输入设备。";
            return;
        }

        m_outputDevice = m_audioOutput->start();
        if (!m_outputDevice) {
            qWarning() << "无法启动音频输出设备。";
            m_audioInput->stop();
            return;
        }

        // 连接 readyRead 信号从 m_inputDevice 而不是 m_audioInput
        connect(m_inputDevice, &QIODevice::readyRead, this, &AudioRecorder::handleAudioInput);

        // 显示录音时长标签
        countLabel->setText("已录制 0 s");
        recordTimer->start(1000); // 每秒更新时间

        recorderBt->setText("停止");
        isRecording = true;
    }

}

void AudioRecorder::executeBtClicked()
{
    // 创建 QProcess 对象
    QString program = "./jqv1.2 RPMsg_UART_CM4.elf";

    // 启动外部程序，使用 startDetached 保证外部程序独立运行
    QProcess::startDetached(program);

    // 退出当前程序
    QCoreApplication::quit();

}


// 新增：保存原始PCM数据到TXT文件（限制30万个样本点）
void AudioRecorder::saveRawDataToTxt(const QByteArray &buffer)
{
    // 静态计数器，记录已保存的总样本数（跨函数调用保持值）
    static int totalSavedSamples = 0;
    // 最大保存样本数（30万）
    const int MAX_SAMPLES = 150000;

    // 如果已达到最大样本数，直接返回
    if (totalSavedSamples >= MAX_SAMPLES) {
        return;
    }

    QAudioFormat format = m_audioInput->format();
    if (!format.isValid())
        return;

    // 打开文件（追加模式），如果文件不存在则创建
    QFile file("raw_audio_data.txt");
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        qWarning() << "无法打开文件用于写入:" << file.errorString();
        return;
    }

    QTextStream out(&file);

    int bytesPerSample = format.sampleSize() / 8;
    int channelCount = format.channelCount();
    int frameSize = bytesPerSample * channelCount;
    int frameCount = buffer.size() / frameSize;

    const char *data = buffer.constData();

    // 计算本次可写入的最大帧数（每个帧包含所有声道的一个样本）
    int remainingSamples = MAX_SAMPLES - totalSavedSamples;
    int writeFrames = qMin(frameCount, remainingSamples);

    // 写入每个采样点的原始值（限制在剩余可写入数量内）
    for (int i = 0; i < writeFrames; ++i) {
        out << "帧 " << (totalSavedSamples + i) << ": ";
        for (int ch = 0; ch < channelCount; ++ch) {
            if (format.sampleType() == QAudioFormat::SignedInt) {
                if (bytesPerSample == 2) { // 16-bit 有符号整数
                    qint16 value;
                    memcpy(&value, data + i * frameSize + ch * bytesPerSample, bytesPerSample);
                    out << "声道" << ch << "=" << value << " ";
                }
                // 可以添加对8-bit、32-bit等其他格式的支持
            }
            // 可以添加对无符号整数、浮点数等样本类型的支持
        }
        out << endl;
    }

    // 更新已保存样本数
    totalSavedSamples += writeFrames;

    // 关闭文件
    file.close();

    // 当达到最大样本数时输出提示
    if (totalSavedSamples >= MAX_SAMPLES) {
        qDebug() << "已达到最大保存样本数（" << MAX_SAMPLES << "），停止写入数据";
        testBrowser->append(QString("已保存30万个数据样本，停止继续保存"));
    }
}




//void AudioRecorder::commandBt1Clicked()
//{
//    if (!m_newWindow) {
//        m_newWindow = new NewWindow(this);
//        // 新增：连接左声道强度信号与槽
//          connect(this, &AudioRecorder::leftChannelLevelUpdated,
//                  m_newWindow, &NewWindow::setLeftChannelLevel);
//    }
//    m_newWindow->show();
//    m_newWindow->activateWindow();
//}



void AudioRecorder::commandBt1Clicked() {
    if (!m_newWindow) {
        m_newWindow = new NewWindow(this);
        // 已有的左声道强度信号连接（保留）
        connect(this, &AudioRecorder::leftChannelLevelUpdated,
                m_newWindow, &NewWindow::setLeftChannelLevel);

        // 新增：无论m_barChartWindow是否已创建，都连接信号
        // （Qt会自动处理对象销毁后的连接失效，无需手动断开）
        connect(m_newWindow, &NewWindow::filteredIntensityUpdated,
                this, &AudioRecorder::onFilteredIntensityReceived);

        connect(m_newWindow, &NewWindow::destroyed, this, [=]() {
            m_newWindow = nullptr;
        });
        // 同步当前强度值（保留）
        m_newWindow->setLeftChannelLevel(leftChannelLevel);
    }
    m_newWindow->show();
    m_newWindow->activateWindow();
}


// 实现槽函数
void AudioRecorder::onCenterFrequencyChanged(double newFreq)
{
    // 更新本地中心频率参数
    m_centerFreq = newFreq;

    // 可以在这里添加需要的后续处理，例如：
    // 1. 更新UI显示
    qDebug() << "中心频率已更新为：" << newFreq;
    // 2. 应用新的频率参数到滤波器
    // 3. 记录日志等
    testBrowser->append(QString("中心频率已调整为：%1 Hz").arg(newFreq));
}





// 修改后的函数：仅自动排序的文件名
//void AudioRecorder::saveBothFilteredData(const QVector<double>& /*filteredLeftData*/, const QVector<double>& /*filteredRightData*/,
//                                         const QVector<double>& originalLeftData, const QVector<double>& originalRightData) {
//    if (!m_saveBothChannels) return;  // 未开启保存时直接返回

//    // 仅校验原始数据的帧对齐（忽略滤波数据）
//    int validFrames = qMin(originalLeftData.size(), originalRightData.size());
//    if (validFrames == 0) return;

//    // 仅累加原始数据到缓冲区（移除滤波数据处理）
//    for (int i = 0; i < validFrames; ++i) {
//        // 只处理原始数据缓冲区
//        m_originalLeftDataBuffer.append(originalLeftData[i]);
//        m_originalRightDataBuffer.append(originalRightData[i]);

//        m_bothDataCount++;

//        // 达到5万帧时保存并重置
//        if (m_bothDataCount >= MAX_BOTH_DATA) {
//            // 只保存原始数据（移除滤波数据相关代码）
//            QString originalFileName = QString("both_channels_original_data_%1.txt").arg(m_bothFileIndex);
//            saveDataToFile(originalFileName, m_originalLeftDataBuffer, m_originalRightDataBuffer);

//            // 仅重置原始数据缓冲区
//            m_originalLeftDataBuffer.clear();
//            m_originalRightDataBuffer.clear();
//            m_bothDataCount = 0;
//            m_bothFileIndex++;

//            // 自动停止保存
//            m_saveBothChannels = false;
//            newLeftBtn->setText("保存");
//            return;
//        }
//    }
//}


void AudioRecorder::saveBothFilteredData(const QVector<double>& /*filteredLeftData*/, const QVector<double>& /*filteredRightData*/,
                                         const QVector<double>& originalLeftData, const QVector<double>& originalRightData) {
    if (!m_saveBothChannels) return;  // 未开启保存时直接返回

    // 仅校验原始数据的帧对齐（忽略滤波数据）
    int validFrames = qMin(originalLeftData.size(), originalRightData.size());
    if (validFrames == 0) return;

    // 仅累加原始数据到缓冲区（移除滤波数据处理）
    for (int i = 0; i < validFrames; ++i) {
        // 只处理原始数据缓冲区
        m_originalLeftDataBuffer.append(originalLeftData[i]);
        m_originalRightDataBuffer.append(originalRightData[i]);

        m_bothDataCount++;

        // 达到5万帧时保存并重置
        if (m_bothDataCount >= MAX_BOTH_DATA) {
            // 获取m_valueLineEdit中的文本作为基础文件名
            QString baseFileName = m_valueLineEdit->text().trimmed();

            // 处理空文件名的情况（提供默认值）
            if (baseFileName.isEmpty()) {
                baseFileName = "both_channels_original_data";
            }

            // 拼接完整的文件名（包含索引）
            QString originalFileName = QString("%1_%2.txt").arg(baseFileName).arg(m_bothFileIndex);

            // 保存数据到文件
            saveDataToFile(originalFileName, m_originalLeftDataBuffer, m_originalRightDataBuffer);

            // 仅重置原始数据缓冲区
            m_originalLeftDataBuffer.clear();
            m_originalRightDataBuffer.clear();
            m_bothDataCount = 0;
            m_bothFileIndex++;

            // 自动停止保存
            m_saveBothChannels = false;
            if (newLeftBtn) { // 空指针保护
                newLeftBtn->setText("保存");
            }
            return;
        }
    }
}






// 保持辅助函数不变，但实际只会处理原始数据
void AudioRecorder::saveDataToFile(const QString& fileName, const QVector<double>& leftData, const QVector<double>& rightData) {
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        // 按"左声道原始值,右声道原始值"格式写入，每行一帧
        for (int j = 0; j < leftData.size() && j < rightData.size(); ++j) {
            out << leftData[j] << "," << rightData[j] << "\n";
        }
        file.close();
        qDebug() << "已保存原始数据至" << fileName << "（" << leftData.size() << "帧）";
        testBrowser->append(QString("原始数据已保存: %1").arg(fileName));
    } else {
        qDebug() << "无法写入文件" << fileName << "：" << file.errorString();
    }
}





void AudioRecorder::handleAudioInput()
{
    if (!m_inputDevice || !m_outputDevice)
        return;

    QByteArray buffer = m_inputDevice->readAll();
    if (buffer.isEmpty())
        return;

    // 实时播放音频
    m_outputDevice->write(buffer);

    // 初始化波形窗口
    if (m_newWindow == nullptr) {
        m_newWindow = new NewWindow(this);
        connect(this, &AudioRecorder::leftChannelSamples, m_newWindow, &NewWindow::updateWaveform);


        connect(m_newWindow, &NewWindow::centerFrequencyChanged,
                this, &AudioRecorder::onCenterFrequencyChanged);

        m_newWindow->show(); // 必须显示窗口才能看到波形
    }

    // ================= 提取左右声道原始波形数据 =================
    QVector<double> leftChannelData;
    QVector<double> rightChannelData;




    QAudioFormat format = m_audioInput->format(); // 获取音频格式
    if (format.isValid()) {
        int bytesPerSample = format.sampleSize() / 8; // 每个样本的字节数
        int channelCount = format.channelCount();     // 声道数（立体声为2）
        int frameSize = bytesPerSample * channelCount; // 每帧字节数
        int frameCount = buffer.size() / frameSize;    // 总帧数

        const char *data = buffer.constData();

        for (int i = 0; i < frameCount; ++i) {
            // 提取左声道数据（索引0）
            int leftChannelPos = i * frameSize + 0 * bytesPerSample;
            // 提取右声道数据（索引1）
            int rightChannelPos = i * frameSize + 1 * bytesPerSample;

            if (format.sampleType() == QAudioFormat::SignedInt && bytesPerSample == 2) {
                qint16 leftSample, rightSample;
                // 左声道
                memcpy(&leftSample, data + leftChannelPos, bytesPerSample);
                leftChannelData.append(static_cast<double>(leftSample));

                // 右声道
                memcpy(&rightSample, data + rightChannelPos, bytesPerSample);
                rightChannelData.append(static_cast<double>(rightSample));
            }
        }
    }


    m_newWindow->updateSpectrumBars(leftChannelData);  // 传入左声道数据



    // 以下是原有的音量强度显示逻辑（保持不变）
    QVector<qreal> levels = getBufferLevels(buffer);


    // 初始化左右声道强度值
    qreal leftChannelLevel = 0.0;
    qreal rightChannelLevel = 0.0;

    // 获取左声道强度（索引0）
    if (levels.count() >= 1) {
        leftChannelLevel = levels.at(0);
        // 更新左声道最大值
        if (leftChannelLevel > m_maxLeftChannelLevel) {
            m_maxLeftChannelLevel = leftChannelLevel;
        }



    }

    // 获取右声道强度（索引1）
    if (levels.count() >= 2) {
        rightChannelLevel = levels.at(1);
        // 更新右声道最大值（需在类中添加m_maxRightChannelLevel成员变量）
        if (rightChannelLevel > m_maxRightChannelLevel) {
            m_maxRightChannelLevel = rightChannelLevel;
        }
    }



    // 合并左右声道实时强度到左声道标签
    leftChannelLevelLabel->setText(
                QString("强度: 左%1% | 右%2%")
                .arg(leftChannelLevel * 100, 0, 'f', 2)
                .arg(rightChannelLevel * 100, 0, 'f', 2)
                );



    // 合并左右声道最大值到左声道最大值标签
    maxLeftChannelLevelLabel->setText(
                QString("最强: 左%1% | 右%2%")
                .arg(m_maxLeftChannelLevel * 100, 0, 'f', 2)
                .arg(m_maxRightChannelLevel * 100, 0, 'f', 2)
                );


      saveBothFilteredData(QVector<double>(), QVector<double>(), leftChannelData, rightChannelData);

}



QVector<qreal> AudioRecorder::getBufferLevels(const QByteArray &buffer)
{
    QVector<qreal> values;

    // 获取音频格式
    QAudioFormat format = m_audioInput->format();
    if (!format.isValid())
        return values;

    int bytesPerSample = format.sampleSize() / 8;
    int channelCount = format.channelCount();
    int frameSize = bytesPerSample * channelCount;
    int frameCount = buffer.size() / frameSize;

    values.fill(0, channelCount);

    const char *data = buffer.constData();

    for (int i = 0; i < frameCount; ++i) {
        for (int ch = 0; ch < channelCount; ++ch) {
            qreal sample = 0.0;

            // 处理不同的样本类型
            if (format.sampleType() == QAudioFormat::SignedInt) {
                if (bytesPerSample == 2) { // 16-bit
                    qint16 value;
                    memcpy(&value, data + i * frameSize + ch * bytesPerSample, bytesPerSample);
                    sample = qAbs(static_cast<qreal>(value)) / static_cast<qreal>(SHRT_MAX);
                }
                // 可以添加更多样本大小的处理
            }
            // 可以添加更多样本类型的处理

            if (sample > values[ch])
                values[ch] = sample;
        }
    }

    return values;
}





void AudioRecorder::updateProgress()
{
    static qint64 recordedSeconds = 0;
    recordedSeconds += 1; // 增加1秒
    countLabel->setText(QString("工作时间 %1 s").arg(recordedSeconds));
}

void AudioRecorder::commandBt2Clicked()
{
    // 重置左右声道最大值变量
    m_maxLeftChannelLevel = 0;
    m_maxRightChannelLevel = 0;

    m_globalMaxFilteredIntensity=0;
    globalMaxLabel->setText(
                QString("滤强: %1%").arg(m_globalMaxFilteredIntensity, 0, 'f', 1)
                );

    // 在同一个标签上合并显示左右声道的最大值重置信息
    maxLeftChannelLevelLabel->setText(
                QString("声道强度: 左=0% | 右=0%")
                );
}


void AudioRecorder::commandBt3Clicked()
{

    //    system(" amixer -c 0 cset numid=1   0");

    ychz += 100;
    QByteArray data = "led2_on";  // 假设我们要发送的测试数据
    serialPort->write(data);
    // 更新现有按钮的文本，显示当前频率值
    newBtn3->setText(QString("频率: %1").arg(ychz));


}

void AudioRecorder::commandBt6Clicked()
{
    ////    system("echo 128 > /sys/devices/platform/soc/40015000.i2c/i2c-2/2-002c/rdac1");  // 执行第一个命令
    ////    system("echo 128 > /sys/devices/platform/soc/40015000.i2c/i2c-2/2-002c/rdac0");  // 执行第一个命令


    //    system(" amixer -c 0 cset numid=1   127");


    ychz = qMax(0, ychz - 100);  // 允许频率降低到0Hz
    QByteArray data = "led2_off";  // 假设我们要发送的测试数据
    serialPort->write(data);

    // 更新现有按钮的文本，显示当前频率值
    newBtn3->setText(QString("频率: %1").arg(ychz));
}


void AudioRecorder::updateHValueFromSlider(int value)
{


    // 使用QProcess异步执行命令
    QProcess *process = new QProcess(this);
    process->start("amixer", QStringList() << "-c" << "0" << "cset" << "numid=1" << QString::number(value));

}

// 槽函数修改（滑动条变量名同步修改）
void AudioRecorder::updateVerticalSliderValue(int value)
{
    // 移除数值反转逻辑，直接使用滑动条的值
    int setValue = value;

    // 执行系统命令
    QString command1 = QString("echo %1 > /sys/devices/platform/soc/40015000.i2c/i2c-2/2-002c/rdac1").arg(setValue);
    QProcess *process1 = new QProcess(this);
    process1->start("/bin/sh", QStringList() << "-c" << command1);

    QString command2 = QString("echo %1 > /sys/devices/platform/soc/40015000.i2c/i2c-2/2-002c/rdac0").arg(setValue);
    QProcess *process2 = new QProcess(this);
    process2->start("/bin/sh", QStringList() << "-c" << command2);

    // 日志显示（更新说明文本）
    testBrowser->append(QString("滑动条: 设置值=%1").arg(setValue));

    // 资源清理
    connect(process1, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            process1, &QProcess::deleteLater);
    connect(process2, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            process2, &QProcess::deleteLater);
}


void AudioRecorder::scanSerialPort()
{
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    qDebug() << "可用的串口列表:";
    for (const QSerialPortInfo &info : ports) {
        qDebug() << "Port:" << info.portName() << "Description:" << info.description();
    }

    if (!ports.isEmpty()) {
        // 如果有可用串口，设置默认串口
        serialPort->setPortName(ports.first().portName());
        qDebug() << "已选择串口:" << ports.first().portName();
    }
}



// 实现新按钮的槽函数
void AudioRecorder::newBtn1Clicked() {
    qDebug() << "新按钮1被点击";
    // 添加按钮1的功能代码

    // 禁用按钮，防止重复点击
    newBtn1->setEnabled(false);
    // 可选：改变按钮样式以直观显示已禁用
    newBtn1->setStyleSheet("background-color: black; color: lightgray;");

    QFile file("/dev/ttyRPMSG0");
    if (!file.exists()) {
        system("cd /lib/firmware");
        QString fw = tr("echo %1 > /sys/class/remoteproc/remoteproc0/firmware").arg(firmware);
        system(fw.toLatin1().data());
        system("echo start > /sys/class/remoteproc/remoteproc0/state");
        system("echo start > /dev/ttyRPMSG0");
        system("sleep 1");
        scanSerialPort();

    }



    // 固定参数配置
    serialPort->setPortName("/dev/ttyRPMSG0");  // 固定设备名
    serialPort->setBaudRate(115200);           // 固定波特率
    serialPort->setDataBits(QSerialPort::Data8); // 8位数据位
    serialPort->setParity(QSerialPort::NoParity); // 无校验
    serialPort->setStopBits(QSerialPort::OneStop); // 1位停止位
    serialPort->setFlowControl(QSerialPort::NoFlowControl); // 无流控



    // 尝试打开串口
    if (!serialPort->open(QIODevice::ReadWrite)) {
        QString errorMsg = QString("串口打开失败!\n"
                                   "设备: %1\n"
                                   "错误: %2\n"
                                   "可能原因:\n"
                                   "1. 设备不存在\n"
                                   "2. 权限不足(尝试: sudo chmod 666 /dev/ttyRPMSG0)\n"
                                   "3. 串口已被占用")
                .arg(serialPort->portName())
                .arg(serialPort->errorString());

    } else {


        qDebug() << "串口已打开 - 配置: 115200 8N1";
    }

}


void AudioRecorder::newBtn2Clicked() {
    qDebug() << "新按钮2被点击";
    // 添加按钮2的功能代码

    QByteArray data = "led1_on";  // 假设我们要发送的测试数据
    serialPort->write(data);

//    switch(ycyl) {
//    case 0: ycyl = 30; break;
//    case 30: ycyl = 50; break;
//    case 50: ycyl = 80; break;
//    case 80: ycyl = 100; break;
//    default: ycyl = 0; break;
//    }

    // 更新按钮文本显示当前音量
    //newBtn2->setText(QString("音量: %1%").arg(ycyl));


}

void AudioRecorder::newBtn3Clicked() {
    qDebug() << "新按钮3被点击";
    // 添加按钮3的功能代码
}

void AudioRecorder::newBtn4Clicked() {
    qDebug() << "新按钮4被点击";
    // 添加按钮4的功能代码
//扫频
//    QByteArray data = "led1_off";  // 假设我们要发送的测试数据
//    serialPort->write(data);

    // 仅在 testBrowser 显示“删除”文本（可根据需求调整文本内容）
       testBrowser->append("删除");
       // 可选：让文本自动滚动到最新一行（避免被遮挡）
       testBrowser->moveCursor(QTextCursor::End);

}






//void AudioRecorder::serialPortReadyRead()
//{

//    QByteArray buf = serialPort->readAll();

//    testBrowser->append("接收: " + QString(buf));




//}



void AudioRecorder::serialPortReadyRead()
{
    QByteArray buf = serialPort->readAll();
    QString data = QString(buf);
    testBrowser->append("接收: " + data);

    // 解析纬度（兼容"维度"和"纬度"字段）
    QRegExp latReg("(维度|纬度)[:：]\\s*([-+]?\\d+\\.?\\d*)");
    if (latReg.indexIn(data) != -1) {
        m_latitude = latReg.cap(2).toDouble();
        // 关键：如果GPS窗口已打开，发送更新信号
        if (m_gpsWindow != nullptr && m_gpsWindow->isVisible()) {
            emit gpsDataSent(m_latitude, m_longitude);
        }
    }

    // 解析经度
    QRegExp lonReg("经度[:：]\\s*([-+]?\\d+\\.?\\d*)");
    if (lonReg.indexIn(data) != -1) {
        m_longitude = lonReg.cap(1).toDouble();
        // 关键：如果GPS窗口已打开，发送更新信号
        if (m_gpsWindow != nullptr && m_gpsWindow->isVisible()) {
            emit gpsDataSent(m_latitude, m_longitude);
        }
    }
}
