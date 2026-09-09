#include "newwindow.h"
#include "signalquality.h"
#include "ui_newwindow.h"
#include <cmath>
#include <vector>
#include <complex>  // 添加复数头文件
#include <algorithm> // 用于std::max_element
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

NewWindow::NewWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::NewWindow)
{
    ui->setupUi(this); // 先初始化UI
    setFixedSize(800, 480);
    setWindowTitle("GPPL5000 · 频谱分析");
    statusBar()->hide();

    QVBoxLayout *pageLayout = new QVBoxLayout(ui->centralwidget);
    pageLayout->setContentsMargins(10, 8, 10, 8);
    pageLayout->setSpacing(7);

    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel *pageTitle = new QLabel("信号接收分析", ui->centralwidget);
    pageTitle->setObjectName("panelTitle");
    ui->label->setText("中心频率：500 Hz");
    ui->label_3->setText("滤波强度：0%");
    ui->pushButton->setText("返回主界面");
    ui->pushButton->setProperty("role", "danger");
    ui->pushButton_4->setText("记录强度");
    ui->pushButton_4->setProperty("role", "secondary");
    ui->pushButton_5->setText("清空记录");
    headerLayout->addWidget(pageTitle);
    headerLayout->addWidget(ui->label);
    headerLayout->addStretch();
    headerLayout->addWidget(ui->label_3);
    headerLayout->addWidget(ui->pushButton_4);
    headerLayout->addWidget(ui->pushButton_5);
    headerLayout->addWidget(ui->pushButton);
    pageLayout->addLayout(headerLayout);

    pageLayout->addWidget(ui->myCustomPlot, 3);

    QHBoxLayout *lowerLayout = new QHBoxLayout();
    lowerLayout->setSpacing(8);
    lowerLayout->addWidget(ui->widget, 1);
    QFrame *frequencyPanel = new QFrame(ui->centralwidget);
    frequencyPanel->setObjectName("toolRail");
    frequencyPanel->setFixedWidth(150);
    QVBoxLayout *frequencyLayout = new QVBoxLayout(frequencyPanel);
    QLabel *frequencyTitle = new QLabel("硬件中心频率", frequencyPanel);
    frequencyTitle->setObjectName("panelTitle");
    ui->pushButton_2->setText("+100 Hz");
    ui->pushButton_3->setText("−100 Hz");
    ui->radioButton->hide();
    ui->radioButton_2->hide();
    ui->label_6->hide();
    ui->label_7->hide();
    ui->label_4->setText("软件版本 V1.6");
    frequencyLayout->addWidget(frequencyTitle);
    frequencyLayout->addWidget(ui->label_5);
    frequencyLayout->addWidget(ui->pushButton_2);
    frequencyLayout->addWidget(ui->pushButton_3);
    frequencyLayout->addStretch();
    frequencyLayout->addWidget(ui->label_4);
    lowerLayout->addWidget(frequencyPanel);
    pageLayout->addLayout(lowerLayout, 2);

       // 获取控件指针
       label_5 = ui->label_5;
       pushButton_2 = ui->pushButton_2;
       pushButton_3 = ui->pushButton_3;

       // 连接按钮与功能函数
       connect(pushButton_2, &QPushButton::clicked, this, &NewWindow::onAddClicked);
       connect(pushButton_3, &QPushButton::clicked, this, &NewWindow::onSubtractClicked);

       // 初始化硬件中心频率显示
       updateLabelSize();

    // 初始化坐标轴，固定纵轴范围
    ui->myCustomPlot->xAxis->setLabel("频率 (Hz)");
    ui->myCustomPlot->yAxis->setLabel("幅值");
    ui->myCustomPlot->xAxis->setRange(100, 2000); // 横轴范围不变
    // 纵轴固定范围：根据实际信号幅值调整（例如0~50000，确保最大信号不超出）
    ui->myCustomPlot->yAxis->setRange(0, 100);
    ui->myCustomPlot->setBackground(QBrush(QColor("#010503")));
    for (QCPAxis *axis : {ui->myCustomPlot->xAxis, ui->myCustomPlot->yAxis}) {
        axis->setBasePen(QPen(QColor("#4cbf78")));
        axis->setTickPen(QPen(QColor("#4cbf78")));
        axis->setSubTickPen(QPen(QColor("#2b7048")));
        axis->setTickLabelColor(QColor("#aee8bd"));
        axis->setLabelColor(QColor("#d9f5df"));
        axis->grid()->setPen(QPen(QColor("#173c28"), 1, Qt::DotLine));
    }

    // 在NewWindow构造函数中修改m_spectrumBars的样式
    m_spectrumBars = new QCPBars(ui->myCustomPlot->xAxis, ui->myCustomPlot->yAxis);
    m_spectrumBars->setWidth(3.0); // 适当加宽柱子
    m_spectrumBars->setPen(QPen(QColor("#77ff9f"), 1));
    m_spectrumBars->setBrush(QColor(55, 224, 112, 210));
    // 可选：添加网格线辅助观察
    ui->myCustomPlot->xAxis->grid()->setVisible(true);
    ui->myCustomPlot->yAxis->grid()->setVisible(true);




    m_avgBars = new QCPBars(ui->widget->xAxis, ui->widget->yAxis);
    m_avgBars->setWidth(0.2);

    ui->widget->yAxis->setRange(0, 100);

    ui->widget->xAxis->setLabel("记录次数");
    ui->widget->xAxis->setRange(0, m_maxBars + 1);
    ui->widget->setBackground(QBrush(QColor("#010503")));
    for (QCPAxis *axis : {ui->widget->xAxis, ui->widget->yAxis}) {
        axis->setBasePen(QPen(QColor("#4cbf78")));
        axis->setTickPen(QPen(QColor("#4cbf78")));
        axis->setTickLabelColor(QColor("#aee8bd"));
        axis->setLabelColor(QColor("#d9f5df"));
        axis->grid()->setPen(QPen(QColor("#173c28"), 1, Qt::DotLine));
    }
    m_avgBars->setPen(QPen(QColor("#60e9ff")));
    m_avgBars->setBrush(QColor(39, 180, 203, 190));


    ui->widget->yAxis->setTicks(true);
    ui->widget->yAxis->setTickLabels(true);
    ui->widget->yAxis->setNumberFormat("f");
    ui->widget->yAxis->setNumberPrecision(0);   // 0位小数，想要1位就改成1


    ui->widget->replot();




}



