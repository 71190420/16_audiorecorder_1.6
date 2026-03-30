#include "newwindow.h"
#include "ui_newwindow.h"
#include <cmath>
#include <vector>
#include <complex>  // 添加复数头文件
#include <algorithm> // 用于std::max_element

NewWindow::NewWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::NewWindow)
{
    ui->setupUi(this); // 先初始化UI
       // 获取控件指针
       radioButton = ui->radioButton;
       radioButton_2 = ui->radioButton_2;
       label_5 = ui->label_5;
       label_7 = ui->label_7;
       pushButton_2 = ui->pushButton_2;
       pushButton_3 = ui->pushButton_3;

       // 连接按钮与功能函数
       connect(pushButton_2, &QPushButton::clicked, this, &NewWindow::onAddClicked);
       connect(pushButton_3, &QPushButton::clicked, this, &NewWindow::onSubtractClicked);

       // 初始化显示（默认选中label_5，显示初始数值）
       radioButton->setChecked(true);
       updateLabelSize();

    // 初始化坐标轴，固定纵轴范围
    ui->myCustomPlot->xAxis->setLabel("频率 (Hz)");
    ui->myCustomPlot->yAxis->setLabel("幅值");
    ui->myCustomPlot->xAxis->setRange(100, 2000); // 横轴范围不变
    // 纵轴固定范围：根据实际信号幅值调整（例如0~50000，确保最大信号不超出）
    ui->myCustomPlot->yAxis->setRange(0, 100);

    // 在NewWindow构造函数中修改m_spectrumBars的样式
    m_spectrumBars = new QCPBars(ui->myCustomPlot->xAxis, ui->myCustomPlot->yAxis);
    m_spectrumBars->setWidth(3.0); // 适当加宽柱子
    m_spectrumBars->setPen(QPen(Qt::darkBlue, 1)); // 深色边框，突出轮廓
    m_spectrumBars->setBrush(QColor(50, 150, 255, 220)); // 提高不透明度（220/255），蓝色更鲜艳
    // 可选：添加网格线辅助观察
    ui->myCustomPlot->xAxis->grid()->setVisible(true);
    ui->myCustomPlot->yAxis->grid()->setVisible(true);




    m_avgBars = new QCPBars(ui->widget->xAxis, ui->widget->yAxis);
    m_avgBars->setWidth(0.2);

    ui->widget->yAxis->setRange(0, 100);

    ui->widget->xAxis->setLabel("记录次数");
    ui->widget->xAxis->setRange(0, m_maxBars + 1);


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
    // 进行滤波
    QVector<double> reFilteredData = butterworthBandpassFilter(filteredAudioData);

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

            // ===== 关键：如果正在录制，就累计10次 =====
            if (m_recording) {
                m_recordSum += filteredIntensity;
                m_recordCount++;

                if (m_recordCount >= 10) {
                    double avg = m_recordSum / 10.0;

                    // 结束本次录制
                    m_recording = false;
                    m_recordCount = 0;
                    m_recordSum = 0.0;

                    // 把平均值追加为一根柱子
                    m_avgHistory.append(avg);
                    if (m_avgHistory.size() > m_maxBars)
                        m_avgHistory.removeFirst();

                    // 更新柱状图数据
                    QVector<double> keys, vals;
                    int n = m_avgHistory.size();
                    keys.reserve(n);
                    vals.reserve(n);
                    for (int i = 0; i < n; ++i) {
                        keys << (i + 1);
                        vals << m_avgHistory[i];
                    }

                    m_avgBars->setData(keys, vals);

                    // X轴固定显示1~10
                    ui->widget->xAxis->setRange(0, 11);

                    // 打开Y轴刻度数字
                    ui->widget->yAxis->setTicks(true);
                    ui->widget->yAxis->setTickLabels(true);
                    ui->widget->yAxis->setNumberFormat("f");
                    ui->widget->yAxis->setNumberPrecision(0);

                    // 计算ymax并留足顶部空间，防止文字被裁
                    double ymax = 0.0;
                    for (double v : m_avgHistory) ymax = qMax(ymax, v);
                    ui->widget->yAxis->setRange(0, ymax * 1.15 + 8.0);

                    // 先清掉旧的文字标签
                    ui->widget->clearItems();

                    // 给每根柱子加一个数值显示
                    for (int i = 0; i < n; ++i) {
                        QCPItemText *text = new QCPItemText(ui->widget);
                        text->setLayer("overlay");
                        text->position->setType(QCPItemPosition::ptPlotCoords);

                        // 往上抬一点，避免贴边
                        text->position->setCoords(i + 1, m_avgHistory[i] + 1.0);

                        text->setPositionAlignment(Qt::AlignHCenter | Qt::AlignBottom);
                        text->setText(QString::number(m_avgHistory[i], 'f', 1));
                    }

                    ui->widget->replot();

                    // 可选提示
                    // ui->statusbar->showMessage(QString("记录完成：Avg(10)=%1%").arg(avg,0,'f',1), 2000);
                }
            }



            // 计算出filteredIntensity后直接打印
           // qDebug() << "NewWindow中计算的滤波强度：" << filteredIntensity;

            // 发送信号
            emit filteredIntensityUpdated(filteredIntensity);


        }
    }

    // 后续FFT计算和频谱显示逻辑（使用滤波后的数据）
    // 1. 准备FFT输入（基于内部滤波后的数据）
    QVector<std::complex<double>> fftInput;
    for (int i = 0; i < FFT_SIZE; ++i) {
        double sample = (i < reFilteredData.size()) ? reFilteredData[i] / 32768.0 : 0.0;
        fftInput.append(std::complex<double>(sample, 0.0));
    }

    // 2. 执行FFT变换
    QVector<std::complex<double>> fftResult = fft(fftInput);

    // 3. 计算频率轴和幅值（使用label5/7的动态范围）
    QVector<double> frequencies;
    QVector<double> magnitudes;
    double freqStep = SAMPLE_RATE / FFT_SIZE;

    // 从label5和label7获取当前上下限（与滤波参数保持一致）
    m_filterMinFreq = label5Size;
    m_filterMaxFreq = label7Size;

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