NewWindow::~NewWindow()
{
    delete m_spectrumBars; // 手动释放柱状图对象
    delete ui;
}
// 在newwindow.cpp中替换原fft函数为迭代版本
QVector<std::complex<double>> NewWindow::fft(const QVector<std::complex<double>>& x)
{
    int N = x.size();
    QVector<std::complex<double>> result = x;

    // 位反转
    for (int i = 1, j = 0; i < N; ++i) {
        int bit = N >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(result[i], result[j]);
    }

    // 迭代FFT
    for (int len = 2; len <= N; len <<= 1) {
        double ang = 2 * M_PI / len;
        std::complex<double> wlen(cos(ang), -sin(ang)); // 旋转因子
        for (int i = 0; i < N; i += len) {
            std::complex<double> w(1);
            for (int j = 0; j < len / 2; ++j) {
                std::complex<double> u = result[i + j];
                std::complex<double> v = result[i + j + len/2] * w;
                result[i + j] = u + v;
                result[i + j + len/2] = u - v;
                w *= wlen;
            }
        }
    }
    return result;
}
//接收原始pcm数据
void NewWindow::updateSpectrumBars(const QVector<double>& filteredAudioData)
{
    // 选频由外部硬件完成；软件直接分析硬件输出，不再重复带通滤波。
    const QVector<double> &reFilteredData = filteredAudioData;

    // 计算滤波后的强度并显示在 label_3（添加频率控制）
    if (!reFilteredData.isEmpty()) {
        // 1. 计数器递增，达到阈值才更新显示
        m_labelUpdateCounter++;
        if (m_labelUpdateCounter >= m_labelUpdateThreshold) {
            m_labelUpdateCounter = 0;  // 重置计数器

            // 2. 找到滤波后数据的峰值（最大绝对值）
            double maxVal = *std::max_element(reFilteredData.begin(), reFilteredData.end());
            double minVal = *std::min_element(reFilteredData.begin(), reFilteredData.end());
            double peakValue = qMax(qAbs(maxVal), qAbs(minVal));

            // 3. 归一化到 0-100（16位PCM最大幅值32767）
            double refMax = 32767.0;
            double filteredIntensity = (peakValue / refMax) * 100.0;

            // 4. 边界约束（0-100范围）
            filteredIntensity = qBound(0.0, filteredIntensity, 100.0);

            // 你原本就有的显示
            ui->label_3->setText(QString(" %1%").arg(filteredIntensity, 0, 'f', 1));

            // ===== 可选：继续维护最近10次缓存（你已有就保留）=====
            m_intensityBuffer.append(filteredIntensity);
            if (m_intensityBuffer.size() > 10)
                m_intensityBuffer.removeFirst();

            // 记录一个完整窗口，用周期门控和分位数抑制静音与突发噪声。
            if (m_recording) {
                m_recordSamples.append(filteredIntensity);
                ui->pushButton_4->setText(
                    QString("采集中 %1/16").arg(m_recordSamples.size()));

                if (m_recordSamples.size() >= 16) {
                    const RobustSignalResult stable = robustSignalLevel(
                        m_recordSamples.constData(), m_recordSamples.size());
                    m_recording = false;
                    m_recordSamples.clear();
                    ui->pushButton_4->setText("记录强度");

                    if (!stable.valid) {
                        ui->label_3->setText("未检测到稳定周期，请重测");
                    } else {
                        m_avgHistory.append(stable.level);
                        if (m_avgHistory.size() > m_maxBars)
                            m_avgHistory.removeFirst();

                        QVector<double> keys, vals;
                        const int n = m_avgHistory.size();
                        keys.reserve(n);
                        vals.reserve(n);
                        for (int i = 0; i < n; ++i) {
                            keys << (i + 1);
                            vals << m_avgHistory[i];
                        }
                        m_avgBars->setData(keys, vals);
                        ui->widget->xAxis->setRange(0, 11);
                        ui->widget->yAxis->setTicks(true);
                        ui->widget->yAxis->setTickLabels(true);
                        ui->widget->yAxis->setNumberFormat("f");
                        ui->widget->yAxis->setNumberPrecision(0);

                        double ymax = 0.0;
                        for (double value : m_avgHistory)
                            ymax = qMax(ymax, value);
                        ui->widget->yAxis->setRange(0, ymax * 1.15 + 8.0);
                        ui->widget->clearItems();

                        for (int i = 0; i < n; ++i) {
                            QCPItemText *text = new QCPItemText(ui->widget);
                            text->setLayer("overlay");
                            text->position->setType(QCPItemPosition::ptPlotCoords);
                            text->position->setCoords(i + 1, m_avgHistory[i] + 1.0);
                            text->setPositionAlignment(Qt::AlignHCenter | Qt::AlignBottom);
                            text->setText(QString::number(m_avgHistory[i], 'f', 1));
                            text->setColor(QColor("#d9f5df"));
                        }
                        ui->widget->replot();
                    }
                }
            }




            // 计算出filteredIntensity后直接打印
           // qDebug() << "NewWindow中计算的滤波强度：" << filteredIntensity;

            // 发送信号
            emit filteredIntensityUpdated(filteredIntensity);


        }
    }

    // 强度测量不依赖FFT；频谱页隐藏时跳过FFT，降低主界面采集负载。
    if (!isVisible())
        return;

    // 后续FFT计算和频谱显示逻辑（使用滤波后的数据）
    // 1. 准备FFT输入（基于内部滤波后的数据）
    QVector<std::complex<double>> fftInput;
    for (int i = 0; i < FFT_SIZE; ++i) {
        double sample = (i < reFilteredData.size()) ? reFilteredData[i] / 32768.0 : 0.0;
        fftInput.append(std::complex<double>(sample, 0.0));
    }

    // 2. 执行FFT变换
    QVector<std::complex<double>> fftResult = fft(fftInput);

    // 3. 频谱只显示硬件中心频率附近，便于观察，不参与滤波。
    QVector<double> frequencies;
    QVector<double> magnitudes;
    double freqStep = SAMPLE_RATE / FFT_SIZE;

    m_filterMinFreq = m_centerFreq - 200.0;
    m_filterMaxFreq = m_centerFreq + 200.0;

    // 确保范围合法
    m_filterMinFreq = qMax(1.0, m_filterMinFreq);
    m_filterMaxFreq = qMin(m_sampleRate / 2 - 1, m_filterMaxFreq);
    if (m_filterMinFreq > m_filterMaxFreq)
        qSwap(m_filterMinFreq, m_filterMaxFreq);

    // 只保留滤波范围内的频率
    for (int i = 0; i < FFT_SIZE/2; ++i) {
        double freq = i * freqStep;
        if (freq >= m_filterMinFreq && freq <= m_filterMaxFreq) {
            frequencies.append(freq);
            magnitudes.append(std::abs(fftResult[i]));
        }
    }

    // 5. 设置柱状图数据并刷新
    m_spectrumBars->setData(frequencies, magnitudes);
    ui->myCustomPlot->replot();
}



void NewWindow::on_pushButton_clicked()
{
    // 找到父窗口（主窗口 AudioRecorder）并显示
    if (QWidget *parentWidget = this->parentWidget()) {
        parentWidget->show();
    }
    // 关闭当前新窗口
    this->close();
}





void NewWindow::updateWaveform(const QVector<double> &waveData)
{


}



void NewWindow::onAddClicked()
{
    m_centerFreq = qMin(20000.0, m_centerFreq + 100.0);
    updateLabelSize();
}

void NewWindow::onSubtractClicked()
{
    m_centerFreq = qMax(100.0, m_centerFreq - 100.0);
    updateLabelSize();
}
void NewWindow::updateLabelSize()
{
    label_5->setText(QString("%1 Hz").arg(m_centerFreq, 0, 'f', 0));
    ui->label->setText(QString("中心频率：%1 Hz").arg(m_centerFreq, 0, 'f', 0));
    emit centerFrequencyChanged(m_centerFreq);
}

void NewWindow::on_pushButton_4_clicked()
{
    m_recording = true;
    m_recordSamples.clear();
    ui->pushButton_4->setText("采集中 0/16");
}

void NewWindow::setCenterFrequency(double frequency)
{
    m_centerFreq = qBound(100.0, frequency, 20000.0);
    label_5->setText(QString("%1 Hz").arg(m_centerFreq, 0, 'f', 0));
    ui->label->setText(QString("中心频率：%1 Hz").arg(m_centerFreq, 0, 'f', 0));
}

void NewWindow::on_pushButton_5_clicked()
{
      m_recording = false;
      m_recordSamples.clear();
      ui->pushButton_4->setText("记录强度");
    // 1️⃣ 清空柱子数据
      m_avgHistory.clear();
      m_avgBars->setData(QVector<double>(), QVector<double>());

      // 2️⃣ 清空柱子顶部的文字（QCPItemText）
      ui->widget->clearItems();

      // 3️⃣ 重置坐标轴显示范围
      ui->widget->xAxis->setRange(0, 11);   // 仍然保持 10 根柱子的范围
      ui->widget->yAxis->setRange(0, 100);  // 强度范围 0~100

      // 4️⃣ 刷新显示
      ui->widget->replot();
}