void NewWindow::setBandpassParams(double sampleRate, double centerFreq, double bandwidth)
{
    m_sampleRate = sampleRate;       // 使用传入的采样率
    m_centerFreq = centerFreq;       // 使用传入的中心频率
    m_bandwidth = bandwidth;         // 使用传入的带宽
}

QVector<double> NewWindow::butterworthBandpassFilter(const QVector<double> &data)
{
    if (data.isEmpty() || m_sampleRate <= 0)
        return data;

    // 1. 获取并校验滤波上下限
    double lowCut = label5Size;
    double highCut = label7Size;
    const double nyquist = m_sampleRate / 2.0;
    lowCut = qMax(1.0, lowCut);
    highCut = qMin(nyquist - 1.0, highCut);
    if (lowCut > highCut)
        qSwap(lowCut, highCut);

    // 2. 计算核心参数
    m_centerFreq = (lowCut + highCut) / 2.0;
    m_bandwidth = highCut - lowCut;
    double w0 = 2 * M_PI * m_centerFreq / m_sampleRate;  // 归一化中心角频率
    double bw = 2 * M_PI * m_bandwidth / m_sampleRate;   // 归一化带宽
    double Q = w0 / bw;                                  // 品质因数

    // 3. 8阶滤波器需要4个二阶节（基于巴特沃斯8阶极点分布特性）
    // 二阶节1（第一对极点）
    double alpha1 = sin(w0) / (2 * Q * 0.3827);  // 8阶第一级系数（基于√2/2≈0.707的衍生调整）
    double b0_1 = alpha1;
    double b1_1 = 0;
    double b2_1 = -alpha1;
    double a0_1 = 1 + alpha1;
    double a1_1 = -2 * cos(w0);
    double a2_1 = 1 - alpha1;
    // 归一化
    b0_1 /= a0_1; b1_1 /= a0_1; b2_1 /= a0_1;
    a1_1 /= a0_1; a2_1 /= a0_1;

    // 二阶节2（第二对极点）
    double alpha2 = sin(w0) / (2 * Q * 0.9239);  // 8阶第二级系数
    double b0_2 = alpha2;
    double b1_2 = 0;
    double b2_2 = -alpha2;
    double a0_2 = 1 + alpha2;
    double a1_2 = -2 * cos(w0);
    double a2_2 = 1 - alpha2;
    // 归一化
    b0_2 /= a0_2; b1_2 /= a0_2; b2_2 /= a0_2;
    a1_2 /= a0_2; a2_2 /= a0_2;

    // 二阶节3（第三对极点，与第二对对称）
    double alpha3 = sin(w0) / (2 * Q * 0.9239);
    double b0_3 = alpha3;
    double b1_3 = 0;
    double b2_3 = -alpha3;
    double a0_3 = 1 + alpha3;
    double a1_3 = -2 * cos(w0);
    double a2_3 = 1 - alpha3;
    // 归一化
    b0_3 /= a0_3; b1_3 /= a0_3; b2_3 /= a0_3;
    a1_3 /= a0_3; a2_3 /= a0_3;

    // 二阶节4（第四对极点，与第一对对称）
    double alpha4 = sin(w0) / (2 * Q * 0.3827);
    double b0_4 = alpha4;
    double b1_4 = 0;
    double b2_4 = -alpha4;
    double a0_4 = 1 + alpha4;
    double a1_4 = -2 * cos(w0);
    double a2_4 = 1 - alpha4;
    // 归一化
    b0_4 /= a0_4; b1_4 /= a0_4; b2_4 /= a0_4;
    a1_4 /= a0_4; a2_4 /= a0_4;

    // 4. 级联滤波（4个二阶节依次处理）
    QVector<double> temp1 = applySecondOrderSection(data, b0_1, b1_1, b2_1, a1_1, a2_1);
    QVector<double> temp2 = applySecondOrderSection(temp1, b0_2, b1_2, b2_2, a1_2, a2_2);
    QVector<double> temp3 = applySecondOrderSection(temp2, b0_3, b1_3, b2_3, a1_3, a2_3);
    QVector<double> filteredData = applySecondOrderSection(temp3, b0_4, b1_4, b2_4, a1_4, a2_4);

    return filteredData;
}

// 辅助函数保持不变（单个二阶节处理）
QVector<double> NewWindow::applySecondOrderSection(const QVector<double>& input,
                                                  double b0, double b1, double b2,
                                                  double a1, double a2)
{
    QVector<double> output(input.size());
    if (input.isEmpty()) return output;

    // 边界处理
    if (input.size() >= 1) output[0] = b0 * input[0];
    if (input.size() >= 2) output[1] = b0 * input[1] + b1 * input[0] - a1 * output[0];

    // 递归滤波
    for (int i = 2; i < input.size(); ++i) {
        output[i] = b0 * input[i] + b1 * input[i-1] + b2 * input[i-2]
                  - a1 * output[i-1] - a2 * output[i-2];
    }

    return output;
}


void NewWindow::updateWaveform(const QVector<double> &waveData)
{


}



void NewWindow::onAddClicked()
{
    if (radioButton->isChecked()) {
        label5Size += 100;
        label5Size = qMin(label5Size, label7Size);  // 下限不能超过上限
    } else if (radioButton_2->isChecked()) {
        label7Size += 100;
        label7Size = qMin(label7Size, (int)(m_sampleRate / 2 - 1));  // 不超过奈奎斯特频率
    }
    updateLabelSize();
}

void NewWindow::onSubtractClicked()
{
    if (radioButton->isChecked()) {
        label5Size = qMax(1, label5Size - 100);  // 下限不低于1
    } else if (radioButton_2->isChecked()) {
        label7Size -= 100;
        label7Size = qMax(label7Size, label5Size);  // 上限不能低于下限
    }
    updateLabelSize();
}
void NewWindow::updateLabelSize()
{
    if (radioButton->isChecked()) {
        // 只显示label_5的当前数值，不修改字体大小
        label_5->setText(QString("%1").arg(label5Size));
    } else if (radioButton_2->isChecked()) {
        // 只显示label_7的当前数值，不修改字体大小
        label_7->setText(QString("%1").arg(label7Size));
    }
}

void NewWindow::setLeftChannelLevel(qreal level)
{


    // 将强度归一化到0-100范围（与之前逻辑一致）
    int displayLevel = qBound(0, static_cast<int>(qRound(level)), 100);


}


void NewWindow::on_pushButton_4_clicked()
{
      m_recording = true;
      m_recordCount = 0;
      m_recordSum = 0.0;
}

void NewWindow::on_pushButton_5_clicked()
{
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
